#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
GAME_DIR="${1:-$HOME/Library/Application Support/Steam/steamapps/common/Hollow Knight Silksong}"
MANAGED="$GAME_DIR/Hollow Knight Silksong.app/Contents/Resources/Data/Managed"
PLUGIN="dist/BepInEx/plugins/SilkSoftMac"
mkdir -p "$PLUGIN/HapticsBridge" dist/Extractor
CSC="${CSC:-$(command -v csc || true)}"
if [[ -z "$CSC" ]]; then echo 'Build requires Mono csc (brew install mono).'; exit 1; fi
"$CSC" -nologo -target:library -optimize+ -out:"$PLUGIN/SilksongPS5Haptics.dll" \
  -r:"$GAME_DIR/BepInEx/core/BepInEx.dll" -r:"$GAME_DIR/BepInEx/core/0Harmony.dll" \
  -r:"$MANAGED/Assembly-CSharp.dll" -r:"$MANAGED/UnityEngine.dll" \
  -r:"$MANAGED/UnityEngine.CoreModule.dll" -r:"$MANAGED/UnityEngine.AudioModule.dll" \
  -r:"$MANAGED/netstandard.dll" src/Plugin/*.cs
xcrun clang -std=c11 -O2 -Wall -Wextra -Werror -arch arm64 -arch x86_64 \
  -mmacosx-version-min=11.0 -framework AudioUnit -framework CoreAudio \
  -framework CoreFoundation -framework IOKit -lz src/Bridge/HapticsBridge.c src/Bridge/BluetoothHaptics.c \
  -o "$PLUGIN/HapticsBridge/HapticsBridge"
codesign --force --sign - "$PLUGIN/HapticsBridge/HapticsBridge"
DOTNET="${DOTNET:-$(command -v dotnet || true)}"
DOTNET="${DOTNET:-$PWD/tools/dotnet/dotnet}"
if [[ ! -x "$DOTNET" ]]; then echo "Build requires the .NET 8 SDK (set DOTNET or add dotnet to PATH)."; exit 1; fi
for RID in osx-arm64 osx-x64; do
  DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DOTNET" publish src/Extractor/Extractor.csproj -c Release \
    -r "$RID" --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true \
    -p:IncludeNativeLibrariesForSelfExtract=true -o "dist/Extractor/$RID" --nologo
  rm -f "dist/Extractor/$RID/Extractor.pdb"
done
cp LICENSE dist/LICENSE
cp README.md dist/README.md
cp PROTOCOL.md dist/PROTOCOL.md
cp VALIDATION.txt dist/VALIDATION.txt
cp -R assets docs dist/
mkdir -p dist/ThirdParty
cp ThirdParty/* dist/ThirdParty/
cp Install.command Uninstall.command 'Test Haptics.command' 'Test Bluetooth Haptics.command' 'Launch Modded Silksong.command' dist/
echo 'Built dist/ (no game assets included).'
