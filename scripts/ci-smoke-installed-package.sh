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
        test -r /usr/share/licenses/viper4linux-re/LICENSE
        test -f /usr/share/man/man1/viper4linux.1
        test "$(readlink /usr/share/man/man1/v4l-cli.1)" = viper4linux-ctl.1
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
        test -r /usr/share/doc/viper4linux-re/THIRD-PARTY-NOTICES.md
        test -f /usr/share/man/man1/viper4linux.1.gz
        test "$(readlink /usr/share/man/man1/v4l-cli.1.gz)" = viper4linux-ctl.1.gz
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
nm -D /usr/lib/ladspa/viper_dsp_ladspa.so | grep -Eq ' [TW] ladspa_descriptor$'

echo 'Checking read-only CLI help and backend fingerprint entrypoints...'
# Read-only command paths from the original release workflow smoke contract.
# Do not run viper4linux, start-backend, or a live PipeWire session.
viper4linux-ctl --help >/dev/null
v4l-cli --help >/dev/null
python3 /usr/lib/viper4linux/viper_launcher.py --fingerprint >/dev/null

echo "PASS: installed $mode package in disposable CI container; files, aliases, LADSPA export, and read-only CLI checks passed (no audio launched)"
