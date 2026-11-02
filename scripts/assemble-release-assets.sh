#!/usr/bin/env bash
# Combine verified native release artifacts into a single, reviewable ZIP input.
# Never create a Git tag, publish assets or update a package repository.
set -Eeuo pipefail
if [[ $# != 4 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ || ! $2 =~ ^[0-9a-f]{40}$ ]]; then
    echo 'Usage: assemble-release-assets.sh VERSION COMMIT_SHA DOWNLOADED_ARTIFACTS OUTPUT_DIR' >&2
    exit 2
fi
version=$1 commit=$2 collected=$3 out=$4
root="$collected/viper4linux-re-${version}-release-candidate"
test -f "$root/SOURCE-COMMIT"
test "$(cat "$root/SOURCE-COMMIT")" = "$commit" || {
    echo 'Source commit does not match the workflow revision.' >&2
    exit 1
}
mkdir -p -- "$out"
cp -a -- "$root/." "$out/"
(cd "$out" && sha256sum --check SHA256SUMS && sha256sum --check ARCH-SHA256SUMS)
test -s "$out/.SRCINFO" && test -s "$out/PKGBUILD"
archive="viper4linux-re-${version}-unified-source.tar.xz"
tar -xOf "$out/$archive" "viper4linux-re-${version}/SOURCE-MANIFEST.txt" \
    | grep -Fx "Application commit: $commit"
for target in ubuntu24.04-mint22 debian13-lmde7; do
    input="$collected/viper4linux-re-${version}-${target}-deb"
    (cd "$input" && sha256sum --check SHA256SUMS)
    shopt -s nullglob
    debs=("$input"/*.deb)
    test "${#debs[@]}" -eq 1 || { echo "Expected exactly one .deb for $target" >&2; exit 1; }
    case "$target" in
        ubuntu24.04-mint22) suffix='1~ubuntu24.04mint22' ;;
        debian13-lmde7) suffix='1~debian13lmde7' ;;
    esac
    test "$(basename "${debs[0]}")" = "viper4linux-re_${version}-${suffix}_amd64.deb" || {
        echo "Unexpected Debian package: ${debs[0]}" >&2
        exit 1
    }
    cp -- "${debs[0]}" "$out/"
    cp -- "$input/SHA256SUMS" "$out/DEB-SHA256SUMS-${target}"
    (cd "$out" && sha256sum --check "DEB-SHA256SUMS-${target}")
done
main_packages=("$out"/viper4linux-re-"$version"-*.pkg.tar.zst)
test "${#main_packages[@]}" -eq 1 || {
    echo 'Expected exactly one Arch package.' >&2
    exit 1
}
test "$(find "$out" -maxdepth 1 -name '*.deb' | wc -l)" -eq 2
! find "$out" -maxdepth 1 -name '*debug*.pkg.tar.*' | grep -q . || {
    echo 'Debug-symbol package must not be distributed.' >&2
    exit 1
}
echo 'PASS: unified source, Arch and two .deb packages, hashes and commit all verified.'
