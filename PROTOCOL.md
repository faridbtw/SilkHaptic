# DualSense Bluetooth transport

This port implements its own packet builder and resampler. The transport facts
were checked against these public implementations:

- MacGameFix `engine/patches/wine-11.21/0004-winebus-bluetooth.patch`:
  https://github.com/MathiasKowoll/MacGameFix/blob/main/engine/patches/wine-11.21/0004-winebus-bluetooth.patch
- DualSense Bluetooth audio protocol, including output CRC direction byte:
  https://github.com/Unravl/dualsense-audio-macos/blob/main/src/engine/ds5_protocol.h

Haptics use 64 frames of signed 8-bit stereo PCM at 3 kHz per report. The first
report is 0x33, 206 bytes; following reports are 0x32, 142 bytes. Control reports
are 0x31, 78 bytes. Each output report includes a four-bit sequence number and a
CRC-32 covering 0xA2 followed by all report bytes except the last four; the CRC
is appended little-endian. PCM session counters advance by two modulo 256.
The first PCM header initializes the audio session; counter wrap does not
reinitialize it. Only one thread in this bridge sends Bluetooth reports.

The game produces 48 kHz floats. A 193-tap Blackman-windowed low-pass filter
(1 kHz cutoff) suppresses aliasing before 16:1 downsampling, followed by
saturating signed 8-bit quantization. Bluetooth therefore preserves the authored
waveforms within the wireless transport's bandwidth, rather than emulating
them as ordinary motor rumble. USB keeps the original 48 kHz stream.

This uses the existing macOS IOHID APIs. No CoreAudio driver, kernel extension,
root helper, virtual controller, or system audio device is installed.

The sender does not transmit all-zero idle audio. A nonzero sample begins a
fresh haptic session, which includes a ~150 ms silent tail before restoring
ordinary rumble mode. During sustained haptics, audio mode is reasserted about
once per second. This keeps silent periods free of wireless output traffic
and leaves more of the Bluetooth link budget to the game's controller input
and output. Under a scheduling/radio stall, only the oldest queued samples
are dropped; the newest samples are retained.
