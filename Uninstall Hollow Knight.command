#!/bin/bash
set -euo pipefail
PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
export SILKHAPTIC_GAME=hollow-knight
exec "$PACKAGE_DIR/Uninstall.command" "$@"
