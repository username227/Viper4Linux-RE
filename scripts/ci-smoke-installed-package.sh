#!/usr/bin/env bash
# Install a previously-built native package inside an ephemeral CI container
# and check runtime entrypoints. This script never starts PipeWire or the GUI.
# It must never be invoked on a workstation or a running audio session.
set -Eeuo pipefail
# Give CI a useful failing check instead of an anonymous exit 1 after apt/pacman.
trap 'printf "FAIL: container smoke at line %s: %s\\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

if [[ $# -ne 2 ]]; then
    echo 'Usage: ci-smoke-installed-package.sh arch|debian ABSOLUTE_PACKAGE_PATH' >&2
    exit 2
fi
if [[ ${VIPER_CI_CONTAINER_SMOKE:-0} != 1 || ! -f /.dockerenv ]]; then
    echo 'Refusing package installation outside an explicitly opted-in CI Docker container.' >&2
    exit 2
fi

mode=$1
package=$2
[[ $package == /* && -s $package ]] || {
    echo "Package must be an existing absolute file: $package" >&2
    exit 2
}

. /etc/os-release
case "$mode" in
    arch)
        [[ $ID == arch && $package == *.pkg.tar.zst ]] || {
            echo 'Arch smoke check requires an Arch container and Arch package.' >&2
            exit 2
        }
        pacman -Syu --noconfirm --needed glibc gcc-libs qt6-base pipewire ladspa python binutils
        pacman -U --noconfirm "$package"
        pacman -Q viper4linux-re
        echo 'Checking Arch-specific installed documentation and man-page links...'
        # Arch provides standard GPL text; custom upstream notice remains packaged.
        test ! -e /usr/share/licenses/viper4linux-re/LICENSE
        test -r /usr/share/licenses/viper4linux-re/ViPERDSP-NOTICE.md
        # Arch makepkg's zipman step may compress .1 files to .1.gz.
        # Official minimal Arch Docker images can also omit all man pages
        # during pacman installation. Verify the built archive contains them
        # regardless, and verify the installed alias whenever it is present.
        arch_listing=$(pacman -Qlp "$package")
        for name in viper4linux.1 viper4linux-ctl.1 v4l-cli.1; do
            [[ $arch_listing == *"usr/share/man/man1/$name"* ]] || {
                echo "Native Arch archive missing manual page: $name" >&2
                exit 1
            }
        done
        if [[ -f /usr/share/man/man1/viper4linux.1.gz ]]; then
            test "$(readlink /usr/share/man/man1/v4l-cli.1.gz)" = viper4linux-ctl.1.gz
        elif [[ -f /usr/share/man/man1/viper4linux.1 ]]; then
            test "$(readlink /usr/share/man/man1/v4l-cli.1)" = viper4linux-ctl.1
        else
            echo 'Arch container omitted optional manual pages; package archive entries verified.'
        fi
        ;;
    debian)
        [[ ( $ID == ubuntu || $ID == debian ) && $package == *.deb ]] || {
            echo 'Debian smoke check requires an Ubuntu/Debian container and .deb.' >&2
            exit 2
        }
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        # Install the already-built artifact, not a fresh locally rebuilt .deb.
        apt-get install -y --no-install-recommends "$package" binutils
        echo 'Checking Debian package registration and documentation...'
        dpkg -s viper4linux-re | grep -Fxq 'Status: install ok installed'
        test -r /usr/share/doc/viper4linux-re/copyright

        # Official slim Ubuntu/Debian Docker images can have dpkg path-exclude
        # rules for /usr/share/doc and /usr/share/man. Verify the actual .deb
        # contains the documentation and symlink even when the container
        # intentionally excludes them during dpkg unpacking.
        deb_listing=$(dpkg-deb --contents "$package")
        for path in \
            usr/share/doc/viper4linux-re/THIRD-PARTY-NOTICES.md \
            usr/share/doc/viper4linux-re/LICENSE \
            usr/share/doc/viper4linux-re/copyright \
            usr/share/man/man1/viper4linux.1.gz \
            usr/share/man/man1/viper4linux-ctl.1.gz; do
            [[ $deb_listing == *"$path"* ]] || {
                echo "Native .deb archive missing required file: $path" >&2
                exit 1
            }
        done
        [[ $deb_listing == *"usr/share/man/man1/v4l-cli.1.gz -> viper4linux-ctl.1.gz"* ]] || {
            echo 'Native .deb archive missing manual-page alias.' >&2
            exit 1
        }

        if grep -Rqs '^path-exclude=/usr/share/doc/' /etc/dpkg/dpkg.cfg.d; then
            echo 'Container dpkg policy excludes optional documentation; archive contents verified.'
        else
            test -r /usr/share/doc/viper4linux-re/THIRD-PARTY-NOTICES.md
        fi
        if grep -Rqs '^path-exclude=/usr/share/man/' /etc/dpkg/dpkg.cfg.d; then
            echo 'Container dpkg policy excludes optional manual pages; archive contents verified.'
        else
            test -f /usr/share/man/man1/viper4linux.1.gz
            test "$(readlink /usr/share/man/man1/v4l-cli.1.gz)" = viper4linux-ctl.1.gz
        fi
        ;;
    *)
        echo "Unknown package format: $mode" >&2
        exit 2
        ;;
esac

# Verify installed binary and data layout, but NEVER start the backend/GUI.
echo 'Checking common installed commands, routing file, and LADSPA module...'
for app in viper4linux viper4linux-gui viper4linux-ctl \
           viper4linux-output viper4linux-output-state; do
    test -x "/usr/bin/$app"
done
test -x /usr/lib/viper4linux/start-backend
test -r /usr/lib/viper4linux/viper_launcher.py
test -r /usr/share/viper4linux/routing-config/filter-chain.conf.d/99-viper-smart.conf
test -L /usr/bin/v4l-cli
test "$(readlink /usr/bin/v4l-cli)" = viper4linux-ctl
test -s /usr/lib/ladspa/viper_dsp_ladspa.so
# Avoid grep -q in a pipefail pipeline: an early match can SIGPIPE nm.
nm -D /usr/lib/ladspa/viper_dsp_ladspa.so | grep -E ' [TW] ladspa_descriptor$' >/dev/null

echo 'Checking read-only CLI help and backend fingerprint entrypoints...'
# Read-only command paths from the original release workflow smoke contract.
# Do not run viper4linux, start-backend, or a live PipeWire session.
viper4linux-ctl --help >/dev/null
v4l-cli --help >/dev/null
python3 /usr/lib/viper4linux/viper_launcher.py --fingerprint >/dev/null

echo "PASS: installed $mode package in disposable CI container; files, aliases, LADSPA export, and read-only CLI checks passed (no audio launched)"
