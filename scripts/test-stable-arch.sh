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
pacman -Syu --noconfirm --needed base-devel cmake pkgconf qt6-base pipewire ladspa python git
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
# makepkg runs the 12 fast package CTests. Enable the full developer suite
# afterwards in the same build tree, without recompiling the production app.
source_tree="$release_dir/arch-build/src/viper4linux-re-$version"
build_tree="$release_dir/arch-build/src/viper4linux-build"
test -f "$source_tree/tests/CMakeLists.txt"
# Unsafe DESTDIR tests run ONLY inside this disposable Arch Docker job,
# against the real CMake install script produced by makepkg.
VIPER_CI_STAGING_GUARD_TEST=1 bash "$source_tree/tests/check-cmake-install-guard.sh" \
    "$build_tree"
runuser -u builder -- bash -c '
    set -Eeuo pipefail
    cmake -S "$1" -B "$2" -DVIPER_BUILD_DEVELOPER_TESTS=ON
    cmake --build "$2"
    ctest --test-dir "$2" --output-on-failure --no-tests=error
' bash "$source_tree" "$build_tree"

# Independently exercise the *normal source-checkout* CMake defaults from the
# same pinned source archive. No package install or PipeWire audio is started.
runuser -u builder -- bash -c '
        set -Eeuo pipefail
        source_tree=$1
        cmake -S "$source_tree" -B "$source_tree/build" \
            -DCMAKE_BUILD_TYPE=Release \
            -DVIPER_BUILD_DEVELOPER_TESTS=OFF
        cmake --build "$source_tree/build" --target viper_source_check
        test -r "$source_tree/build/routing-config/filter-chain.conf"
        test -r "$source_tree/build/routing-config/filter-chain.conf.d/99-viper-smart.conf"
        (cd "$source_tree" && python3 src/viper_launcher.py --fingerprint >/dev/null)
        echo "PASS: real CMake source-checkout build, routing setup and backend fingerprint"
    ' bash "$source_tree"

# Verify repeatable CMake staging from the native compiled package build.
# Only private temporary DESTDIR trees are used; never install on this host.
bash "$source_tree/tests/check-cmake-install-parity.sh" \
    "$release_dir/arch-build/src/viper4linux-build"

# Only the main application package is a release asset. makepkg may also
# produce a debug-symbol package, which is deliberately not distributed here.
shopt -s nullglob
main_packages=("$release_dir"/arch-build/viper4linux-re-"${version}"-*.pkg.tar.zst)
(( ${#main_packages[@]} == 1 )) || {
    echo "Expected exactly one main Arch package; found ${#main_packages[@]}." >&2
    exit 1
}
package=${main_packages[0]}
pacman -Qip "$package"
listing=$(pacman -Qlp "$package")
for installed in usr/bin/viper4linux usr/bin/viper4linux-gui \
                 usr/bin/viper4linux-ctl usr/lib/ladspa/viper_dsp_ladspa.so \
                 usr/share/man/man1/viper4linux.1 \
                 usr/share/licenses/viper4linux-re/ViPERDSP-NOTICE.md; do
    grep -Fq "$installed" <<< "$listing" || {
        echo "Missing expected package entry: $installed" >&2; exit 1;
    }
done
# Stage only the tested application package, never the optional debug symbols.
name=$(basename -- "$package")
install -m644 -- "$package" "$release_dir/$name"
(
    cd "$release_dir"
    sha256sum -- "$name" > ARCH-SHA256SUMS
    sha256sum --check ARCH-SHA256SUMS
)
echo "PASS: Built verified stable Arch package from local unified archive: $package"
echo "PASS: Staged main Arch package and ARCH-SHA256SUMS for release review."
