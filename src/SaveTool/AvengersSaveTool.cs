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

    public const int SaveSize = 0x1000000;
    public const int MaxFile = 64 << 20, MaxPlain = 32 << 20;

    public static byte[] ReadFile(string path)
    {
        if (new FileInfo(path).Length > MaxFile) return null;
        return File.ReadAllBytes(path);
    }

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
            uint a = 1, b = 0;
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

    public static byte[] EncodeChecked(byte[] plain, ulong steamId)
    {
        byte[] data = Encode(plain, steamId);
        byte[] back = DecodeSave(data, steamId);
        return back != null && SameBytes(back, plain) ? data : null;
    }

    public static bool SameBytes(byte[] a, byte[] b)
    {
        if (a == null || b == null || a.Length != b.Length) return false;
        for (int i = 0; i < a.Length; i++) if (a[i] != b[i]) return false;
        return true;
    }

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
                for (long i = o + 0x10; i + 8 <= blockEnd; i += 8)
                    if (BitConverter.ToUInt32(p, (int)i) == StageKey) return (int)i + 4;
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

static class SaveFolder
{
    public static readonly Regex SaveName = new Regex(@"^save1_\d+\.dat$", RegexOptions.IgnoreCase);
    public static readonly Regex BackupName = new Regex(@"^savebk_\d+\.dat$", RegexOptions.IgnoreCase);

    public static bool IsSteamIdFolder(string name)
    {
        ulong id;
        return name.Length >= 15 && name.Length <= 20 && name.All(c => c >= '0' && c <= '9') && ulong.TryParse(name, out id);
    }

    public static List<string> SaveFiles(string dir, bool withBackups)
    {
        return Directory.GetFiles(dir).Where(f => SaveName.IsMatch(Path.GetFileName(f)) ||
                                              (withBackups && BackupName.IsMatch(Path.GetFileName(f))))
                                      .OrderBy(f => f).ToList();
    }

    public static List<string> DatFiles(string dir)
    {
        return Directory.GetFiles(dir).Where(f => string.Equals(Path.GetExtension(f), ".dat", StringComparison.OrdinalIgnoreCase)).ToList();
    }

    public static bool GameRunning() { return Process.GetProcessesByName("Avengers").Length > 0; }

    public static void WriteDurable(string path, byte[] data)
    {
        if (data == null) throw new ArgumentNullException("data");
        using (var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16, FileOptions.WriteThrough))
        {
            fs.Write(data, 0, data.Length);
            fs.Flush(true);
        }
    }

    public static void WriteAtomic(string path, byte[] data)
    {
        string tmp = path + ".tmp";
        WriteDurable(tmp, data);
        if (File.Exists(path)) File.Replace(tmp, path, null);
        else File.Move(tmp, path);
    }

    public static List<string> ReplaceSaves(string dir, IList<KeyValuePair<string, byte[]>> files)
    {
        string tag = ".savetool-" + DateTime.Now.ToString("yyyyMMddHHmmssfff");
        var staged = new List<KeyValuePair<string, string>>();
        var aside = new List<KeyValuePair<string, string>>();
        var placed = new List<string>();
        try
        {
            foreach (var kv in files)
            {
                if (kv.Value == null) throw new IOException(kv.Key + " has no data");
                string final = Path.Combine(dir, kv.Key), tmp = final + tag + ".new";
                staged.Add(new KeyValuePair<string, string>(tmp, final));
                WriteDurable(tmp, kv.Value);
                if (!Save.SameBytes(File.ReadAllBytes(tmp), kv.Value)) throw new IOException(kv.Key + " did not read back as written");
            }
            var old = SaveFiles(dir, true);
            foreach (var kv in staged)
                if (File.Exists(kv.Value) && !old.Any(o => string.Equals(o, kv.Value, StringComparison.OrdinalIgnoreCase))) old.Add(kv.Value);
            foreach (string f in old)
            {
                string moved = f + tag + ".old";
                File.Move(f, moved);
                aside.Add(new KeyValuePair<string, string>(moved, f));
            }
            foreach (var kv in staged)
            {
                File.Move(kv.Key, kv.Value);
                placed.Add(kv.Value);
            }
        }
        catch
        {
            foreach (string f in placed) TryDelete(f);
            foreach (var kv in aside) { try { File.Move(kv.Key, kv.Value); } catch { } }
            foreach (var kv in staged) TryDelete(kv.Key);
            throw;
        }
        var left = new List<string>();
        foreach (var kv in aside) if (!TryDelete(kv.Key)) left.Add(kv.Key);
        return left;
    }

    static bool TryDelete(string path)
    {
        try { if (File.Exists(path)) File.Delete(path); return true; }
        catch { return false; }
    }
}

class MainForm : Form
{
    static readonly string Root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Marvel's Avengers");
    ComboBox account = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260 };
    ListView list = new ListView { View = View.Details, FullRowSelect = true, Dock = DockStyle.Fill };
    TextBox log = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Bottom, Height = 120 };

    public MainForm()
    {
        Text = "Avengers Save Tool " + BuildInfo.Version + " - by Space Trucker";
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

    static bool SamePath(string a, string b)
    {
        return string.Equals(Path.GetFullPath(a).TrimEnd('\\'), Path.GetFullPath(b).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
    }

    void LoadAccounts()
    {
        account.Items.Clear();
        list.Items.Clear();
        if (Directory.Exists(Root))
            foreach (string d in Directory.GetDirectories(Root))
                if (SaveFolder.IsSteamIdFolder(Path.GetFileName(d))) account.Items.Add(Path.GetFileName(d));
        if (account.Items.Count == 0) { Log("No save folder found in " + Root + ". Start the game once and make a save."); return; }
        account.SelectedIndex = 0;
    }

    void LoadSaves()
    {
        list.Items.Clear();
        if (account.SelectedItem == null || !Directory.Exists(AccountDir)) return;
        foreach (string f in SaveFolder.SaveFiles(AccountDir, false))
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

    bool GameClosed()
    {
        if (!SaveFolder.GameRunning()) return true;
        MessageBox.Show(this, "Please close Marvel's Avengers first.\n\nThe game writes its saves when it exits, which would undo the change.",
                        "Avengers Save Tool", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        return false;
    }

    string Backup(string why)
    {
        string dst = Path.Combine(Root, "SaveTool Backups", DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss-fff") + "_" + why, (string)account.SelectedItem);
        Directory.CreateDirectory(dst);
        foreach (string f in SaveFolder.DatFiles(AccountDir)) File.Copy(f, Path.Combine(dst, Path.GetFileName(f)));
        Log("Backup: " + dst);
        return dst;
    }

    bool ReplaceSaves(string why, IList<KeyValuePair<string, byte[]>> files)
    {
        if (!GameClosed()) return false;
        Backup(why);
        foreach (string f in SaveFolder.ReplaceSaves(AccountDir, files)) Log("Could not delete " + Path.GetFileName(f) + " (the old file, kept beside the saves)");
        return true;
    }

    void Unlock()
    {
        if (!HaveAccount() || !GameClosed()) return;
        var targets = list.SelectedItems.Count > 0 ? list.SelectedItems.Cast<ListViewItem>().Select(i => (string)i.Tag).ToList()
                                                   : SaveFolder.SaveFiles(AccountDir, false);
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
            byte[] data = Save.EncodeChecked(plain, SteamId);
            if (data == null) throw new Exception("Verification failed for " + Path.GetFileName(f) + "; nothing was written.");
            SaveFolder.WriteAtomic(f, data);
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
        var files = SaveFolder.SaveFiles(src, true);
        if (files.Count == 0) { Log("No save1_*.dat files in " + src); return; }
        var raw = files.Select(f => Save.ReadFile(f)).ToList();

        ulong srcId = 0;
        string name = Path.GetFileName(src.TrimEnd('\\'));
        if (SaveFolder.IsSteamIdFolder(name) && raw.Any(d => Save.DecodeSave(d, ulong.Parse(name)) != null)) srcId = ulong.Parse(name);
        while (srcId == 0)
        {
            string s = Prompt("The shared save's owner Steam ID (the 17-digit name of the folder it came from):");
            if (s == null) return;
            s = s.Trim();
            if (SaveFolder.IsSteamIdFolder(s) && raw.Any(d => Save.DecodeSave(d, ulong.Parse(s)) != null)) srcId = ulong.Parse(s);
            else MessageBox.Show(this, "These files can't be opened with that Steam ID.", "Avengers Save Tool");
        }
        var converted = new List<KeyValuePair<string, byte[]>>();
        for (int i = 0; i < files.Count; i++)
        {
            byte[] plain = Save.DecodeSave(raw[i], srcId);
            if (plain == null) { Log(Path.GetFileName(files[i]) + ": skipped (damaged or from another account)"); continue; }
            byte[] data = Save.EncodeChecked(plain, SteamId);
            if (data == null) throw new Exception("Verification failed for " + Path.GetFileName(files[i]) + "; nothing was written.");
            converted.Add(new KeyValuePair<string, byte[]>(Path.GetFileName(files[i]), data));
        }
        if (converted.Count == 0) { Log("Nothing could be imported."); return; }
        if (MessageBox.Show(this, string.Format("Replace this account's saves with {0} imported file(s)?\nYour current saves are backed up first; your settings (profile1.dat) are kept.",
                converted.Count), "Avengers Save Tool", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        if (!ReplaceSaves("before-import", converted)) return;
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

        var files = SaveFolder.DatFiles(sel).Where(f => SaveFolder.SaveName.IsMatch(Path.GetFileName(f)) || SaveFolder.BackupName.IsMatch(Path.GetFileName(f)) ||
                                             string.Equals(Path.GetFileName(f), "profile1.dat", StringComparison.OrdinalIgnoreCase)).ToList();
        var saves = files.Where(f => SaveFolder.SaveName.IsMatch(Path.GetFileName(f))).ToList();
        if (saves.Count == 0) { Log("No save1_*.dat files in that folder."); return; }
        var data = files.ToDictionary(f => Path.GetFileName(f), f => Save.ReadFile(f));
        var unread = data.Where(kv => kv.Value == null).Select(kv => kv.Key).ToList();
        if (unread.Count > 0)
        {
            MessageBox.Show(this, "These files are too large to be saves of this game, so nothing was restored:\n" + string.Join("\n", unread),
                            "Avengers Save Tool", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        int readable = saves.Count(f => Save.DecodeSave(data[Path.GetFileName(f)], SteamId) != null);
        if (readable == 0) { MessageBox.Show(this, "None of these saves belong to this account, so the game couldn't load them.\nUse \"Import a shared save\" for saves from another account.", "Avengers Save Tool"); return; }
        if (MessageBox.Show(this, string.Format("Restore {0} file(s) from\n{1}?\n({2} of {3} saves are readable for this account.)\nThe current saves are backed up first.",
                files.Count, sel, readable, saves.Count), "Avengers Save Tool", MessageBoxButtons.YesNo) != DialogResult.Yes) return;
        if (!ReplaceSaves("before-restore", data.ToList())) return;
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
