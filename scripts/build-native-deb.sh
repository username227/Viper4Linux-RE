#!/usr/bin/env bash
# Build a native, distro-specific .deb from the already published unified source.
# Run in a pristine Ubuntu/Debian container, not on the host user's system.
set -Eeuo pipefail
if [[ $# != 4 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo 'Usage: build-native-deb.sh VERSION TARGET ARCHIVE OUTPUT_DIR' >&2
    exit 2
fi
version=$1 target=$2 archive=$3 out=$4
# Native .deb packaging owns only distro dependencies, metadata and archive
# assembly. CMake owns all DSP preparation and installed application files.
# Build-only CI must not install the generated application package, even
# inside its disposable distro container. Default release behavior is retained.
skip_install=${VIPER_DEB_SKIP_INSTALL_TEST:-0}
case "$skip_install" in
    0|1) ;;
    *) echo "Unsupported VIPER_DEB_SKIP_INSTALL_TEST: $skip_install" >&2; exit 2 ;;
esac
case "$target" in
    ubuntu24.04-mint22)
        expected_os=ubuntu expected_version=24.04
        revision='1~ubuntu24.04mint22'
        ;;
    debian13-lmde7)
        expected_os=debian expected_version=13
        revision='1~debian13lmde7'
        ;;
    *) echo "Unsupported target: $target" >&2; exit 2 ;;
esac
# A build for one distribution must not silently run on another.
. /etc/os-release
[[ ${ID:-} == "$expected_os" && ${VERSION_ID:-} == "$expected_version" ]] || {
    echo "Wrong base OS for $target: actual ${ID:-unknown} ${VERSION_ID:-unknown}" >&2
    exit 1
}
[[ -f $archive ]] || { echo "Source archive missing: $archive" >&2; exit 1; }
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd -P)
archive=$(realpath -- "$archive")
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config qt6-base-dev ladspa-sdk \
    python3 pipewire binutils dpkg-dev ca-certificates xz-utils
work=$(mktemp -d /tmp/viper4linux-deb-XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
tar -xJf "$archive" -C "$work"
tree="$work/viper4linux-re-$version"
[[ -f "$tree/CMakeLists.txt" && -f "$tree/third_party/ViPERDSP/CMakeLists.txt" ]] || {
    echo 'Unified source archive does not include the pinned ViPERDSP source.' >&2; exit 1;
}
built="$work/build"
cmake -S "$tree" -B "$built" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVIPER_ENABLE_SYSTEM_INSTALL=ON \
    -DVIPER_SYSTEM_INSTALL_LAYOUT=debian
cmake --build "$built" --parallel 3
for program in viper-gui viper-ctl viper_dsp_ladspa.so; do
    [[ -s "$built/$program" ]] || { echo "Missing compiled file: $program" >&2; exit 1; }
done
nm -D "$built/viper_dsp_ladspa.so" | grep -Eq ' [TW] ladspa_descriptor$'
(
    cd "$tree"
    bash tests/check-mock.sh
    python3 tests/check-launcher.py
    python3 tests/check-theme-contract.py
    python3 tests/check-ui-contract.py
    python3 tests/check-tray-contract.py
    python3 tests/check-named-presets-contract.py
    python3 tests/check-preset-reset-contract.py
)
pkgroot="$work/package"
mkdir -p "$pkgroot/DEBIAN"
# All native application payload paths and Debian documentation are supplied
# by the same CMake installation rules used by Arch; no manual file copy list.
DESTDIR="$pkgroot" cmake --install "$built"# Let Debian's actual shlibs database determine Qt/C++ ABI dependencies.
# In particular Noble has libqt6widgets6t64; Debian 13 has libqt6widgets6.
mkdir -p "$work/dpkg-shlibdeps/debian"
cat > "$work/dpkg-shlibdeps/debian/control" <<'CONTROL'
Source: viper4linux-re
Section: sound
Priority: optional
Maintainer: ViPER for Linux contributors <noreply@github.com>
Standards-Version: 4.7.0

Package: viper4linux-re
Architecture: any
Description: Native ViPERDSP audio processor for PipeWire
CONTROL
shlib_output=$(cd "$work/dpkg-shlibdeps" && dpkg-shlibdeps -O \
    -e"$pkgroot/usr/bin/viper4linux-gui" \
    -e"$pkgroot/usr/bin/viper4linux-ctl" \
    -e"$pkgroot/usr/lib/ladspa/viper_dsp_ladspa.so")
shlib_depends=${shlib_output#shlibs:Depends=}
[[ $shlib_depends != "$shlib_output" && -n $shlib_depends ]] || {
    echo "Failed to derive runtime shared-library dependencies: $shlib_output" >&2
    exit 1
}
package_version="$version-$revision"
cat > "$pkgroot/DEBIAN/control" <<CONTROL
Package: viper4linux-re
Version: $package_version
Section: sound
Priority: optional
Architecture: amd64
Maintainer: ViPER for Linux contributors <noreply@github.com>
Depends: $shlib_depends, pipewire, python3, qt6-qpa-plugins
Homepage: https://github.com/username227/viper4linux-re
Description: Native ViPERDSP audio effects for PipeWire
 Native Qt6 interface, independent PipeWire DSP backend, presets and
 automatic following of the default output device.
 This build targets $target.
CONTROL
package="$out/viper4linux-re_${package_version}_amd64.deb"
dpkg-deb --build --root-owner-group "$pkgroot" "$package"
dpkg-deb --info "$package"
dpkg-deb --contents "$package" | grep -F 'usr/lib/ladspa/viper_dsp_ladspa.so'
if [[ $skip_install == 0 ]]; then
    # Release-path behavior: verify package dependency resolution and installed
    # command-line/launcher paths in a disposable distribution container.
    apt-get install -y --no-install-recommends "$package"
    viper4linux-ctl --help >/dev/null
    v4l-cli --help >/dev/null
    python3 /usr/lib/viper4linux/viper_launcher.py --fingerprint >/dev/null
fi
(cd "$out" && sha256sum -- "$(basename "$package")" > SHA256SUMS && sha256sum --check SHA256SUMS)
if [[ $skip_install == 1 ]]; then
    echo "PASS: $target native Debian package built and inspected; package NOT installed: $package"
else
    echo "PASS: $target native Debian package built, installed, and smoke-tested: $package"
fi
