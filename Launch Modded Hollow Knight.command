#!/bin/bash
set -euo pipefail
GAME_DIR="${1:-${HOLLOW_KNIGHT_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight}}"
cd "$GAME_DIR"
# Steam's API also needs the app identity when launched outside its Play button.
export SteamAppId="${SteamAppId:-367520}"
export SteamGameId="${SteamGameId:-367520}"
if [[ ! -f run_bepinex.sh ]]; then echo 'BepInEx launcher not found.'; exit 1; fi
if [[ "$(uname -m)" == arm64 ]]; then
  exec /usr/bin/arch -x86_64 /bin/bash ./run_bepinex.sh
fi
exec /bin/bash ./run_bepinex.sh
