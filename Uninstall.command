#!/bin/bash
set -euo pipefail
case "${SILKHAPTIC_GAME:-silksong}" in
  hollow-knight)
    DEFAULT_DIR="${HOLLOW_KNIGHT_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight}"
    GAME_NAME="Hollow Knight"; PLUGIN_NAME="SilkHaptic" ;;
  silksong)
    DEFAULT_DIR="${SILKSONG_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}"
    GAME_NAME="Hollow Knight Silksong"; PLUGIN_NAME="SilkSoftMac" ;;
  *) echo 'Unknown game.'; exit 1 ;;
esac
GAME_DIR="${1:-$DEFAULT_DIR}"
if pgrep -x "$GAME_NAME" >/dev/null; then echo "Quit $GAME_NAME before uninstalling."; exit 1; fi
PLUGIN_DIR="$GAME_DIR/BepInEx/plugins/$PLUGIN_NAME"
if [[ -d "$PLUGIN_DIR" ]]; then
  BACKUP_DIR="$GAME_DIR/BepInEx/$PLUGIN_NAME-backups/uninstalled-$(date +%Y%m%d-%H%M%S)-$$"
  mkdir -p "$BACKUP_DIR"
  mv "$PLUGIN_DIR" "$BACKUP_DIR/$PLUGIN_NAME"
  echo "Uninstalled SilkHaptic for $GAME_NAME. Files preserved in: $BACKUP_DIR"
else
  echo "SilkHaptic is not installed for $GAME_NAME."
fi
