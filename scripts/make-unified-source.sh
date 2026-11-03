#!/usr/bin/env bash
# Export a tagged source revision and its pinned ViPERDSP submodule as a single
# offline-buildable archive. This is a release asset, NOT GitHub's default zip.
set -Eeuo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo 'Usage: scripts/make-unified-source.sh VERSION [OUTPUT_DIRECTORY]' >&2
    exit 2
fi
version=$1
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][a-zA-Z0-9.-]+)?$ ]]; then
    echo 'VERSION must be a release version such as 1.0.0' >&2
    exit 2
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
outdir=${2:-$PWD}
mkdir -p -- "$outdir"
outdir=$(cd -- "$outdir" && pwd -P)
archive="$outdir/viper4linux-re-${version}-unified-source.tar.xz"
[[ ! -e $archive ]] || { echo "Refusing to overwrite existing archive: $archive" >&2; exit 1; }

for executable in git tar mktemp; do
    command -v "$executable" >/dev/null || { echo "Missing $executable" >&2; exit 1; }
done
# This utility is run from the committed source, ideally at a release tag.
git -C "$root" rev-parse --is-inside-work-tree | grep -qx true || {
    echo 'Run this from a Git source checkout, not a source-only archive.' >&2
    exit 1
}
[[ -z $(git -C "$root" status --porcelain --untracked-files=no --ignore-submodules=none) ]] || {
    echo 'Source checkout or submodule contains changes. Commit/revert before exporting.' >&2
    exit 1
}
upstream_path='third_party/ViPERDSP'
expected=$(git -C "$root" rev-parse "HEAD:$upstream_path")
entry=$(git -C "$root" ls-tree HEAD -- "$upstream_path")
[[ $entry == "160000 commit $expected"$'\t'"$upstream_path" ]] || {
    echo 'ViPERDSP is not recorded as a Git submodule.' >&2; exit 1;
}
[[ -f "$root/$upstream_path/CMakeLists.txt" ]] || {
    echo 'Missing submodule sources. Run git submodule update --init first.' >&2; exit 1;
}
actual=$(git -C "$root/$upstream_path" rev-parse HEAD)
[[ $actual == "$expected" ]] || {
    echo "ViPERDSP revision mismatch: recorded $expected, checked out $actual" >&2; exit 1;
}

revision=$(git -C "$root" rev-parse --short=12 HEAD)
release_dir="viper4linux-re-$version"
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
mkdir -p -- "$stage/$release_dir"
# Only committed files are exported, never untracked development artifacts.
git -C "$root" archive --format=tar HEAD | tar -xf - -C "$stage/$release_dir"
mkdir -p -- "$stage/$release_dir/$upstream_path"
git -C "$root/$upstream_path" archive --format=tar "$expected" \
    | tar -xf - -C "$stage/$release_dir/$upstream_path"
# A unified release has real dependency sources, not an unresolved Git link.
rm -f -- "$stage/$release_dir/.gitmodules"
printf '%s\n' "$version" > "$stage/$release_dir/RELEASE-VERSION"
printf '%s\n' "$revision" > "$stage/$release_dir/RELEASE-REVISION"
printf 'ViPERDSP repository: https://github.com/likelikeslike/ViPERDSP.git\nViPERDSP commit: %s\nApplication commit: %s\nDownstream patches: present but not applied; apply at build time to a private copy\n' \
    "$expected" "$(git -C "$root" rev-parse HEAD)" > "$stage/$release_dir/SOURCE-MANIFEST.txt"
# Reproducible timestamps/ownership independent of the packager's machine.
mtime=${SOURCE_DATE_EPOCH:-$(git -C "$root" show -s --format=%ct HEAD)}
[[ $mtime =~ ^[0-9]+$ ]] || { echo 'Invalid SOURCE_DATE_EPOCH' >&2; exit 1; }
(
    cd -- "$stage"
    tar --sort=name --mtime="@$mtime" --owner=0 --group=0 --numeric-owner \
        -cJf "$archive" "$release_dir"
)
echo "Created $archive"
echo "Application $revision; pinned ViPERDSP $expected"
