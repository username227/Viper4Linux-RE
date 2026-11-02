#!/usr/bin/env bash
# Stage the Arch system package in DESTDIR. This never installs to the host.
# Usage: bash packaging/arch/install-package.sh SOURCE_TREE BUILD_DIR PKGDIR PKGNAME
set -Eeuo pipefail

if (( $# != 4 )); then
    printf 'Usage: %s SOURCE_TREE BUILD_DIR PKGDIR PKGNAME\n' "$0" >&2
    exit 2
fi
source_tree=$(cd -- "$1" && pwd -P)
build_dir=$(cd -- "$2" && pwd -P)
pkgdir=$3
pkgname=$4

# A typo must never turn a packaging operation into a host-system install.
if [[ $pkgdir != /* || $pkgdir == / || $pkgdir == /usr || $pkgdir == /usr/ || $pkgdir == "$source_tree" || $pkgdir == "$build_dir" ]]; then
    echo 'Destination must be an absolute staging directory, never a system/source/build directory.' >&2
    exit 2
fi
if [[ ! $pkgname =~ ^[a-z0-9][a-z0-9@._+-]*$ ]]; then
    echo "Invalid package name: $pkgname" >&2
    exit 2
fi

# Validate all required source and binary paths before writing anything.
required_sources=(
    "$source_tree/src/viper_launcher.py"
    "$source_tree/src/viper_output_graph.py"
    "$source_tree/src/viper_output_store.py"
    "$source_tree/src/viper_output_memory.py"
    "$source_tree/src/viper_output_monitor.py"
    "$source_tree/packaging/arch/start-backend"
    "$source_tree/packaging/arch/viper4linux"
    "$source_tree/packaging/arch/viper4linux-output"
    "$source_tree/packaging/arch/viper4linux.desktop"
    "$source_tree/routing-config/filter-chain.conf.d/99-viper-smart.conf"
    "$source_tree/resources/viper4linux-blue.png"
    "$source_tree/resources/viper4linux-blue.svg"
    "$source_tree/man/viper4linux.1"
    "$source_tree/man/viper4linux-ctl.1"
    "$source_tree/LICENSE"
    "$source_tree/THIRD-PARTY-NOTICES.md"
    "$source_tree/README.md"
)
for file in "${required_sources[@]}"; do
    [[ -f $file ]] || { echo "Missing package source: $file" >&2; exit 1; }
done
[[ -d $source_tree/docs ]] || { echo "Missing docs directory: $source_tree/docs" >&2; exit 1; }
for file in "$build_dir/viper-gui" "$build_dir/viper-ctl" "$build_dir/viper-output-state" "$build_dir/viper_dsp_ladspa.so"; do
    [[ -s $file ]] || { echo "Missing built artifact: $file" >&2; exit 1; }
done

# The source directory is the manifest for production Python modules. New
# viper_*.py modules are picked up without edits to a PKGBUILD or this script.
shopt -s nullglob
python_modules=("$source_tree"/src/viper_*.py)
(( ${#python_modules[@]} >= 5 )) || { echo 'No complete ViPER runtime Python module set found.' >&2; exit 1; }

install -Dm755 "$build_dir/viper-gui" "$pkgdir/usr/bin/viper4linux-gui"
install -Dm755 "$build_dir/viper-ctl" "$pkgdir/usr/bin/viper4linux-ctl"
install -Dm755 "$build_dir/viper-output-state" "$pkgdir/usr/bin/viper4linux-output-state"
ln -s -- viper4linux-ctl "$pkgdir/usr/bin/v4l-cli"
install -Dm755 "$build_dir/viper_dsp_ladspa.so" "$pkgdir/usr/lib/ladspa/viper_dsp_ladspa.so"

for file in "${python_modules[@]}"; do
    install -Dm644 "$file" "$pkgdir/usr/lib/viper4linux/${file##*/}"
done
install -Dm755 "$source_tree/packaging/arch/start-backend" "$pkgdir/usr/lib/viper4linux/start-backend"
install -Dm755 "$source_tree/packaging/arch/viper4linux" "$pkgdir/usr/bin/viper4linux"
install -Dm755 "$source_tree/packaging/arch/viper4linux-output" "$pkgdir/usr/bin/viper4linux-output"
install -Dm644 "$source_tree/routing-config/filter-chain.conf.d/99-viper-smart.conf" \
    "$pkgdir/usr/share/viper4linux/routing-config/filter-chain.conf.d/99-viper-smart.conf"
install -Dm644 "$source_tree/packaging/arch/viper4linux.desktop" \
    "$pkgdir/usr/share/applications/viper4linux.desktop"
install -Dm644 "$source_tree/resources/viper4linux-blue.png" \
    "$pkgdir/usr/share/icons/hicolor/256x256/apps/viper4linux.png"
install -Dm644 "$source_tree/resources/viper4linux-blue.svg" \
    "$pkgdir/usr/share/icons/hicolor/scalable/apps/viper4linux.svg"
install -Dm644 "$source_tree/man/viper4linux.1" "$pkgdir/usr/share/man/man1/viper4linux.1"
install -Dm644 "$source_tree/man/viper4linux-ctl.1" "$pkgdir/usr/share/man/man1/viper4linux-ctl.1"
ln -s -- viper4linux-ctl.1 "$pkgdir/usr/share/man/man1/v4l-cli.1"
install -Dm644 "$source_tree/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
install -Dm644 "$source_tree/THIRD-PARTY-NOTICES.md" \
    "$pkgdir/usr/share/licenses/$pkgname/ViPERDSP-NOTICE.md"
install -Dm644 "$source_tree/README.md" "$pkgdir/usr/share/doc/viper4linux/README.md"
install -d "$pkgdir/usr/share/doc/viper4linux/docs"
cp -a -- "$source_tree/docs/." "$pkgdir/usr/share/doc/viper4linux/docs/"

# The final staged file list is checked before makepkg creates the archive.
for file in "${python_modules[@]}"; do
    staged="$pkgdir/usr/lib/viper4linux/${file##*/}"
    [[ -f $staged ]] && cmp -s -- "$file" "$staged" || {
        echo "Python module missing or different in staged package: ${file##*/}" >&2
        exit 1
    }
done
for file in \
    usr/bin/viper4linux-gui usr/bin/viper4linux-ctl usr/bin/viper4linux \
    usr/bin/viper4linux-output-state usr/bin/viper4linux-output \
    usr/lib/ladspa/viper_dsp_ladspa.so usr/lib/viper4linux/start-backend \
    usr/share/viper4linux/routing-config/filter-chain.conf.d/99-viper-smart.conf \
    usr/share/applications/viper4linux.desktop \
    usr/share/icons/hicolor/256x256/apps/viper4linux.png \
    usr/share/icons/hicolor/scalable/apps/viper4linux.svg \
    usr/share/man/man1/viper4linux.1 usr/share/man/man1/viper4linux-ctl.1 \
    "usr/share/licenses/$pkgname/LICENSE" \
    "usr/share/licenses/$pkgname/ViPERDSP-NOTICE.md" \
    usr/share/doc/viper4linux/README.md; do
    [[ -f $pkgdir/$file ]] || { echo "Missing staged package file: $file" >&2; exit 1; }
done
for file in usr/bin/v4l-cli usr/share/man/man1/v4l-cli.1; do
    [[ -L $pkgdir/$file ]] || { echo "Missing staged symlink: $file" >&2; exit 1; }
done
printf 'PASS: staged %d Python modules and verified the Arch installation tree.\n' "${#python_modules[@]}"
