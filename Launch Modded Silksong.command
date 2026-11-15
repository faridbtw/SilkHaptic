#!/bin/bash
set -euo pipefail
GAME_DIR="${1:-${SILKSONG_GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}}"
cd "$GAME_DIR"
if [[ ! -f run_bepinex.sh ]]; then echo 'BepInEx launcher not found.'; exit 1; fi
ARCHS="$(lipo -archs libdoorstop.dylib)"
if [[ "$(uname -m)" == arm64 && "$ARCHS" != *arm64* ]]; then
  exec /usr/bin/arch -x86_64 /bin/bash ./run_bepinex.sh
fi
exec /bin/bash ./run_bepinex.sh
