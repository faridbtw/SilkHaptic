#!/bin/bash
set -euo pipefail
GAME_DIR="${1:-${SILKSONG_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}}"
if pgrep -x 'Hollow Knight Silksong' >/dev/null; then echo 'Quit Silksong before uninstalling.'; exit 1; fi
PLUGIN_DIR="$GAME_DIR/BepInEx/plugins/SilkSoftMac"
if [[ -d "$PLUGIN_DIR" ]]; then
  BACKUP_DIR="$GAME_DIR/BepInEx/SilkSoftMac-backups/uninstalled-$(date +%Y%m%d-%H%M%S)-$$"
  mkdir -p "$BACKUP_DIR"
  mv "$PLUGIN_DIR" "$BACKUP_DIR/SilkSoftMac"
  echo "Uninstalled. Files preserved in: $BACKUP_DIR"
else
  echo 'SilkSoftMac is not installed.'
fi
