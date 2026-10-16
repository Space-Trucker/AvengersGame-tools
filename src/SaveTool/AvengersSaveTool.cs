// Avengers Save Tool - Marvel's Avengers (PC, Steam 2.8.2) save helper.
//   * Unlock the war table + all heroes on a save (skips the Reassemble campaign)
//   * Import a save shared by another player (re-encrypted for your Steam account)
//   * Back up / restore your saves
// Every change is preceded by a backup of the account's save files (.dat). The game must be closed.
//
// Save file format (found by the author):
//   file = RC4(zlib(plain)) + CRC-64 of the zlib stream (8 bytes, little-endian)
//   RC4 key = first 16 bytes of SHA-1(25 E2 E3 53 51 54 76 9D || SteamID64 little-endian)
//   CRC-64: ECMA-182 polynomial table, reflected update, init 0, result inverted
//   plain = 16 MB pool; blocks from 0x1AF49C (bytes used at 0x1AF498), block+8 = type | size_dwords << 8
//   block type 4 holds {u32 key, u32 value} pairs; key 0x7C766BB1 is the story stage,
//   0x06B870E3 = war table and heroes unlocked.
// Build: csc /target:winexe /out:AvengersSaveTool.exe AvengersSaveTool.cs  (.NET Framework 4.x)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Avengers Save Tool")]
[assembly: System.Reflection.AssemblyProduct("Avengers Save Tool")]
[assembly: System.Reflection.AssemblyCompany("Space Trucker")]
[assembly: System.Reflection.AssemblyCopyright("Space Trucker")]
[assembly: System.Reflection.AssemblyVersion("1.0.0.0")]

static class Save
{
    static readonly byte[] Const = { 0x25, 0xE2, 0xE3, 0x53, 0x51, 0x54, 0x76, 0x9D };
    public const uint StageKey = 0x7C766BB1, StageUnlocked = 0x06B870E3;
    static ulong[] crcTable;

    static byte[] Key(ulong steamId)
    {
        byte[] msg = new byte[16];
        Buffer.BlockCopy(Const, 0, msg, 0, 8);
        Buffer.BlockCopy(BitConverter.GetBytes(steamId), 0, msg, 8, 8);
        using (SHA1 sha = SHA1.Create()) return sha.ComputeHash(msg).Take(16).ToArray();
    }

    static byte[] Rc4(byte[] key, byte[] data, int len)
    {
        byte[] s = new byte[256];
        for (int i = 0; i < 256; i++) s[i] = (byte)i;
        for (int i = 0, j = 0; i < 256; i++)
        {
            j = (j + s[i] + key[i % key.Length]) & 255;
            byte t = s[i]; s[i] = s[j]; s[j] = t;
        }
        byte[] o = new byte[len];
        for (int n = 0, i = 0, j = 0; n < len; n++)
        {
            i = (i + 1) & 255; j = (j + s[i]) & 255;
            byte t = s[i]; s[i] = s[j]; s[j] = t;
            o[n] = (byte)(data[n] ^ s[(s[i] + s[j]) & 255]);
        }
        return o;
    }

    public static ulong Crc64(byte[] data)
    {
        if (crcTable == null)
        {
            const ulong poly = 0x42F0E1EBA9EA3693UL;
            crcTable = new ulong[256];
            for (int i = 0; i < 256; i++)
            {
                ulong c = (ulong)i << 56;
                for (int k = 0; k < 8; k++) c = (c >> 63) != 0 ? (c << 1) ^ poly : c << 1;
                crcTable[i] = c;
            }
        }
        ulong r = 0;
        foreach (byte b in data) r = crcTable[(r ^ b) & 255] ^ (r >> 8);
        return ~r;
    }

    public const int SaveSize = 0x1000000;       // plain size of a save1_N / savebk_N file (16 MB)
    const int MaxFile = 64 << 20, MaxPlain = 32 << 20;

    // Reads a file, refusing anything far larger than a save (shared files are untrusted).
    public static byte[] ReadFile(string path)
    {
        if (new FileInfo(path).Length > MaxFile) return null;
        return File.ReadAllBytes(path);
    }

    // Returns the plain save data, or null if the file isn't a save for this SteamID. Output is capped,
    // so a crafted file can't expand into gigabytes.
    public static byte[] Decode(byte[] file, ulong steamId)
    {
        if (file == null || file.Length < 16 || file.Length > MaxFile) return null;
        byte[] z = Rc4(Key(steamId), file, file.Length - 8);
        if (BitConverter.ToUInt64(file, file.Length - 8) != Crc64(z)) return null;
        if (z.Length < 6 || z[0] != 0x78 || ((z[0] << 8) | z[1]) % 31 != 0 || (z[1] & 0x20) != 0) return null;
        try
        {
            using (var src = new MemoryStream(z, 2, z.Length - 2))
            using (var inf = new DeflateStream(src, CompressionMode.Decompress))
            using (var dst = new MemoryStream())
            {
                byte[] buf = new byte[1 << 16];
                int n;
                while ((n = inf.Read(buf, 0, buf.Length)) > 0)
                {
                    if (dst.Length + n > MaxPlain) return null;
                    dst.Write(buf, 0, n);
                }
                return dst.ToArray();
            }
        }
        catch (InvalidDataException) { return null; }
    }

    // Decodes a save1_N / savebk_N file and checks it is a whole save (not a profile or anything else).
    public static byte[] DecodeSave(byte[] file, ulong steamId)
    {
        byte[] p = Decode(file, steamId);
        return p != null && p.Length == SaveSize ? p : null;
    }

    public static byte[] Encode(byte[] plain, ulong steamId)
    {
        byte[] z;
        using (var dst = new MemoryStream())
        {
            dst.WriteByte(0x78); dst.WriteByte(0x9C);
            using (var def = new DeflateStream(dst, CompressionLevel.Optimal, true)) def.Write(plain, 0, plain.Length);
            uint a = 1, b = 0;   // Adler-32
            foreach (byte x in plain) { a = (a + x) % 65521; b = (b + a) % 65521; }
            uint adler = (b << 16) | a;
            dst.WriteByte((byte)(adler >> 24)); dst.WriteByte((byte)(adler >> 16));
            dst.WriteByte((byte)(adler >> 8)); dst.WriteByte((byte)adler);
            z = dst.ToArray();
        }
        byte[] enc = Rc4(Key(steamId), z, z.Length);
        byte[] o = new byte[enc.Length + 8];
        Buffer.BlockCopy(enc, 0, o, 0, enc.Length);
        Buffer.BlockCopy(BitConverter.GetBytes(Crc64(z)), 0, o, enc.Length, 8);
        return o;
    }

    // Offset of the story-stage value, or -1. Every offset is bounds-checked (shared saves are untrusted).
    public static int FindStage(byte[] p)
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
            long blockEnd = Math.Min(o + size, end);
            if ((w & 0xFF) == 4)
            {
                // {u32 key, u32 value} pairs on 8-byte steps from the block start
                for (long i = o + 0x10; i + 8 <= blockEnd; i += 8)
                    if (BitConverter.ToUInt32(p, (int)i) == StageKey) return (int)i + 4;
                return -1;
            }
            o += size;
        }
        return -1;
    }

    public static string StageText(uint v)
    {
        if (v == StageUnlocked) return "war table + heroes unlocked";
        if (v == 0x49481C6F) return "campaign start (locked)";
        if (v == 0x157BC485) return "mid-campaign (locked)";
        return string.Format("other (0x{0:X8})", v);
    }
}

class MainForm : Form
{
    static readonly string Root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Marvel's Avengers");
    static readonly Regex SaveName = new Regex(@"^save1_\d+\.dat$", RegexOptions.IgnoreCase);
    static readonly Regex BackupName = new Regex(@"^savebk_\d+\.dat$", RegexOptions.IgnoreCase);
    ComboBox account = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260 };
    ListView list = new ListView { View = View.Details, FullRowSelect = true, Dock = DockStyle.Fill };
    TextBox log = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Bottom, Height = 120 };

    public MainForm()
    {
        Text = "Avengers Save Tool 1.0 - by Space Trucker";
        Width = 760; Height = 520; StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 9f);
        list.Columns.Add("Save", 110); list.Columns.Add("Modified", 150); list.Columns.Add("Story stage", 300);

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 40, Padding = new Padding(6) };
        top.Controls.Add(new Label { Text = "Account folder:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) });
        top.Controls.Add(account);
        var refresh = new Button { Text = "Refresh", AutoSize = true };
        refresh.Click += delegate { Safe(LoadAccounts); };
        top.Controls.Add(refresh);

        var buttons = new FlowLayoutPanel { Dock = DockStyle.Right, Width = 230, FlowDirection = FlowDirection.TopDown, Padding = new Padding(6) };
        AddButton(buttons, "Unlock war table + heroes", "Sets the story stage of the selected saves (or all) so the war table and all heroes are unlocked.", Unlock);
        AddButton(buttons, "Import a shared save...", "Pick a folder with another player's save files. They are re-encrypted for this account.", Import);
        AddButton(buttons, "Back up now", "Copies this account's save files (.dat) to Documents\\Marvel's Avengers\\SaveTool Backups.", delegate { if (HaveAccount()) Backup("manual"); });
        AddButton(buttons, "Restore a backup...", "Puts a backup made by this tool back in place (the current saves are backed up first).", Restore);
        AddButton(buttons, "Open save folder", "", delegate { if (HaveAccount() && Directory.Exists(AccountDir)) Process.Start("explorer.exe", "\"" + AccountDir + "\""); });

        Controls.Add(list); Controls.Add(buttons); Controls.Add(log); Controls.Add(top);
        account.SelectedIndexChanged += delegate { Safe(LoadSaves); };
        Shown += delegate
        {
            Log("Close the game before changing saves (the game writes its saves when it exits).");
            Log("If you use Steam Cloud and Steam later reports a conflict, choose the files on this PC (local).");
            Safe(LoadAccounts);
        };
    }

    void Safe(Action act)
    {
        try { act(); }
        catch (Exception e) { Log("Error: " + e.Message); MessageBox.Show(this, e.Message, "Avengers Save Tool", MessageBoxButtons.OK, MessageBoxIcon.Error); }
    }

    void AddButton(Control parent, string text, string tip, Action act)
    {
        var b = new Button { Text = text, Width = 210, Height = 34, Margin = new Padding(3, 3, 3, 6) };
        b.Click += delegate { Safe(act); };
        if (tip.Length > 0) new ToolTip().SetToolTip(b, tip);
        parent.Controls.Add(b);
    }

    void Log(string s) { log.AppendText(DateTime.Now.ToString("HH:mm:ss ") + s + Environment.NewLine); }

    string AccountDir { get { return account.SelectedItem == null ? "" : Path.Combine(Root, (string)account.SelectedItem); } }
    ulong SteamId { get { return ulong.Parse((string)account.SelectedItem); } }

    bool HaveAccount()
    {
        if (account.SelectedItem != null) return true;
        Log("No account folder selected.");
        return false;
    }

    static bool IsSteamIdFolder(string name) { return name.Length >= 15 && name.Length <= 20 && name.All(c => c >= '0' && c <= '9'); }

    static bool SamePath(string a, string b)
    {
        return string.Equals(Path.GetFullPath(a).TrimEnd('\\'), Path.GetFullPath(b).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
    }

    // save1_N.dat (and savebk_N.dat) only; Directory.GetFiles patterns also match 8.3 short names
    // (e.g. "save1_1.dat_old"), so the names are checked exactly.
    static List<string> SaveFiles(string dir, bool withBackups)
    {
        return Directory.GetFiles(dir).Where(f => SaveName.IsMatch(Path.GetFileName(f)) ||
                                              (withBackups && BackupName.IsMatch(Path.GetFileName(f))))
                                      .OrderBy(f => f).ToList();
    }

    static List<string> DatFiles(string dir)
    {
        return Directory.GetFiles(dir).Where(f => string.Equals(Path.GetExtension(f), ".dat", StringComparison.OrdinalIgnoreCase)).ToList();
    }

    void LoadAccounts()
    {
        account.Items.Clear();
        list.Items.Clear();
        if (Directory.Exists(Root))
            foreach (string d in Directory.GetDirectories(Root))
                if (IsSteamIdFolder(Path.GetFileName(d))) account.Items.Add(Path.GetFileName(d));
        if (account.Items.Count == 0) { Log("No save folder found in " + Root + ". Start the game once and make a save."); return; }
        account.SelectedIndex = 0;
    }

    void LoadSaves()
    {
        list.Items.Clear();
        if (account.SelectedItem == null || !Directory.Exists(AccountDir)) return;
        foreach (string f in SaveFiles(AccountDir, false))
        {
            string stage;
            try
            {
                byte[] plain = Save.DecodeSave(Save.ReadFile(f), SteamId);
                int at = Save.FindStage(plain);
                stage = plain == null ? "can't read (not a save for this account?)"
                      : at < 0 ? "story stage not found" : Save.StageText(BitConverter.ToUInt32(plain, at));
            }
            catch (Exception e) { stage = "can't read (" + e.Message + ")"; }
            var it = new ListViewItem(new[] { Path.GetFileName(f), File.GetLastWriteTime(f).ToString("yyyy-MM-dd HH:mm"), stage });
            it.Tag = f;
            list.Items.Add(it);
        }
    }

    // Changing saves while the game runs is unsafe: it writes its saves on exit (and autosaves).
    bool GameClosed()
    {
        if (Process.GetProcessesByName("Avengers").Length == 0) return true;
        MessageBox.Show(this, "Please close Marvel's Avengers first.\n\nThe game writes its saves when it exits, which would undo the change.",
                        "Avengers Save Tool", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        return false;
    }

    string Backup(string why)
    {
        string dst = Path.Combine(Root, "SaveTool Backups", DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss-fff") + "_" + why, (string)account.SelectedItem);
        Directory.CreateDirectory(dst);
        foreach (string f in DatFiles(AccountDir)) File.Copy(f, Path.Combine(dst, Path.GetFileName(f)));
        Log("Backup: " + dst);
        return dst;
    }

    // Write via a temporary file and a rename, so an interrupted write never leaves a half-written save.
    static void WriteAtomic(string path, byte[] data)
    {
        string tmp = path + ".tmp";
        File.WriteAllBytes(tmp, data);
        if (File.Exists(path)) File.Replace(tmp, path, null);
        else File.Move(tmp, path);
    }

    void Unlock()
    {
        if (!HaveAccount() || !GameClosed()) return;
        var targets = list.SelectedItems.Count > 0 ? list.SelectedItems.Cast<ListViewItem>().Select(i => (string)i.Tag).ToList()
                                                   : SaveFiles(AccountDir, false);
        if (targets.Count == 0) { Log("No saves to change."); return; }
        Backup("before-unlock");
        int changed = 0;
        foreach (string f in targets)
        {
            byte[] plain = Save.DecodeSave(Save.ReadFile(f), SteamId);
            int at = Save.FindStage(plain);
            if (at < 0) { Log(Path.GetFileName(f) + ": skipped (can't read it or no story stage)"); continue; }
            if (BitConverter.ToUInt32(plain, at) == Save.StageUnlocked) { Log(Path.GetFileName(f) + ": already unlocked"); continue; }
            Buffer.BlockCopy(BitConverter.GetBytes(Save.StageUnlocked), 0, plain, at, 4);
            byte[] data = Save.Encode(plain, SteamId);
            byte[] check = Save.DecodeSave(data, SteamId);
            if (check == null || Save.FindStage(check) != at || BitConverter.ToUInt32(check, at) != Save.StageUnlocked)
                throw new Exception("Verification failed for " + Path.GetFileName(f) + "; nothing was written.");
            WriteAtomic(f, data);
            changed++;
            Log(Path.GetFileName(f) + ": war table + heroes unlocked");
        }
        LoadSaves();
        if (changed > 0) MessageBox.Show(this, "Done. Start the game and load the save.", "Avengers Save Tool");
    }

    void Import()
    {
        if (!HaveAccount() || !GameClosed()) return;
        string src;
        using (var dlg = new FolderBrowserDialog { Description = "Select the folder that contains the shared save1_*.dat files" })
        {
            if (dlg.ShowDialog(this) != DialogResult.OK) return;
            src = dlg.SelectedPath;
        }
        if (SamePath(src, AccountDir)) { Log("That is this account's own save folder; pick the folder with the shared files."); return; }
        var files = SaveFiles(src, true);
        if (files.Count == 0) { Log("No save1_*.dat files in " + src); return; }
        var raw = files.Select(f => Save.ReadFile(f)).ToList();
        // Find the SteamID the shared files were made with: the folder name, else ask.
        ulong srcId = 0;
        string name = Path.GetFileName(src.TrimEnd('\\'));
        if (IsSteamIdFolder(name) && raw.Any(d => Save.DecodeSave(d, ulong.Parse(name)) != null)) srcId = ulong.Parse(name);
        while (srcId == 0)
        {
            string s = Prompt("The shared save's owner Steam ID (the 17-digit name of the folder it came from):");
            if (s == null) return;
            s = s.Trim();
            if (IsSteamIdFolder(s) && raw.Any(d => Save.DecodeSave(d, ulong.Parse(s)) != null)) srcId = ulong.Parse(s);
            else MessageBox.Show(this, "These files can't be opened with that Steam ID.", "Avengers Save Tool");
        }
        var converted = new List<KeyValuePair<string, byte[]>>();
        for (int i = 0; i < files.Count; i++)
        {
            byte[] plain = Save.DecodeSave(raw[i], srcId);
            if (plain == null) { Log(Path.GetFileName(files[i]) + ": skipped (damaged or from another account)"); continue; }
            converted.Add(new KeyValuePair<string, byte[]>(Path.GetFileName(files[i]), Save.Encode(plain, SteamId)));
        }
        if (converted.Count == 0) { Log("Nothing could be imported."); return; }
        if (MessageBox.Show(this, string.Format("Replace this account's saves with {0} imported file(s)?\nYour current saves are backed up first; your settings (profile1.dat) are kept.",
                converted.Count), "Avengers Save Tool", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        Backup("before-import");
        foreach (string f in SaveFiles(AccountDir, true)) File.Delete(f);
        foreach (var kv in converted) WriteAtomic(Path.Combine(AccountDir, kv.Key), kv.Value);
        Log(string.Format("Imported {0} file(s) from Steam ID {1}.", converted.Count, srcId));
        LoadSaves();
    }

    void Restore()
    {
        if (!HaveAccount() || !GameClosed()) return;
        string backups = Path.Combine(Root, "SaveTool Backups");
        string sel;
        using (var dlg = new FolderBrowserDialog { Description = "Select a backup folder (the one named with your Steam ID inside a dated folder)", SelectedPath = Directory.Exists(backups) ? backups : Root })
        {
            if (dlg.ShowDialog(this) != DialogResult.OK) return;
            sel = dlg.SelectedPath;
        }
        if (SamePath(sel, AccountDir)) { Log("That is the live save folder, not a backup."); return; }
        // Read everything into memory first; nothing is deleted until all of it has been read and checked.
        var files = DatFiles(sel).Where(f => SaveName.IsMatch(Path.GetFileName(f)) || BackupName.IsMatch(Path.GetFileName(f)) ||
                                             string.Equals(Path.GetFileName(f), "profile1.dat", StringComparison.OrdinalIgnoreCase)).ToList();
        var saves = files.Where(f => SaveName.IsMatch(Path.GetFileName(f))).ToList();
        if (saves.Count == 0) { Log("No save1_*.dat files in that folder."); return; }
        var data = files.ToDictionary(f => Path.GetFileName(f), f => Save.ReadFile(f));
        int readable = saves.Count(f => Save.DecodeSave(data[Path.GetFileName(f)], SteamId) != null);
        if (readable == 0) { MessageBox.Show(this, "None of these saves belong to this account, so the game couldn't load them.\nUse \"Import a shared save\" for saves from another account.", "Avengers Save Tool"); return; }
        if (MessageBox.Show(this, string.Format("Restore {0} file(s) from\n{1}?\n({2} of {3} saves are readable for this account.)\nThe current saves are backed up first.",
                files.Count, sel, readable, saves.Count), "Avengers Save Tool", MessageBoxButtons.YesNo) != DialogResult.Yes) return;
        Backup("before-restore");
        foreach (string f in SaveFiles(AccountDir, true)) File.Delete(f);
        foreach (var kv in data) WriteAtomic(Path.Combine(AccountDir, kv.Key), kv.Value);
        Log("Restored from " + sel);
        LoadSaves();
    }

    string Prompt(string question)
    {
        using (var f = new Form { Text = "Avengers Save Tool", Width = 460, Height = 160, FormBorderStyle = FormBorderStyle.FixedDialog, StartPosition = FormStartPosition.CenterParent, MinimizeBox = false, MaximizeBox = false })
        {
            var l = new Label { Text = question, Left = 10, Top = 10, Width = 420, Height = 34 };
            var t = new TextBox { Left = 10, Top = 48, Width = 420 };
            var ok = new Button { Text = "OK", Left = 270, Top = 80, Width = 75, DialogResult = DialogResult.OK };
            var cancel = new Button { Text = "Cancel", Left = 355, Top = 80, Width = 75, DialogResult = DialogResult.Cancel };
            f.Controls.AddRange(new Control[] { l, t, ok, cancel });
            f.AcceptButton = ok; f.CancelButton = cancel;
            return f.ShowDialog(this) == DialogResult.OK ? t.Text : null;
        }
    }

    [STAThread]
    static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Application.ThreadException += (s, e) => MessageBox.Show(e.Exception.Message, "Avengers Save Tool", MessageBoxButtons.OK, MessageBoxIcon.Error);
        Application.Run(new MainForm());
    }
}
