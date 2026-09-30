// Disposable test double only. Does not contact USB, Android, or real ADB.
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

class FakeAdb {
    static string Root = Environment.GetEnvironmentVariable("ACQUEST_INSTALL_TEST_ROOT");
    static string Scenario = Environment.GetEnvironmentVariable("ACQUEST_INSTALL_TEST_SCENARIO") ?? "fresh";
    const string Marker = "__ACQUEST_INSTALL_OK__\n";
    static string Remote(string path) {
        if (!path.StartsWith("files/rom/", StringComparison.Ordinal)) throw new Exception("Unexpected remote path " + path);
        return Path.Combine(Root, "private", path.Replace('/', Path.DirectorySeparatorChar));
    }
    static void Out(string text) { Console.Write(text); }
    static int Main(string[] original) {
        if (String.IsNullOrEmpty(Root) || !Directory.Exists(Root)) return 99;
        File.AppendAllText(Path.Combine(Root, "calls.txt"), String.Join("|", original).Replace("\n", "\\n") + "\n");
        string serial = "";
        string[] a = original;
        if (a.Length > 1 && a[0] == "-s") { serial = a[1]; a = new string[original.Length - 2]; Array.Copy(original, 2, a, 0, a.Length); }
        if (a[0] == "devices") {
            Out("List of devices attached\n");
            if (Scenario == "unauthorized") Out("QST\tunauthorized\n");
            else if (Scenario == "offline") Out("QST\toffline\n");
            else {
                if (Scenario != "phoneonly") Out("QST\tdevice product:quest\n");
                if (Scenario == "twoquests") Out("QST2\tdevice product:quest\n");
                Out("PHONE\tdevice product:pixel\n");
            }
            return 0;
        }
        if (a[0] == "shell" && a[1] == "getprop") { Out(serial == "PHONE" ? "Pixel 8\n" : "Quest 3\n"); return 0; }
        if (serial != "QST") throw new Exception("Mutation/query routed to wrong device " + serial);
        if (a[0] == "shell" && a[1] == "pidof") {
            if (Scenario == "busy") { Out("2468\n"); return 0; }
            return 1;
        }
        if (a[0] == "install") {
            if (a.Length != 3 || a[1] != "-r" || !File.Exists(a[2])) throw new Exception("Bad install arguments");
            if (Scenario == "installfail") { Out("Failure [INSTALL_FAILED_UPDATE_INCOMPATIBLE]\n"); return 1; }
            Out("Performing Streamed Install\nSuccess\n"); return 0;
        }
        if (a.Length != 6 || a[1] != "run-as" || a[2] != "com.liquidazir.animalcrossingquest" || a[3] != "sh" || a[4] != "-c")
            throw new Exception("Unexpected command " + String.Join("|", a));
        string command = a[5];
        if (a[0] == "exec-in") {
            Match path = Regex.Match(command, "cat > (files/rom/\\S+)");
            if (!path.Success) throw new Exception("Bad upload");
            using (Stream output = File.Create(Remote(path.Groups[1].Value))) Console.OpenStandardInput().CopyTo(output);
            return 0;
        }
        if (a[0] != "exec-out" || !command.StartsWith("set -eu\n") || !command.EndsWith("printf '__ACQUEST_INSTALL_OK__\\n'"))
            throw new Exception("Missing remote success marker");
        if (Scenario == "remoteerror") { Out("run-as: unavailable\n"); return 0; }
        if (command.Contains("\npwd\n")) Out("/data/user/0/com.liquidazir.animalcrossingquest\n");
        else if (command.Contains("find files/rom")) {
            string folder = Path.Combine(Root, "private", "files", "rom");
            if (Directory.Exists(folder)) foreach (string path in Directory.GetFiles(folder)) Out("files/rom/" + Path.GetFileName(path) + "\n");
        } else if (command.Contains("mkdir -p files/rom")) Directory.CreateDirectory(Path.Combine(Root, "private", "files", "rom"));
        else if (command.Contains("sha256sum ")) {
            string path = Regex.Match(command, "sha256sum (files/rom/\\S+)").Groups[1].Value;
            byte[] bytes = File.ReadAllBytes(Remote(path));
            using (SHA256 hash = SHA256.Create()) {
                string digest = BitConverter.ToString(hash.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
                if (Scenario == "hashfail") digest = new string('0', 64);
                Out(digest + "  " + path + "\n" + bytes.LongLength + "\n");
            }
        } else if (command.Contains("if [ -e ")) {
            string path = Regex.Match(command, "-e (files/rom/\\S+) ").Groups[1].Value;
            Out(File.Exists(Remote(path)) ? "yes\n" : "no\n");
        } else if (command.Contains("mv -n ")) {
            Match paths = Regex.Match(command, "mv -n (files/rom/\\S+) (files/rom/\\S+)");
            string source = Remote(paths.Groups[1].Value), destination = Remote(paths.Groups[2].Value);
            if (Scenario == "race") File.WriteAllText(destination, "existing-file-wins");
            if (!File.Exists(destination)) File.Move(source, destination);
        } else if (command.Contains("rm -f ")) {
            string path = Regex.Match(command, "rm -f (files/rom/\\S+)").Groups[1].Value;
            File.Delete(Remote(path));
        } else throw new Exception("Unexpected app script " + command);
        Out(Marker);
        return 0;
    }
}
