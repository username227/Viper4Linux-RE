#!/usr/bin/env bash
# Run inside the official archlinux container with the checkout mounted at /workspace.
set -Eeuo pipefail
if [[ $# -ne 2 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo 'Usage: scripts/test-stable-arch.sh VERSION RELEASE_DIRECTORY' >&2
    exit 2
fi
version=$1
release_dir=$2
pkg="$release_dir/PKGBUILD"
archive="$release_dir/viper4linux-re-${version}-unified-source.tar.xz"
test -f "$pkg" && test -f "$archive"

# Only dependencies come from Arch repos. makepkg reuses the local source archive.
pacman -Syu --noconfirm --needed base-devel cmake pkgconf qt6-base pipewire ladspa python
id -u builder >/dev/null 2>&1 || useradd -m builder
mkdir -p "$release_dir/arch-build"
cp "$pkg" "$release_dir/arch-build/PKGBUILD"
chown -R builder:builder "$release_dir"

runuser -u builder -- env SRCDEST="$release_dir" bash -c '
    set -Eeuo pipefail
    cd "$1/arch-build"
    makepkg --noconfirm --cleanbuild --nodeps
    makepkg --printsrcinfo > "$1/.SRCINFO"
' bash "$release_dir"
package=$(find "$release_dir/arch-build" -maxdepth 1 -name "viper4linux-re-${version}-*.pkg.tar.*" -print -quit)
[[ -f $package ]] || { echo 'No Arch package was generated.' >&2; exit 1; }
pacman -Qip "$package"
listing=$(pacman -Qlp "$package")
for installed in usr/bin/viper4linux usr/bin/viper4linux-gui \
                 usr/bin/viper4linux-ctl usr/lib/ladspa/viper_dsp_ladspa.so \
                 usr/share/man/man1/viper4linux.1 \
                 usr/share/licenses/viper4linux-re/LICENSE; do
    grep -Fq "$installed" <<< "$listing" || {
        echo "Missing expected package entry: $installed" >&2; exit 1;
    }
done
echo "PASS: Built verified stable Arch package from local unified archive: $package"
