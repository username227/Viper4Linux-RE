#!/usr/bin/env bash
# Non-destructive Arch staging parity: compare CMake's DESTDIR output against
# the established installer, including paths, file bytes, modes and symlinks.
# Usage: bash tests/check-cmake-install-parity.sh BUILD_DIRECTORY
set -Eeuo pipefail
if (( $# != 1 )); then
    echo 'Usage: tests/check-cmake-install-parity.sh BUILD_DIRECTORY' >&2
    exit 2
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
built=$(cd -- "$1" && pwd -P)
[[ -f "$built/CMakeCache.txt" ]] || { echo 'Missing CMake build cache.' >&2; exit 1; }
grep -Fxq 'VIPER_ENABLE_SYSTEM_INSTALL:BOOL=ON' "$built/CMakeCache.txt" || {
    echo 'System staging option is not enabled in the build.' >&2; exit 1;
}
grep -Fxq 'CMAKE_INSTALL_PREFIX:PATH=/usr' "$built/CMakeCache.txt" || {
    echo 'System staging must use /usr.' >&2; exit 1;
}
for file in viper-gui viper-ctl viper-output-state viper_dsp_ladspa.so; do
    [[ -s "$built/$file" ]] || { echo "Missing compiled file: $file" >&2; exit 1; }
done

work=$(mktemp -d /tmp/viper-stage-parity-XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
old="$work/original"
new="$work/cmake"
mkdir -p -- "$old" "$new"

bash "$root/packaging/arch/install-package.sh" "$root" "$built" "$old" viper4linux-re
DESTDIR="$new" cmake --install "$built"

python3 - "$old" "$new" <<'PY'
import hashlib
import os
from pathlib import Path
import stat
import sys

def staged_manifest(directory):
    root = Path(directory)
    result = {}
    for path in root.rglob('*'):
        name = path.relative_to(root).as_posix()
        mode = stat.S_IMODE(path.lstat().st_mode)
        if path.is_symlink():
            result[name] = ('symlink', mode, os.readlink(path))
        elif path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            result[name] = ('file', mode, digest)
        elif path.is_dir():
            result[name] = ('directory', mode)
        else:
            result[name] = ('unknown', mode)
    return result

original = staged_manifest(sys.argv[1])
cmake = staged_manifest(sys.argv[2])
if original != cmake:
    diff = sorted(set(original) | set(cmake))
    mismatches = [path for path in diff if original.get(path) != cmake.get(path)]
    print(f'FAIL: {len(mismatches)} Arch staging differences:', file=sys.stderr)
    for path in mismatches[:40]:
        print(f'  {path}', file=sys.stderr)
        print(f'    old: {original.get(path)}', file=sys.stderr)
        print(f'    new: {cmake.get(path)}', file=sys.stderr)
    sys.exit(1)
files = sum(kind[0] == 'file' for kind in cmake.values())
links = sum(kind[0] == 'symlink' for kind in cmake.values())
print(f'PASS: exact Arch staging parity: {files} files, {links} symlinks, '
      'matching SHA-256 data, permissions and directory tree')
PY
