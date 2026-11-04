#!/usr/bin/env bash
# Compatibility entry point for independently maintained AUR -git recipes.
# This script never copies application files; CMake is the only installer.
set -Eeuo pipefail

if (( $# != 4 )); then
    echo 'Usage: install-package.sh SOURCE_TREE BUILD_DIR PKGDIR PKGNAME' >&2
    exit 2
fi
source_tree=$(cd -- "$1" && pwd -P)
build_dir=$(cd -- "$2" && pwd -P)
pkgdir=$3
pkgname=$4
if [[ $pkgdir != /* || $pkgdir == / || $pkgdir == /usr || $pkgdir == /usr/ ||
      $pkgdir == "$source_tree" || $pkgdir == "$build_dir" ]]; then
    echo 'Destination must be an absolute staging directory, never a system/source/build directory.' >&2
    exit 2
fi
if [[ ! $pkgname =~ ^[a-z0-9][a-z0-9@._+-]*$ ]]; then
    echo "Invalid package name: $pkgname" >&2
    exit 2
fi
[[ -f "$build_dir/CMakeCache.txt" ]] || {
    echo "Existing CMake build directory required: $build_dir" >&2
    exit 1
}
# Older AUR PKGBUILDs configure the build without install rules. Keep their
# pinned DSP source cache and outputs; enable CMake packaging in-place.
cmake -S "$source_tree" -B "$build_dir" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVIPER_ENABLE_SYSTEM_INSTALL=ON \
    -DVIPER_SYSTEM_INSTALL_LAYOUT=arch \
    -DVIPER_SYSTEM_PACKAGE_NAME="$pkgname"
mkdir -p -- "$pkgdir"
DESTDIR="$pkgdir" cmake --install "$build_dir"
