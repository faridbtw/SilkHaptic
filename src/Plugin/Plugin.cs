using System;
using System.IO;
using BepInEx;
using BepInEx.Configuration;
using BepInEx.Logging;
using HarmonyLib;
using UnityEngine;

namespace SilksongPS5Haptics
{
    internal static class GameSupport
    {
        public const string Version = "1.1.0";
#if HOLLOW_KNIGHT
        public const string Id = "com.faridbtw.hollowknight.haptics";
        public const string Name = "SilkHaptic (Hollow Knight)";
        public const int Port = 48112;
#else
        public const string Id = "com.will.silksong.ps5haptics";
        public const string Name = "SilkHaptic";
        public const int Port = 48111;
#endif
    }

    [BepInPlugin(GameSupport.Id, GameSupport.Name, GameSupport.Version)]
    public class PS5HapticsPlugin : BaseUnityPlugin
    {
        internal static ManualLogSource Log;
        internal static ConfigEntry<bool> KeepRumble;
        internal static ConfigEntry<float> MasterGain;
        internal static ConfigEntry<int> BridgePort;
        internal static ConfigEntry<string> ClipsPath;
        internal static ConfigEntry<bool> AutoStartBridge;

        private void Awake()
        {
            Log = Logger;

            KeepRumble = Config.Bind("General", "KeepRumble", false,
                "Also send the normal dual-motor rumble while a PS5 haptic clip plays (may feel doubled).");
            MasterGain = Config.Bind("General", "MasterGain", 1.0f,
                new ConfigDescription("Overall haptic intensity.", new AcceptableValueRange<float>(0f, 2f)));
            BridgePort = Config.Bind("General", "BridgePort", GameSupport.Port,
                new ConfigDescription("Localhost bridge port.", new AcceptableValueRange<int>(1024, 65535)));
            ClipsPath = Config.Bind("General", "ClipsPath", "",
                "Folder containing the extracted PS5 haptic WAVs. Empty = <plugin folder>/clips.");

            AutoStartBridge = Config.Bind("General", "AutoStartBridge", true,
                "Launch the native macOS HapticsBridge automatically when the game starts.");

            string clips = ClipsPath.Value;
            if (string.IsNullOrEmpty(clips))
                clips = Path.Combine(Path.GetDirectoryName(Info.Location), "clips");

            if (AutoStartBridge.Value)
                TryStartBridge();

            HapticEngine.Start(clips, BridgePort.Value);
            new Harmony(GameSupport.Id).PatchAll(typeof(MixerPatch));
            Log.LogInfo($"PS5 haptics plugin loaded. Clips: {clips}");
        }

        // Unity main-thread state, cached each frame for the audio thread.
        private void Update()
        {
            try
            {
                HapticEngine.CachedTimeScale = Time.timeScale;
                HapticEngine.CachedStrengthMultiplier = VibrationManager.StrengthMultiplier;
                HapticEngine.CachedMasterGain = MasterGain.Value;

                SuperviseBridge();
            }
            catch
            {
                // ConfigManager may not be ready during early boot; keep last values.
            }
        }

        private float _nextBridgeCheck = 8f;   // initial grace: give the Awake launch time to open its listener
        private int _bridgeRelaunches;

        // Autostart is no longer fire-and-forget. The launch can silently come to nothing — classically
        // when a bridge from the PREVIOUS session was still mid-teardown, so our fresh bridge lost the
        // single-instance mutex to a dying process and exited (then the old one finished dying: no bridge
        // at all). If the engine still can't connect and NO bridge process exists, launch another one.
        // Capped so a genuinely broken bridge (or a user who deliberately exited the tray app) isn't
        // relaunched forever.
        private void SuperviseBridge()
        {
            if (!AutoStartBridge.Value || HapticEngine.Connected) return;
            if (Time.unscaledTime < _nextBridgeCheck) return;
            _nextBridgeCheck = Time.unscaledTime + 10f;
            if (_bridgeRelaunches >= 5) return;
            if (bridgeProcess != null && !bridgeProcess.HasExited) return;

            _bridgeRelaunches++;
            Log.LogWarning($"Bridge unreachable and no HapticsBridge process found — relaunching (attempt {_bridgeRelaunches}/5).");
            TryStartBridge();
        }

        private System.Diagnostics.Process bridgeProcess;

        private void OnApplicationQuit()
        {
            StopBridge();
        }

        private void OnDestroy()
        {
            HapticEngine.Shutdown();
            StopBridge();
        }

        // Close the bridge we started when the game quits. Without this the tray
        // app lingers as a child of the game process, so Steam keeps showing the
        // game as "Running" until you Stop it manually. Only the instance that
        // actually launched the running bridge kills it (a second coop instance
        // loses the mutex race, so its handle has already exited -> no-op).
        private void StopBridge()
        {
            try
            {
                if (bridgeProcess != null && !bridgeProcess.HasExited)
                    bridgeProcess.Kill();
            }
            catch { }
        }

        // The bridge is single-instance (mutex), so launching when one is
        // already running is a harmless no-op.
        private void TryStartBridge()
        {
            try
            {
                string exe = Path.Combine(Path.GetDirectoryName(Info.Location), "HapticsBridge", "HapticsBridge");
                if (!File.Exists(exe)) { Log.LogWarning("macOS HapticsBridge missing: " + exe); return; }
                string args = string.Format(System.Globalization.CultureInfo.InvariantCulture,
                    "--port {0} --parent-pid {1}", BridgePort.Value, System.Diagnostics.Process.GetCurrentProcess().Id);
                var start = new System.Diagnostics.ProcessStartInfo(exe, args) {
                    UseShellExecute = false, WorkingDirectory = Path.GetDirectoryName(exe)
                };
                // Doorstop belongs in Unity, never in the native audio bridge.
                foreach (string key in new System.Collections.ArrayList(start.EnvironmentVariables.Keys))
                    if (key.StartsWith("DYLD_") || key.StartsWith("LD_") || key.StartsWith("DOORSTOP_"))
                        start.EnvironmentVariables.Remove(key);
                bridgeProcess = System.Diagnostics.Process.Start(start);
                Log.LogInfo("Launched macOS bridge: " + exe);
            }
            catch (Exception e) { Log.LogWarning("Could not start macOS bridge: " + e.Message); }
        }

    }

    [HarmonyPatch(typeof(VibrationManager), nameof(VibrationManager.GetMixer))]
    internal static class MixerPatch
    {
        private static void Postfix(ref VibrationMixer __result)
        {
            if (__result == null || __result is HapticMixerWrapper)
                return;
            __result = HapticMixerWrapper.GetOrCreate(__result);
        }
    }
}
