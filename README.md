<p align="center">
  <img src="assets/silkhaptic-banner.svg?v=2" alt="SilkHaptic. DualSense haptics for Silksong on macOS. USB-C and Bluetooth." width="100%">
</p>

<p align="center">
  <strong>Feel Hornet’s footsteps, dashes, and landings through your PS5 controller.</strong><br>
  Silksong’s PS5 haptic effects, brought to your Mac.
</p>

<p align="center">
  <a href="https://github.com/faridbtw/SilkHaptic/releases/latest"><strong>Download for macOS</strong></a>
  &nbsp; · &nbsp;
  <a href="#get-started">Get started</a>
  &nbsp; · &nbsp;
  <a href="#need-a-hand">Get help</a>
</p>

---

SilkHaptic uses the haptic effects already included in your copy of **Hollow Knight: Silksong**. Instead of feeling the same basic buzz for everything, you can feel the different textures of movement and action through a DualSense controller.

| Plug in | Go wireless | Launch and play |
| :--- | :--- | :--- |
| Connect your controller with a USB-C data cable. | Pair it with your Mac over Bluetooth. | The haptic companion starts and closes with the game. |

## What you need

- **Hollow Knight: Silksong for macOS.**
- **BepInEx 5, already installed and working.** This is the mod loader that lets the game run SilkHaptic.
- **A DualSense controller**, connected by USB or Bluetooth.
- **macOS 12 or newer.**

Tested in gameplay on an Apple Silicon Mac with a standard DualSense, using both connections. Intel builds and DualSense Edge support are included, but haven’t been tested on those devices.

## Get started

### 1. Download and install

Download **[SilkHaptic-macOS-v1.0.zip](https://github.com/faridbtw/SilkHaptic/releases/download/v1.0/SilkHaptic-macOS-v1.0.zip)** from the release page and unzip it.

Quit Silksong, then double-click **`Install.command`** inside the extracted folder. The installer finds the usual Steam installation and prepares the haptic effects from your own game files.

> Choose the **SilkHaptic macOS ZIP** under the release’s Assets section. GitHub’s “Source code” downloads are for building the project yourself.

### 2. Connect your controller

| USB-C | Bluetooth |
| :--- | :--- |
| Plug the controller into your Mac using a **data cable**. | Hold **Create + PS** until the light flashes. Open **System Settings → Bluetooth** and connect **DualSense Wireless Controller**. |
| Gives you the most consistent timing and full waveform detail. | Lets you play without a cable. Timing and detail can differ from USB. |

**Switch whenever you like.** USB takes priority when plugged in. Unplug it, and SilkHaptic uses Bluetooth if the controller is connected wirelessly.

### 3. Play

Open **`Launch Modded Silksong.command`** from the extracted folder, or launch through Steam **if you’ve already configured Steam to load BepInEx**.

That’s it. The haptic companion opens automatically. Keep your usual speakers or headphones selected for game audio.

## Everyday questions

**Do I have to open another app every time?**

No. Once the game launches with BepInEx, SilkHaptic starts its companion automatically and closes it when you quit.

**Will the normal Steam Play button work?**

Yes, if your Steam launch options already enable BepInEx. Otherwise, use the included `Launch Modded Silksong.command`.

**Do I need DSX, Wine, or a Bluetooth driver?**

No. Both connection methods work directly on macOS.

**Can I change the intensity?**

Yes. After your first launch, quit the game and open `BepInEx/config/com.will.silksong.ps5haptics.cfg` in your game folder. Change `MasterGain`: `0.5` is gentler, `1.0` is the default, and `2.0` is stronger. The game’s own vibration settings still apply.

**What happens to ordinary rumble?**

Effects without a matching PS5 waveform use the game’s normal rumble. Normal rumble is also used while the companion is disconnected.

## Need a hand?

| What’s happening? | What to try |
| :--- | :--- |
| Nothing vibrates. | Check that BepInEx loads, vibration is enabled in the game, and the controller is connected. |
| Bluetooth doesn’t work. | Unplug USB and check that macOS Bluetooth says **Connected**, rather than just listing the paired controller. |
| Wireless effects feel delayed or uneven. | Try USB to compare. Close other controller utilities; they can compete for the same connection. |
| The controller connects, but haptics stay silent. | Try disabling Steam Input for Silksong, then launch again. |
| The installer can’t find my game. | Follow the [custom installation instructions](docs/ADVANCED.md#installing-somewhere-else). |

To check the controller on its own, quit the game and run **`Test Haptics.command`**. You should feel the left grip, then the right. For a wireless check, unplug USB and run **`Test Bluetooth Haptics.command`**.

Still stuck? **[Open an issue](https://github.com/faridbtw/SilkHaptic/issues/new)** and include your macOS version, controller model, connection type, and [the logs](docs/ADVANCED.md#finding-the-logs).

## Update or remove

**Update:** quit the game, extract the new release, and run `Install.command` again. Your previous SilkHaptic installation is backed up automatically.

**Remove:** quit the game and run `Uninstall.command`. It moves SilkHaptic out of the active mods folder. Your saves and other mods are left in place.

## For the curious

- [Advanced settings, custom game folders, and building from source](docs/ADVANCED.md)
- [How the Bluetooth transport works](PROTOCOL.md)
- [What’s been tested](VALIDATION.txt)

SilkHaptic prepares all **314 haptic clips** from your own game installation. No Team Cherry game assets are included in this download.

## Credits

Based on **countlessbats’ Silk-Soft Touch / Silksong PS5 Haptics**, using the [MIT-licensed source preserved by alexkiri](https://github.com/alexkiri/SilksongHaptics) at commit `5312eab`. The macOS port and Bluetooth backend are maintained by [faridbtw](https://github.com/faridbtw).

[MIT license](LICENSE) · [Third-party notices](ThirdParty)

Silksong and its game assets belong to Team Cherry. DualSense is a Sony trademark. SilkHaptic is a community project.
