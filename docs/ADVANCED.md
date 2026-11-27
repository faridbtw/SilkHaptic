# Advanced guide

[← Back to SilkHaptic](../README.md)

SilkHaptic supports the updated macOS Hollow Knight build and Hollow Knight: Silksong.

Most players only need the [quick setup](../README.md#get-started). This page covers other install locations, configuration, troubleshooting details, and building the project.

## Installing somewhere else

The installer looks for the usual Steam folder:

```text
~/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong
```

For another Steam library or game location, open Terminal in the extracted release folder and run:

```sh
./Install.command "/path/to/Hollow Knight Silksong"
```

Use the folder containing **Hollow Knight Silksong.app**, not the app itself. Pass the same folder to the launcher or uninstaller:

```sh
./"Launch Modded Silksong.command" "/path/to/Hollow Knight Silksong"
./Uninstall.command "/path/to/Hollow Knight Silksong"
```

You can also set `SILKSONG_GAME_DIR` in your shell environment. BepInEx 5 must already be installed in the selected game folder.

For original Hollow Knight, the default folder is `~/Library/Application Support/Steam/steamapps/common/Hollow Knight`. To use another location:

```sh
./"Install Hollow Knight.command" "/path/to/Hollow Knight"
./"Launch Modded Hollow Knight.command" "/path/to/Hollow Knight"
./"Uninstall Hollow Knight.command" "/path/to/Hollow Knight"
```

Use the folder containing `hollow_knight.app`. `HOLLOW_KNIGHT_GAME_DIR` overrides its default location. The updated Unity 6 version is required; the extractor rejects older builds without the PS5 clips.

The included Hollow Knight launcher uses Rosetta on Apple Silicon with BepInEx 5 and passes Steam’s app identity for direct launches. Keep Steam running.

## Settings

The first game launch creates this file inside your game folder:

```text
BepInEx/config/com.will.silksong.ps5haptics.cfg
```

| Setting | Default | What it does |
| :--- | :--- | :--- |
| `AdaptiveLightbar` | `true` | Changes the DualSense light bar with game state and health in both games. |
| `MasterGain` | `1.0` | Haptic intensity, from `0` to `2`. |
| `KeepRumble` | `false` | Adds ordinary rumble alongside the PS5 effect. This can feel doubled. |
| `AutoStartBridge` | `true` | Starts the haptic companion with the game. |
| `BridgePort` | `48111` | Local connection between the game and its companion. |
| `ClipsPath` | Empty | Optional path to a different folder of extracted haptic clips. |

The in-game vibration settings are respected. Keep game audio on your usual speakers or headphones; you don’t need to select the controller as the system audio output.

For Hollow Knight, the config is `BepInEx/config/com.faridbtw.hollowknight.haptics.cfg`; its default bridge port is `48112`. Silksong uses its existing config and port `48111`.

## Finding the logs

Look inside your Silksong folder:

| Log | Location |
| :--- | :--- |
| Game and plugin | `BepInEx/LogOutput.log` |
| Haptic companion | `BepInEx/plugins/SilkSoftMac/HapticsBridge/HapticsBridge.log` |

`Connected to HapticsBridge` means the game has reached its companion. `Haptic play:` confirms that a game event selected a PS5 waveform. The companion log identifies USB or Bluetooth output and records connection errors.

Hollow Knight’s companion log is `BepInEx/plugins/SilkHaptic/HapticsBridge/HapticsBridge.log`.

The Silksong internal folder is still called `SilkSoftMac` for compatibility with the first version of this port. The project’s public name is **SilkHaptic**.

## Connection details

**USB:** the companion opens the DualSense audio output directly. It sends 48 kHz audio to actuator channels 3 and 4 and silence to channels 1 and 2. It leaves the Mac’s default audio output unchanged.

**Bluetooth:** the companion filters and converts the authored waveform to the controller’s 3 kHz signed 8-bit stereo haptic format, then sends it through macOS HID APIs. No system driver or administrator helper is installed. Idle silence isn’t transmitted, leaving more wireless bandwidth for the game’s controller traffic.

USB is preferred when its audio output is available. Without USB, the companion uses a connected Bluetooth DualSense. Bluetooth timing depends on radio conditions, macOS scheduling, and other applications writing to the controller.

For the packet layout and resampling design, see [PROTOCOL.md](../PROTOCOL.md). For test coverage and hardware limits, see [VALIDATION.txt](../VALIDATION.txt). Steam Controller output is not implemented.

## Backups

Installing an update or running the uninstaller moves the previous plugin folder into:

```text
BepInEx/SilkSoftMac-backups/
```

Hollow Knight uses `BepInEx/SilkHaptic-backups/`.

This is outside the active plugins folder. Configuration, saves, and other mods are preserved.

## Build from source

You need Xcode command line tools, Mono (`csc`), .NET 8 SDK, and your own Silksong installation with BepInEx 5. The build reads the game and loader assemblies locally; those files are not redistributed.

```sh
./build.sh "/path/to/Hollow Knight Silksong"
python3 tools/package_release.py
```

If .NET isn’t on your PATH, specify it:

```sh
DOTNET=/path/to/dotnet ./build.sh "/path/to/Hollow Knight Silksong"
```

To build both game plugins, pass both installation folders, or keep Hollow Knight in its default Steam location:

```sh
./build.sh "/path/to/Hollow Knight Silksong" "/path/to/Hollow Knight"
```

The packager expects both compiled plugins for the combined release.

The build also checks `tools/dotnet/dotnet` as a fallback. Compiled files go into `dist/`; the installable ZIP goes into `releases/`. The release includes self-contained extractors for Apple Silicon and Intel, so players do not need .NET installed.

## Verification

```sh
python3 tools/verify_bluetooth.py
python3 tools/verify_bridge.py
```

The first command checks Bluetooth packet layouts, CRCs, sequence counters, stereo isolation, sample counts, and alias rejection without a controller. The second checks fragmented network delivery and output consumption; it needs a connected DualSense.

Run the network test with the game closed so two companions don’t compete for the controller.

The bridge’s `--self-test` checks USB channel routing, speaker-channel silence, limiting, invalid samples, and underrun handling without hardware. `--list` lists available outputs. `--transport auto|usb|bluetooth` selects a connection method for manual launches.
