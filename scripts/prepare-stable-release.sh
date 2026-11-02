#!/usr/bin/env bash
# Produce reviewable stable-release source and packaging assets, but never publish.
set -Eeuo pipefail
if [[ $# -ne 2 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo 'Usage: scripts/prepare-stable-release.sh VERSION OUTPUT_DIRECTORY' >&2
    exit 2
fi
version=$1
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
mkdir -p -- "$2"
out=$(cd -- "$2" && pwd -P)

# If the release tag exists, it must refer to the checked-out source revision.
if git -C "$root" rev-parse -q --verify "refs/tags/v${version}^{commit}" >/dev/null; then
    tag_commit=$(git -C "$root" rev-parse "refs/tags/v${version}^{commit}")
    [[ $tag_commit == "$(git -C "$root" rev-parse HEAD)" ]] || {
        echo "v${version} refers to a different commit; refusing to build a mislabeled release" >&2
        exit 1
    }
fi

bash "$root/scripts/make-unified-source.sh" "$version" "$out"
archive="$out/viper4linux-re-${version}-unified-source.tar.xz"
python3 "$root/scripts/render-stable-pkgbuild.py" "$version" "$archive" \
    "$root/packaging/arch/PKGBUILD.stable.in" "$out/PKGBUILD" > "$out/SHA256SUMS"
(cd "$out" && sha256sum -c SHA256SUMS)
git -C "$root" rev-parse HEAD > "$out/SOURCE-COMMIT"
python3 "$root/tests/check-release-artifacts.py" "$version" "$out"
echo "Release assets prepared under $out; no tag or release was published."
