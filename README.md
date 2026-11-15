# SilkHaptic

macOS port of **Silk-Soft Touch / Silksong PS5 Haptics**, based on the MIT-licensed
[countlessbats source preserved by alexkiri](https://github.com/alexkiri/SilksongHaptics)
(commit `5312eab`). A BepInEx 5 plugin mixes Silksong's authored PS5 waveforms;
a native CoreAudio/IOKit companion sends them to the DualSense's actuators
over USB or Bluetooth. No Windows software, DSX, or Wine is required by this port.

[Download the latest macOS release](https://github.com/faridbtw/SilkHaptic/releases/latest).

## Install

1. Have a working **BepInEx 5** installation for the macOS version of Silksong.
2. Connect a **DualSense or DualSense Edge** by USB, or pair it in macOS Bluetooth settings, and quit Silksong.
3. Download and extract the release ZIP, then open `Install.command` in that folder. It extracts the 314 waveforms
   from your own macOS game installation and installs the plugin.
4. Launch the game with BepInEx. `Launch Modded Silksong.command` uses your
   existing BepInEx launcher and selects Rosetta if your installed Doorstop is
   Intel-only. Native arm64 Doorstop installations use arm64.

The internal plugin folder remains `SilkSoftMac` for compatibility with the initial port.

The installer defaults to:
`~/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong`.
For a different installation, run `./Install.command "/path/to/game folder"`
and pass the same path to the launcher/uninstaller, or set `SILKSONG_GAME_DIR`.
The release includes self-contained Apple Silicon and Intel extractors; you
need no .NET installation to install it. macOS 12+ is required by the extractor.
The native bridge is universal arm64/x86_64. Intel hardware has not been tested.
BepInEx itself is not bundled or replaced.

USB is preferred automatically while its audio endpoint is available. Without
USB, the bridge streams haptic packets directly to the Bluetooth controller.
To pair: hold Create + PS until the light flashes, then connect the controller
in System Settings → Bluetooth. No driver or administrator access is needed.

Keep normal game audio on your speakers/headphones. The bridge opens the
controller directly and leaves the system default output unchanged. Over USB,
it writes silence to channels 1/2 and haptics to channels 3/4. Over Bluetooth,
it low-pass filters and resamples the authored waveforms from 48 kHz to the
controller's 3 kHz signed 8-bit stereo haptic format. Bluetooth may have more
latency and less detail than USB; it still plays the authored waveform. The bridge sends no wireless audio
packets during idle silence, leaving bandwidth for the game's controller traffic.
See PROTOCOL.md for the transport details.

`Test Haptics.command` sends a one-second left-then-right actuator test. Run it
with Silksong closed so the game's bridge does not already hold the port lock.
`Test Bluetooth Haptics.command` explicitly tests the wireless transport.
Keep the USB cable unplugged for a meaningful wireless test. Do not run a
standalone Bluetooth test while the game bridge is already streaming wirelessly.
You can also run the bridge with `--list` to list audio outputs.

## Settings and logs

The first game launch creates:
`BepInEx/config/com.will.silksong.ps5haptics.cfg`.

- `MasterGain`: 0–2; default 1. Haptic intensity.
- `KeepRumble`: false by default. Enable only if you want ordinary rumble too.
- `AutoStartBridge`: true by default.
- `BridgePort`: localhost TCP port; default 48111.
- `ClipsPath`: optional override for the extracted WAV directory.

The game's vibration settings still apply. Events without a matching waveform
and events while the bridge is disconnected use the game's original rumble.
USB removal switches to Bluetooth if a paired controller is available; otherwise
the connection closes and normal game rumble remains. Reconnecting USB switches
back to its full-rate audio output.
The bridge uses a bounded queue, retains the newest samples during wireless
stalls, and closes with the game.

Game log: `BepInEx/LogOutput.log`. Look for `Connected to HapticsBridge` and
`Haptic play:`. Bridge log:
`BepInEx/plugins/SilkSoftMac/HapticsBridge/HapticsBridge.log`.

If the device appears but you feel nothing, quit controller utility apps or try
disabling Steam Input for Silksong; another program may reset the HID audio mode.
If Bluetooth is absent, confirm the pad is connected in macOS settings, not
just paired or connected to another device. Steam Controller output is not
implemented in this macOS port.

## Remove or update

Quit Silksong, then open `Uninstall.command`. It moves this port's folder into
`BepInEx/SilkSoftMac-backups`, outside the plugin scan path, so removal is
reversible. Other mods and saves are untouched. Installing over an existing
SilkHaptic installation similarly preserves the old folder there.

## Build

Requires Xcode command line tools, Mono (`csc`), and .NET 8 SDK. Run:

```sh
DOTNET=/path/to/dotnet ./build.sh "/path/to/Silksong"
```

Without `DOTNET`, the build uses `dotnet` on PATH, then `tools/dotnet/dotnet`. The build references
the user's local game/BepInEx assemblies; they are not redistributed. The
installer's extraction runs locally, and the release ZIP contains no game clips.
After building, run `python3 tools/package_release.py` to create the installable ZIP
and its SHA-256 checksum under `releases/`.

`python3 tools/verify_bridge.py` tests fragmented network delivery and actual
output consumption on a connected USB or Bluetooth DualSense.
`python3 tools/verify_bluetooth.py` checks packet structure, CRC against Python
zlib, sequence/counter wrap, stereo isolation, resampling, and alias rejection. The bridge's `--self-test`
checks actuator channel mapping, silence on speaker channels, limiting,
nonfinite input, and underrun silence without a controller.

## License

MIT; see LICENSE. Upstream copyright: countlessbats and contributors, 2026.
The macOS backend, independent Bluetooth encoder/resampler, and packaging
are maintained as SilkHaptic by [faridbtw](https://github.com/faridbtw). Protocol references are recorded in PROTOCOL.md. Team Cherry owns
Silksong and its haptic assets. DualSense is a Sony trademark.
