#!/usr/bin/env bash
# Prepare an unsigned Launchpad source upload from the EXACT published unified
# source archive. No publication and no host application installation.
set -Eeuo pipefail
if [[ $# -ne 4 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo 'Usage: prepare-launchpad-source.sh VERSION noble|resolute|stonking UNIFIED_ARCHIVE OUTPUT_DIR' >&2
  exit 2
fi

version=$1
series=$2
archive=$3
output=$4
case "$series" in
  noble) distribution_suffix='1~ubuntu24.04mint22' ;;
  resolute) distribution_suffix='1~ubuntu26.04' ;;
  stonking) distribution_suffix='1~ubuntu26.10' ;;
  *) echo "Unsupported Ubuntu series: $series" >&2; exit 2 ;;
esac
ppa_revision=${VIPER_PPA_REVISION:-1}
[[ $ppa_revision =~ ^[1-9][0-9]*$ ]] || {
  echo "VIPER_PPA_REVISION must be a positive integer" >&2; exit 2;
}
deb_version="${version}-${distribution_suffix}+ppa${ppa_revision}"
for command in dpkg-buildpackage tar realpath sha256sum; do
  command -v "$command" >/dev/null || {
    echo "Missing build tool: $command (run this from Ubuntu with dpkg-dev and debhelper)" >&2
    exit 2
  }
done
[[ -f $archive ]] || { echo "Missing source archive: $archive" >&2; exit 1; }
archive=$(realpath -- "$archive")
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd -P)
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
packaging="$root/packaging/launchpad/debian"

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
tar -xJf "$archive" -C "$work"
tree="$work/viper4linux-re-$version"
[[ -f $tree/CMakeLists.txt && -f $tree/third_party/ViPERDSP/viper/ViPER.cpp ]] || {
  echo "Archive is not the complete unified source for version $version" >&2; exit 1;
}
[[ $(<"$tree/RELEASE-VERSION") == "$version" ]] || {
  echo 'RELEASE-VERSION does not match requested version' >&2; exit 1;
}
[[ ! -e $tree/debian ]] || { echo 'Source archive already contains debian/ metadata' >&2; exit 1; }
cp -a -- "$packaging" "$tree/debian"
cat > "$tree/debian/changelog" <<EOF
viper4linux-re ($deb_version) $series; urgency=medium

  * Package upstream ViPER4Linux-RE $version for the Launchpad PPA.
  * Build native binaries using the existing CMake Debian install layout.

 -- ViPER4Linux-RE contributors <noreply@github.com>  $(LC_ALL=C date -Ru)
EOF
# Debian 3.0 (quilt) expects the pristine upstream tarball alongside the source
# directory. Its original archive payload is preserved byte-for-byte.
cp -- "$archive" "$work/viper4linux-re_${version}.orig.tar.xz"
(cd "$tree" && dpkg-buildpackage -S -sa -us -uc -d)
for extension in dsc changes debian.tar.xz orig.tar.xz buildinfo; do
  for item in "$work"/viper4linux-re_*."$extension"; do
    [[ ! -e $item ]] || cp -- "$item" "$output/"
  done
done
test -s "$output/viper4linux-re_${version}.orig.tar.xz"
test -s "$output/viper4linux-re_${deb_version}_source.changes"
echo "Prepared unsigned Launchpad source package for $series: $deb_version"
echo "No files were uploaded. Review, sign and upload from your Launchpad account."
