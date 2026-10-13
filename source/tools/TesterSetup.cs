using System;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;

class TesterSetup : Form {
    static string root = AppDomain.CurrentDomain.BaseDirectory;
    static string gameDir = Path.Combine(root, "StarForge", "NativeGame");
    Label status;
    static string EdgePath() {
        foreach (var folder in new[] { Environment.SpecialFolder.ProgramFilesX86, Environment.SpecialFolder.ProgramFiles }) {
            string path = Path.Combine(Environment.GetFolderPath(folder), "Microsoft", "Edge", "Application", "msedge.exe");
            if (File.Exists(path)) return path;
        }
        return null;
    }
    static bool ArchiveValid(string path) {
        try { using (var archive = ZipFile.OpenRead(path)) {
            bool script = false, texture = false;
            foreach (var entry in archive.Entries) {
                script |= entry.FullName.Contains("ast_corneria/");
                texture |= entry.FullName.Contains("ast_enmy_planet/");
            }
            return archive.Entries.Count > 100 && script && texture;
        }} catch { return false; }
    }
    static void ImportArchive(string path) {
        if (!ArchiveValid(path)) throw new Exception("This is not a readable Star Fox 64 asset archive. Choose sf64.o2r from a working Starship installation.");
        string target = Path.Combine(gameDir, "sf64.o2r");
        if (String.Equals(Path.GetFullPath(path), Path.GetFullPath(target), StringComparison.OrdinalIgnoreCase)) return;
        if (File.Exists(target)) File.Copy(target, target + ".before-setup", true);
        File.Copy(path, target, true);
    }
    static void StartLocal(string filename, string folder) {
        Process.Start(new ProcessStartInfo(Path.Combine(folder, filename)) { WorkingDirectory = folder, UseShellExecute = true });
    }
    Button AddButton(string text, int y, Action action) {
        var button = new Button { Text = text, Left = 24, Top = y, Width = 550, Height = 38 };
        button.Click += (s, e) => { try { action(); RefreshStatus(); } catch (Exception ex) { MessageBox.Show(this, ex.Message, "StarForge setup"); } };
        Controls.Add(button); return button;
    }
    void RefreshStatus() {
        status.Text = "Windows 64-bit: " + (Environment.Is64BitOperatingSystem ? "OK" : "required") +
            "\nMicrosoft Edge: " + (EdgePath() != null ? "OK" : "install Edge using the button below") +
            "\nGame assets: " + (ArchiveValid(Path.Combine(gameDir, "sf64.o2r")) ? "ready" : "choose one of the two setup options below");
    }
    TesterSetup() {
        Text = "StarForge v" + System.Text.RegularExpressions.Regex.Match(File.ReadAllText(Path.Combine(root, "StarForge", "version.json")), @"\d+\.\d+\.\d+").Value + " · Tester setup"; ClientSize = new Size(600, 460);
        Font = new Font("Segoe UI", 10); FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = false;
        var icon = Path.Combine(root, "StarForge", "StarForge.ico"); if (File.Exists(icon)) Icon = new Icon(icon);
        Controls.Add(new Label { Text = "Welcome! Keep this extracted folder together.\nNo administrator rights or development tools are needed.", Left = 24, Top = 20, Width = 550, Height = 50 });
        status = new Label { Left = 24, Top = 80, Width = 550, Height = 85 }; Controls.Add(status);
        AddButton("1A. Use my existing Starship sf64.o2r archive", 175, () => {
            using (var dialog = new OpenFileDialog { Title = "Choose your Starship game archive", Filter = "Star Fox 64 archive (sf64.o2r)|sf64.o2r", CheckFileExists = true }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                ImportArchive(dialog.FileName);
            }
        });
        AddButton("1B. Create game assets from my own ROM", 220, () => {
            MessageBox.Show(this, "Starship will ask for your supported US 1.0 or US 1.1 .z64 ROM.\nAfter extraction, close Starship and return here.\nIf it is already set up, Starship opens normally.", "Create game assets");
            StartLocal("Starship.exe", gameDir);
        });
        AddButton("2. Open StarForge", 275, () => {
            if (!Environment.Is64BitOperatingSystem) throw new Exception("This tester build needs a 64-bit Windows PC.");
            if (EdgePath() == null) throw new Exception("Install Microsoft Edge, then try again.");
            if (!ArchiveValid(Path.Combine(gameDir, "sf64.o2r"))) throw new Exception("Complete step 1 first. The editor can open by itself, but Playtest needs your game archive.");
            StartLocal("StarForge.exe", Path.Combine(root, "StarForge"));
        });
        AddButton("Read the illustrated setup guide and tester checklist", 320, () => StartLocal("START-HERE.html", root));
        AddButton("Get Microsoft Edge from Microsoft", 365, () => Process.Start("https://www.microsoft.com/edge/download"));
        Controls.Add(new Label { Text = "Already set up? Next time, open StarForge\\StarForge.exe directly.", Left = 24, Top = 420, Width = 550, Height = 25 });
        Activated += (s, e) => RefreshStatus(); RefreshStatus();
    }
    [STAThread] static void Main(string[] args) {
        try {
            if (args.Length > 0 && args[0] == "--verify") {
                File.WriteAllText(Path.Combine(root, "setup-check.txt"), "Windows64=" + Environment.Is64BitOperatingSystem + "\nEdge=" + (EdgePath() != null) + "\nAssets=" + ArchiveValid(Path.Combine(gameDir, "sf64.o2r")) + "\nGame=" + File.Exists(Path.Combine(gameDir, "Starship.exe")));
                return;
            }
            if (args.Length == 2 && args[0] == "--import-archive") { ImportArchive(args[1]); return; }
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new TesterSetup());
        } catch (Exception e) { File.WriteAllText(Path.Combine(root, "setup-error.txt"), e.Message); if (args.Length == 0) MessageBox.Show(e.Message, "StarForge setup"); Environment.ExitCode = 1; }
    }
}







