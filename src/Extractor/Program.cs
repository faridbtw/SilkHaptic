using System;
namespace HapticsBridge {
internal static class Engine { public static void Log(string s) { Console.Error.WriteLine(s); } }
internal static class Program {
public static int Main(string[] args) {
if (args.Length != 2) { Console.Error.WriteLine("Usage: Extractor <game-directory> <clips-directory>"); return 2; }
try { int n=ClipExtractor.ExtractAll(args[0],args[1],null); Console.WriteLine("Extracted " + n + " haptic clips."); return n > 0 ? 0 : 1; }
catch(Exception e) { Console.Error.WriteLine(e); return 1; }
}
}}
