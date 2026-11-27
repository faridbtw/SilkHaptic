#!/usr/bin/env python3
"""Package compiled SilkHaptic without game assets, dependencies, or local logs."""
from pathlib import Path
import hashlib
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist'
SOURCE_FILES = ['README.md', 'LICENSE', 'PROTOCOL.md', 'VALIDATION.txt', 'build.sh',
                'Install.command', 'Uninstall.command', 'Test Haptics.command',
                'Test Bluetooth Haptics.command', 'Launch Modded Silksong.command',
                'Install Hollow Knight.command', 'Uninstall Hollow Knight.command',
                'Launch Modded Hollow Knight.command', '.gitignore']
REQUIRED = ['BepInEx/plugins/SilkSoftMac/SilksongPS5Haptics.dll',
            'BepInEx/plugins/SilkSoftMac/HapticsBridge/HapticsBridge',
            'Extractor/osx-arm64/Extractor', 'Extractor/osx-x64/Extractor',
            'HollowKnight/BepInEx/plugins/SilkHaptic/HollowKnightHaptics.dll',
            'HollowKnight/BepInEx/plugins/SilkHaptic/HapticsBridge/HapticsBridge']
for name in REQUIRED:
    if not (DIST / name).is_file():
        raise SystemExit(f'Missing {name}; run ./build.sh first.')
source = DIST / 'Source'
source.mkdir(exist_ok=True)
for name in SOURCE_FILES:
    shutil.copy2(ROOT / name, source / name)
for name in ['src', 'ThirdParty', 'assets', 'docs']:
    shutil.copytree(ROOT / name, source / name, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('bin', 'obj', '*.log', '.DS_Store'))
(source / 'tools').mkdir(exist_ok=True)
for name in ['verify_bridge.py', 'verify_bluetooth.py', 'package_release.py']:
    shutil.copy2(ROOT / 'tools' / name, source / 'tools' / name)
files = sorted(p for p in DIST.rglob('*') if p.is_file())
for file in files:
    if file.suffix.lower() in {'.wav', '.bundle', '.log', '.pdb'} or file.name == '.DS_Store':
        raise SystemExit(f'Refusing to package local asset or log: {file}')
    if file.suffix.lower() == '.dll' and file.name not in {'SilksongPS5Haptics.dll', 'HollowKnightHaptics.dll'}:
        raise SystemExit(f'Unexpected assembly: {file}')
releases = ROOT / 'releases'
releases.mkdir(exist_ok=True)
plugin_source = (ROOT / 'src/Plugin/Plugin.cs').read_text()
version = re.search(r'public const string Version = "([0-9]+\.[0-9]+\.[0-9]+)";', plugin_source)
if not version:
    raise SystemExit('Could not find the version in src/Plugin/Plugin.cs.')
archive = releases / f'SilkHaptic-macOS-v{version[1]}.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for file in files:
        z.write(file, Path('SilkHaptic-macOS') / file.relative_to(DIST))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
print(archive)
print(f'SHA256: {checksum}')
