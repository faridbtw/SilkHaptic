#!/bin/bash
set -euo pipefail
PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="${1:-${SILKSONG_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}}"
PLUGIN_DIR="$GAME_DIR/BepInEx/plugins/SilkSoftMac"
if [[ ! -f "$GAME_DIR/BepInEx/core/BepInEx.dll" ]]; then echo 'BepInEx 5 is required in the selected game directory.'; exit 1; fi
if [[ ! -f "$GAME_DIR/Hollow Knight Silksong.app/Contents/Resources/Data/Managed/Assembly-CSharp.dll" ]]; then echo 'macOS Silksong installation not found.'; exit 1; fi
if pgrep -x 'Hollow Knight Silksong' >/dev/null; then echo 'Quit Silksong before installing.'; exit 1; fi
RID=osx-arm64
[[ "$(uname -m)" == x86_64 ]] && RID=osx-x64
STAGE_DIR="$(mktemp -d "$GAME_DIR/BepInEx/.SilkSoftMac-stage.XXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
cp -R "$PACKAGE_DIR/BepInEx/plugins/SilkSoftMac" "$STAGE_DIR/SilkSoftMac"
echo 'Extracting PS5 haptic waveforms from your own game files...'
"$PACKAGE_DIR/Extractor/$RID/Extractor" "$GAME_DIR" "$STAGE_DIR/SilkSoftMac/clips"
chmod +x "$STAGE_DIR/SilkSoftMac/HapticsBridge/HapticsBridge"
mkdir -p "$GAME_DIR/BepInEx/plugins"
if [[ -e "$PLUGIN_DIR" ]]; then
  BACKUP_DIR="$GAME_DIR/BepInEx/SilkSoftMac-backups/$(date +%Y%m%d-%H%M%S)-$$"
  mkdir -p "$BACKUP_DIR"
  mv "$PLUGIN_DIR" "$BACKUP_DIR/SilkSoftMac"
  echo "Previous version saved in: $BACKUP_DIR"
fi
mv "$STAGE_DIR/SilkSoftMac" "$PLUGIN_DIR"
echo "Installed: $PLUGIN_DIR"
echo 'Connect the DualSense by USB or macOS Bluetooth. Launch Silksong with BepInEx enabled.'
