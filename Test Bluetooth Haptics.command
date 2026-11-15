#!/bin/bash
set -euo pipefail
PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PACKAGE_DIR"
exec "$PACKAGE_DIR/BepInEx/plugins/SilkSoftMac/HapticsBridge/HapticsBridge" --port 48121 --transport bluetooth --test
