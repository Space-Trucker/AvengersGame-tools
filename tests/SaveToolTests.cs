using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Threading;

static class SaveToolTests
{
    static int checks, failed;

    static void Check(bool ok, string what)
    {
        checks++;
        if (ok) return;
        failed++;
        Console.WriteLine("  FAIL " + what);
    }

    static void Throws<T>(Action act, string what) where T : Exception
    {
        try { act(); Check(false, what + ": no exception"); }
        catch (T) { Check(true, what); }
        catch (Exception e) { Check(false, what + ": " + e.GetType().Name + " " + e.Message); }
    }

    static int Main(string[] args)
    {
        if (args.Length == 1 && args[0] == "--wait") { Thread.Sleep(60000); return 0; }
        string tmp = Path.Combine(Path.GetTempPath(), "SaveToolTests-" + Process.GetCurrentProcess().Id);
        Directory.CreateDirectory(tmp);
        try
        {
            TestFormat();
            TestFindStage();
            TestFolderNames(tmp);
            TestWrites(tmp);
            TestReplace(tmp);
            TestGameRunning(tmp);
            if (args.Length >= 2) TestRealSaves(ulong.Parse(args[0]), args.Skip(1).ToArray(), tmp);
        }
        finally
        {
            foreach (string f in Directory.GetFiles(tmp, "*", SearchOption.AllDirectories)) File.SetAttributes(f, FileAttributes.Normal);
            Directory.Delete(tmp, true);
        }
        Console.WriteLine("SaveToolTests: {0} checks, {1} failed", checks, failed);
        return failed == 0 ? 0 : 1;
    }

    const int Start = 0x1AF49C;
    const ulong IdA = 11111111111111111UL, IdB = 22222222222222222UL;

    static byte[] Plain(params uint[][] blocks)
    {
        byte[] p = new byte[Save.SaveSize];
        for (int i = 0; i < p.Length; i++) p[i] = (byte)(i % 251);
        int o = Start;
        foreach (uint[] b in blocks)
        {
            int pairs = (b.Length - 1) / 2, size = 0x10 + pairs * 8;
            Array.Clear(p, o, size);
            BitConverter.GetBytes(b[0] | ((uint)(size / 4) << 8)).CopyTo(p, o + 8);
            for (int k = 0; k < pairs; k++)
            {
                BitConverter.GetBytes(b[1 + 2 * k]).CopyTo(p, o + 0x10 + 8 * k);
                BitConverter.GetBytes(b[2 + 2 * k]).CopyTo(p, o + 0x14 + 8 * k);
            }
            o += size;
        }
        BitConverter.GetBytes((uint)(o - Start)).CopyTo(p, Start - 4);
        return p;
    }

    static void TestFormat()
    {
        byte[] plain = Plain(new uint[] { 4, 1, 2, Save.StageKey, 0x49481C6F });
        byte[] a = Save.Encode(plain, IdA);
        Check(Save.SameBytes(Save.DecodeSave(a, IdA), plain), "a save decodes back to its plain bytes");
        Check(Save.DecodeSave(a, IdB) == null, "another account's key does not open it");
        byte[] ac = Save.EncodeChecked(plain, IdA);
        Check(ac != null && Save.SameBytes(ac, a), "EncodeChecked gives the encoded file when it decodes back");
        Check(Save.EncodeChecked(new byte[Save.SaveSize - 1], IdA) == null, "EncodeChecked refuses what would not decode as a whole save");
        byte[] bad = (byte[])a.Clone();
        bad[bad.Length / 2] ^= 0x40;
        Check(Save.DecodeSave(bad, IdA) == null, "a changed byte fails the CRC");
        Check(Save.Decode(null, IdA) == null && Save.Decode(new byte[15], IdA) == null, "too short or none: null");
        Check(Save.Decode(new byte[Save.MaxFile + 1], IdA) == null, "over 64 MB: null");
        Check(Save.Decode(Save.Encode(new byte[Save.MaxPlain + 1], IdA), IdA) == null, "decoding past 32 MB: null");
        Check(Save.Decode(Save.Encode(new byte[1000], IdA), IdA) != null && Save.DecodeSave(Save.Encode(new byte[1000], IdA), IdA) == null,
              "a profile-size file decodes, but is not a save");
        Check(Save.SameBytes(new byte[0], new byte[0]) && !Save.SameBytes(new byte[1], new byte[2]) && !Save.SameBytes(null, new byte[0]),
              "SameBytes");
    }

    static void TestFindStage()
    {
        int first = Start + 0x10 + 8 + 4;
        Check(Save.FindStage(Plain(new uint[] { 4, 1, 2, Save.StageKey, 7 })) == first, "the key in the first type-4 block");
        byte[] second = Plain(new uint[] { 2, Save.StageKey, 9 }, new uint[] { 4, 5, 6 }, new uint[] { 4, 8, 9, Save.StageKey, 7 });
        int at = Start + 0x18 + 0x18 + 0x10 + 8 + 4;
        Check(Save.FindStage(second) == at, "the key in a later type-4 block (the 1.0 search stopped at the first)");
        Check(OldFindStage(second) == -1, "the 1.0 search does not find it");
        Check(Save.FindStage(Plain(new uint[] { 2, Save.StageKey, 7 })) == -1, "a key outside type-4 blocks is not the stage");
        Check(Save.FindStage(Plain(new uint[] { 4, 1, 2 })) == -1, "no key: -1");
        Check(Save.FindStage(null) == -1 && Save.FindStage(new byte[Start - 1]) == -1, "too short or none: -1");
        byte[] past = Plain(new uint[] { 4, 1, 2, Save.StageKey, 7 });
        BitConverter.GetBytes((uint)(past.Length)).CopyTo(past, Start - 4);
        Check(Save.FindStage(past) == -1, "used bytes past the end of the data: -1");
        byte[] tiny = Plain(new uint[] { 4, Save.StageKey, 7 });
        BitConverter.GetBytes(4u | (2u << 8)).CopyTo(tiny, Start + 8);
        Check(Save.FindStage(tiny) == -1, "a block smaller than its header: -1");
        byte[] cut = Plain(new uint[] { 4, 1, 2, Save.StageKey, 7 });
        BitConverter.GetBytes((uint)0x18).CopyTo(cut, Start - 4);
        Check(Save.FindStage(cut) == -1, "a key past the used bytes is not read");
    }

    static int OldFindStage(byte[] p)
    {
        const int start = 0x1AF49C;
        if (p == null || p.Length < start) return -1;
        long end = start + (long)BitConverter.ToUInt32(p, start - 4);
        if (end > p.Length) return -1;
        long o = start;
        while (o + 12 <= end)
        {
            uint w = BitConverter.ToUInt32(p, (int)o + 8);
            long size = (w >> 6) & 0x3FFFFFC;
            if (size < 12) return -1;
            if ((w & 0xFF) == 4)
            {
                long blockEnd = Math.Min(o + size, end);
                for (long i = o + 0x10; i + 8 <= blockEnd; i += 8)
                    if (BitConverter.ToUInt32(p, (int)i) == Save.StageKey) return (int)i + 4;
                return -1;
            }
            o += size;
        }
        return -1;
    }

    static void TestFolderNames(string tmp)
    {
        Check(SaveFolder.IsSteamIdFolder("12345678901234567"), "a 17-digit id");
        Check(SaveFolder.IsSteamIdFolder("18446744073709551615"), "the largest 64-bit id");
        Check(!SaveFolder.IsSteamIdFolder("18446744073709551616"), "one over the largest: not an id (1.0 threw later)");
        Check(!SaveFolder.IsSteamIdFolder("99999999999999999999"), "20 nines: not an id");
        Check(SaveFolder.IsSteamIdFolder("123456789012345") && !SaveFolder.IsSteamIdFolder("12345678901234"), "15 digits at least");
        Check(!SaveFolder.IsSteamIdFolder("1234567890123456a") && !SaveFolder.IsSteamIdFolder("+1234567890123456") &&
              !SaveFolder.IsSteamIdFolder(""), "digits only");
        string dir = Path.Combine(tmp, "names");
        Directory.CreateDirectory(dir);
        foreach (string n in new[] { "save1_1.dat", "save1_12.dat", "SAVE1_2.DAT", "savebk_1.dat", "save1_x.dat", "save1_1.dat_old",
                                     "profile1.dat", "save1_1.dat.savetool-1.new", "notes.txt" })
            File.WriteAllText(Path.Combine(dir, n), n);
        var saves = SaveFolder.SaveFiles(dir, false).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToArray();
        Check(saves.SequenceEqual(new[] { "SAVE1_2.DAT", "save1_1.dat", "save1_12.dat" }), "SaveFiles: " + string.Join(",", saves));
        var all = SaveFolder.SaveFiles(dir, true).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToArray();
        Check(all.SequenceEqual(new[] { "SAVE1_2.DAT", "save1_1.dat", "save1_12.dat", "savebk_1.dat" }), "SaveFiles with backups: " + string.Join(",", all));
        Check(SaveFolder.DatFiles(dir).Count == 6, "DatFiles: every .dat");
    }

    static void TestWrites(string tmp)
    {
        string f = Path.Combine(tmp, "w.dat");
        Throws<ArgumentNullException>(() => SaveFolder.WriteDurable(f, null), "WriteDurable refuses no data");
        byte[] one = Enumerable.Range(0, 70000).Select(i => (byte)i).ToArray(), two = new byte[] { 9, 8, 7 };
        SaveFolder.WriteDurable(f, one);
        Check(Save.SameBytes(File.ReadAllBytes(f), one), "WriteDurable writes the bytes");
        string g = Path.Combine(tmp, "atomic.dat");
        SaveFolder.WriteAtomic(g, one);
        Check(Save.SameBytes(File.ReadAllBytes(g), one), "WriteAtomic: a new file");
        SaveFolder.WriteAtomic(g, two);
        Check(Save.SameBytes(File.ReadAllBytes(g), two), "WriteAtomic: replaces the file");
        Check(!File.Exists(g + ".tmp"), "WriteAtomic leaves no temporary file");
    }

    static string Sha(byte[] b) { using (var s = SHA256.Create()) return BitConverter.ToString(s.ComputeHash(b)); }

    static SortedDictionary<string, string> Snapshot(string dir)
    {
        var d = new SortedDictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (string f in Directory.GetFiles(dir)) d[Path.GetFileName(f)] = Sha(File.ReadAllBytes(f));
        foreach (string f in Directory.GetDirectories(dir)) d[Path.GetFileName(f)] = "dir";
        return d;
    }

    static bool Same(SortedDictionary<string, string> a, SortedDictionary<string, string> b)
    {
        return a.Count == b.Count && a.All(kv => b.ContainsKey(kv.Key) && b[kv.Key] == kv.Value);
    }

    static byte[] Bytes(string s) { return System.Text.Encoding.ASCII.GetBytes(s); }

    static string Account(string tmp, string name)
    {
        string dir = Path.Combine(tmp, name);
        Directory.CreateDirectory(dir);
        File.WriteAllBytes(Path.Combine(dir, "save1_1.dat"), Bytes("old 1"));
        File.WriteAllBytes(Path.Combine(dir, "save1_2.dat"), Bytes("old 2"));
        File.WriteAllBytes(Path.Combine(dir, "savebk_1.dat"), Bytes("old bk 1"));
        File.WriteAllBytes(Path.Combine(dir, "profile1.dat"), Bytes("profile"));
        File.WriteAllBytes(Path.Combine(dir, "notes.txt"), Bytes("notes"));
        return dir;
    }

    static List<KeyValuePair<string, byte[]>> Files(params object[] nameData)
    {
        var l = new List<KeyValuePair<string, byte[]>>();
        for (int i = 0; i < nameData.Length; i += 2) l.Add(new KeyValuePair<string, byte[]>((string)nameData[i], (byte[])nameData[i + 1]));
        return l;
    }

    static string Text(string dir, string name) { return File.ReadAllText(Path.Combine(dir, name)); }
    static bool NoLeftovers(string dir) { return !Directory.GetFiles(dir).Any(f => f.Contains(".savetool-")); }

    static void TestReplace(string tmp)
    {
        string dir = Account(tmp, "import");
        var left = SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1"), "savebk_1.dat", Bytes("new bk 1")));
        Check(left.Count == 0, "import: nothing left to delete");
        Check(Text(dir, "save1_1.dat") == "new 1" && Text(dir, "savebk_1.dat") == "new bk 1", "import: the new files");
        Check(!File.Exists(Path.Combine(dir, "save1_2.dat")), "import: a save not in the import is gone (as 1.0)");
        Check(Text(dir, "profile1.dat") == "profile" && Text(dir, "notes.txt") == "notes", "import: profile and other files kept");
        Check(NoLeftovers(dir), "import: no temporary files");

        dir = Account(tmp, "restore");
        SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("r 1"), "save1_3.dat", Bytes("r 3"), "profile1.dat", Bytes("r profile")));
        var names = Directory.GetFiles(dir).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToArray();
        Check(names.SequenceEqual(new[] { "notes.txt", "profile1.dat", "save1_1.dat", "save1_3.dat" }), "restore: " + string.Join(",", names));
        Check(Text(dir, "save1_1.dat") == "r 1" && Text(dir, "save1_3.dat") == "r 3" && Text(dir, "profile1.dat") == "r profile",
              "restore: the restored files");

        dir = Path.Combine(tmp, "case");
        Directory.CreateDirectory(dir);
        File.WriteAllBytes(Path.Combine(dir, "Save1_1.dat"), Bytes("old"));
        File.WriteAllBytes(Path.Combine(dir, "Profile1.dat"), Bytes("old profile"));
        SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new"), "profile1.dat", Bytes("new profile")));
        Check(Directory.GetFiles(dir).Length == 2 && Text(dir, "save1_1.dat") == "new" && Text(dir, "profile1.dat") == "new profile",
              "case: one file each, the new content");

        dir = Account(tmp, "nodata");
        var before = Snapshot(dir);
        Throws<IOException>(() => SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1"), "save1_2.dat", null)), "no data: refused");
        Check(Same(Snapshot(dir), before) && NoLeftovers(dir), "no data: the folder as it was");

        dir = Account(tmp, "nowrite");
        before = Snapshot(dir);
        Throws<DirectoryNotFoundException>(() => SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1"), Path.Combine("missing", "save1_2.dat"), Bytes("x"))),
                                           "a write fails: refused");
        Check(Same(Snapshot(dir), before) && NoLeftovers(dir), "a write fails: the folder as it was");

        dir = Account(tmp, "held");
        before = Snapshot(dir);
        using (new FileStream(Path.Combine(dir, "save1_2.dat"), FileMode.Open, FileAccess.Read, FileShare.Read))
            Throws<IOException>(() => SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1"), "save1_2.dat", Bytes("new 2"))), "held open: refused");
        Check(Same(Snapshot(dir), before) && NoLeftovers(dir), "held open: the folder as it was");

        dir = Account(tmp, "commit");
        Directory.CreateDirectory(Path.Combine(dir, "save1_9.dat"));
        before = Snapshot(dir);
        Throws<IOException>(() => SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1"), "save1_2.dat", Bytes("new 2"), "save1_9.dat", Bytes("new 9"))),
                            "a rename fails: refused");
        Check(Same(Snapshot(dir), before) && NoLeftovers(dir), "a rename fails: the folder as it was");

        dir = Account(tmp, "readonly");
        File.SetAttributes(Path.Combine(dir, "savebk_1.dat"), FileAttributes.ReadOnly);
        left = SaveFolder.ReplaceSaves(dir, Files("save1_1.dat", Bytes("new 1")));
        Check(left.Count == 1 && Path.GetFileName(left[0]).StartsWith("savebk_1.dat.savetool-") && left[0].EndsWith(".old"),
              "read-only old file: reported");
        Check(Text(dir, "save1_1.dat") == "new 1" && !File.Exists(Path.Combine(dir, "savebk_1.dat")) && !File.Exists(Path.Combine(dir, "save1_2.dat")),
              "read-only old file: the change is complete");
    }

    static void TestGameRunning(string tmp)
    {
        Check(!SaveFolder.GameRunning(), "no Avengers.exe runs before the test (close the game to run the tests)");
        string exe = Path.Combine(tmp, "Avengers.exe");
        File.Copy(Assembly.GetExecutingAssembly().Location, exe);
        using (var p = Process.Start(new ProcessStartInfo(exe, "--wait") { UseShellExecute = false, CreateNoWindow = true }))
        {
            bool seen = false;
            for (int i = 0; i < 50 && !seen; i++) { seen = SaveFolder.GameRunning(); if (!seen) Thread.Sleep(100); }
            Check(seen, "a process named Avengers.exe is seen");
            p.Kill();
            p.WaitForExit();
        }
        bool gone = false;
        for (int i = 0; i < 50 && !gone; i++) { gone = !SaveFolder.GameRunning(); if (!gone) Thread.Sleep(100); }
        Check(gone, "and no longer once it has ended");
    }

    static void TestRealSaves(ulong id, string[] folders, string tmp)
    {
        string copies = Path.Combine(tmp, "real");
        Directory.CreateDirectory(copies);
        int n = 0;
        foreach (string folder in folders)
        {
            foreach (string f in SaveFolder.SaveFiles(folder, true))
            {
                string copy = Path.Combine(copies, (n++) + "_" + Path.GetFileName(f));
                File.Copy(f, copy);
                string what = folder + "\\" + Path.GetFileName(f);
                byte[] plain = Save.DecodeSave(Save.ReadFile(copy), id);
                Check(plain != null, what + ": decodes");
                if (plain == null) continue;
                int at = Save.FindStage(plain);
                Check(at > 0 && at == OldFindStage(plain), what + ": the story stage where 1.0 found it (" + at + ")");
                byte[] other = Save.EncodeChecked(plain, IdB);
                Check(other != null && Save.SameBytes(Save.DecodeSave(other, IdB), plain), what + ": converted to another id");
                byte[] back = other == null ? null : Save.EncodeChecked(Save.DecodeSave(other, IdB), id);
                Check(back != null && Save.SameBytes(Save.DecodeSave(back, id), plain), what + ": and back");
            }
        }
        Console.WriteLine("real saves checked: {0}", n);
    }
}
