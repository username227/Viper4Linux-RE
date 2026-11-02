#!/usr/bin/env bash
# Build a native, distro-specific .deb from the already published unified source.
# Run in a pristine Ubuntu/Debian container, not on the host user's system.
set -Eeuo pipefail
if [[ $# != 4 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo 'Usage: build-native-deb.sh VERSION TARGET ARCHIVE OUTPUT_DIR' >&2
    exit 2
fi
version=$1 target=$2 archive=$3 out=$4
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
# Apply downstream fixes to a private copy, matching the stable Arch build.
dsp="$work/ViPERDSP-patched"
cp -a -- "$tree/third_party/ViPERDSP" "$dsp"
VIPERDSP_SOURCE_DIR="$dsp" python3 "$tree/patches/fix_lufs_speed.py"
VIPERDSP_SOURCE_DIR="$dsp" python3 "$tree/patches/preallocate_stereo_imager.py"
VIPERDSP_SOURCE_DIR="$dsp" python3 "$tree/patches/preallocate_multiband_compressor.py"
built="$work/build"
cmake -S "$tree" -B "$built" -DCMAKE_BUILD_TYPE=Release -DVIPERDSP_SOURCE_DIR="$dsp"
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
# Keep installation paths identical to the working Arch package and launcher.
install -Dm755 "$built/viper-gui" "$pkgroot/usr/bin/viper4linux-gui"
install -Dm755 "$built/viper-ctl" "$pkgroot/usr/bin/viper4linux-ctl"
install -Dm755 "$built/viper-output-state" "$pkgroot/usr/bin/viper4linux-output-state"
install -Dm755 "$tree/packaging/arch/viper4linux-output" "$pkgroot/usr/bin/viper4linux-output"
ln -s -- viper4linux-ctl "$pkgroot/usr/bin/v4l-cli"
install -Dm755 "$built/viper_dsp_ladspa.so" "$pkgroot/usr/lib/ladspa/viper_dsp_ladspa.so"
install -Dm644 "$tree/src/viper_launcher.py" "$pkgroot/usr/lib/viper4linux/viper_launcher.py"
for module in "$tree"/src/viper_*.py; do
    install -Dm644 "$module" "$pkgroot/usr/lib/viper4linux/${module##*/}"
done
install -Dm755 "$tree/packaging/arch/start-backend" "$pkgroot/usr/lib/viper4linux/start-backend"
install -Dm755 "$tree/packaging/arch/viper4linux" "$pkgroot/usr/bin/viper4linux"
install -Dm644 "$tree/routing-config/filter-chain.conf.d/99-viper-smart.conf" \
    "$pkgroot/usr/share/viper4linux/routing-config/filter-chain.conf.d/99-viper-smart.conf"
install -Dm644 "$tree/packaging/arch/viper4linux.desktop" \
    "$pkgroot/usr/share/applications/viper4linux.desktop"
install -Dm644 "$tree/resources/viper4linux-blue.png" \
    "$pkgroot/usr/share/icons/hicolor/256x256/apps/viper4linux.png"
install -Dm644 "$tree/resources/viper4linux-blue.svg" \
    "$pkgroot/usr/share/icons/hicolor/scalable/apps/viper4linux.svg"
install -Dm644 "$tree/man/viper4linux.1" "$pkgroot/usr/share/man/man1/viper4linux.1"
install -Dm644 "$tree/man/viper4linux-ctl.1" "$pkgroot/usr/share/man/man1/viper4linux-ctl.1"
gzip -n -9 "$pkgroot/usr/share/man/man1/"*.1
ln -s -- viper4linux-ctl.1.gz "$pkgroot/usr/share/man/man1/v4l-cli.1.gz"
install -Dm644 "$tree/README.md" "$pkgroot/usr/share/doc/viper4linux-re/README.md"
install -Dm644 "$tree/LICENSE" "$pkgroot/usr/share/doc/viper4linux-re/LICENSE"
install -Dm644 "$tree/THIRD-PARTY-NOTICES.md" \
    "$pkgroot/usr/share/doc/viper4linux-re/THIRD-PARTY-NOTICES.md"
cp -a "$tree/docs" "$pkgroot/usr/share/doc/viper4linux-re/docs"
# Debian copyright file is package-specific; our MIT license doesn't relicense DSP.
{
    printf 'ViPER for Linux RE original code — MIT license.\n'
    printf 'ViPERDSP has separate terms; see THIRD-PARTY-NOTICES.md.\n\n'
    cat "$tree/LICENSE"
} > "$pkgroot/usr/share/doc/viper4linux-re/copyright"
chmod 644 "$pkgroot/usr/share/doc/viper4linux-re/copyright"
# Let Debian's actual shlibs database determine Qt/C++ ABI dependencies.
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
# Exercise dependency resolution and the installed command-line/launcher paths.
apt-get install -y --no-install-recommends "$package"
viper4linux-ctl --help >/dev/null
v4l-cli --help >/dev/null
python3 /usr/lib/viper4linux/viper_launcher.py --fingerprint >/dev/null
(cd "$out" && sha256sum -- "$(basename "$package")" > SHA256SUMS && sha256sum --check SHA256SUMS)
echo "PASS: $target native Debian package built, installed, and smoke-tested: $package"
