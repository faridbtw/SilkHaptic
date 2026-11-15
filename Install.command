#!/bin/bash
set -euo pipefail
PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
case "${SILKHAPTIC_GAME:-silksong}" in
  hollow-knight)
    DEFAULT_DIR="${HOLLOW_KNIGHT_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight}"
    GAME_APP="hollow_knight.app"; GAME_NAME="Hollow Knight"; PLUGIN_NAME="SilkHaptic"
    PAYLOAD="$PACKAGE_DIR/HollowKnight/BepInEx/plugins/$PLUGIN_NAME"
    ;;
  silksong)
    DEFAULT_DIR="${SILKSONG_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}"
    GAME_APP="Hollow Knight Silksong.app"; GAME_NAME="Hollow Knight Silksong"; PLUGIN_NAME="SilkSoftMac"
    PAYLOAD="$PACKAGE_DIR/BepInEx/plugins/$PLUGIN_NAME"
    ;;
  *) echo 'Unknown game. Use Install.command or Install Hollow Knight.command.'; exit 1 ;;
esac
GAME_DIR="${1:-$DEFAULT_DIR}"
PLUGIN_DIR="$GAME_DIR/BepInEx/plugins/$PLUGIN_NAME"
if [[ ! -f "$GAME_DIR/BepInEx/core/BepInEx.dll" ]]; then echo "BepInEx 5 is required in your $GAME_NAME folder."; exit 1; fi
if [[ ! -f "$GAME_DIR/$GAME_APP/Contents/Resources/Data/Managed/Assembly-CSharp.dll" ]]; then echo "macOS $GAME_NAME installation not found."; exit 1; fi
if [[ ! -d "$PAYLOAD" ]]; then echo "The $GAME_NAME plugin is missing from this download."; exit 1; fi
if pgrep -x "$GAME_NAME" >/dev/null; then echo "Quit $GAME_NAME before installing."; exit 1; fi
RID=osx-arm64
[[ "$(uname -m)" == x86_64 ]] && RID=osx-x64
STAGE_DIR="$(mktemp -d "$GAME_DIR/BepInEx/.$PLUGIN_NAME-stage.XXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
cp -R "$PAYLOAD" "$STAGE_DIR/$PLUGIN_NAME"
echo "Preparing $GAME_NAME haptics from your own game files..."
"$PACKAGE_DIR/Extractor/$RID/Extractor" "$GAME_DIR" "$STAGE_DIR/$PLUGIN_NAME/clips"
chmod +x "$STAGE_DIR/$PLUGIN_NAME/HapticsBridge/HapticsBridge"
mkdir -p "$GAME_DIR/BepInEx/plugins"
if [[ -e "$PLUGIN_DIR" ]]; then
  BACKUP_DIR="$GAME_DIR/BepInEx/$PLUGIN_NAME-backups/$(date +%Y%m%d-%H%M%S)-$$"
  mkdir -p "$BACKUP_DIR"
  mv "$PLUGIN_DIR" "$BACKUP_DIR/$PLUGIN_NAME"
  echo "Previous version saved in: $BACKUP_DIR"
fi
mv "$STAGE_DIR/$PLUGIN_NAME" "$PLUGIN_DIR"
echo "Installed SilkHaptic for $GAME_NAME."
echo 'Connect the DualSense by USB or macOS Bluetooth, then launch the game with BepInEx.'
