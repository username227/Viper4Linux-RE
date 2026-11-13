#!/usr/bin/env bash
# Deliberately invokes unsafe install requests. Run ONLY inside a disposable
# CI Docker container; never on a developer workstation or host installation.
set -Eeuo pipefail
if [[ ${VIPER_CI_STAGING_GUARD_TEST:-0} != 1 || ! -f /.dockerenv ]]; then
    echo 'Refusing unsafe-prefix tests outside explicitly opted-in CI Docker.' >&2
    exit 2
fi
if (( $# != 1 )); then
    echo 'Usage: tests/check-cmake-install-guard.sh ABSOLUTE_CMAKE_BUILD_DIR' >&2
    exit 2
fi
built=$(cd -- "$1" && pwd -P)
[[ -f "$built/CMakeCache.txt" ]] || { echo 'CMake build directory missing.' >&2; exit 1; }
grep -Fxq 'VIPER_ENABLE_SYSTEM_INSTALL:BOOL=ON' "$built/CMakeCache.txt"
grep -Fxq 'CMAKE_INSTALL_PREFIX:PATH=/usr' "$built/CMakeCache.txt"

work=$(mktemp -d /tmp/viper-cmake-guard-XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
mkdir -p -- "$work/stage"
ln -s -- / "$work/host-root-symlink"

reject() {
    local expected=$1
    shift
    if "$@" > "$work/result.log" 2>&1; then
        echo "FAIL: unsafe CMake install request was accepted: $*" >&2
        exit 1
    fi
    if ! grep -Eqi -- "$expected" "$work/result.log"; then
        echo "FAIL: rejection did not report expected safety guard: $*" >&2
        cat -- "$work/result.log" >&2
        exit 1
    fi
}

reject 'DESTDIR' env -u DESTDIR cmake --install "$built"
reject 'Unsafe DESTDIR|Symlinked DESTDIR' env DESTDIR=/usr cmake --install "$built"
reject 'Unsafe DESTDIR|Symlinked DESTDIR' env DESTDIR="$work/host-root-symlink" cmake --install "$built"
reject 'existing.*DESTDIR|DESTDIR.*existing' env DESTDIR="$work/no-such-stage" cmake --install "$built"
reject 'prefix' env DESTDIR="$work/stage" cmake --install "$built" --prefix /tmp/viper-unsupported-prefix

DESTDIR="$work/stage" cmake --install "$built" > "$work/success.log"
test -x "$work/stage/usr/bin/viper4linux-gui"
test -x "$work/stage/usr/bin/v4l-cli"
test ! -L "$work/stage/usr/bin/v4l-cli"
test "$(readlink "$work/stage/usr/bin/viper4linux-ctl")" = v4l-cli
test -s "$work/stage/usr/lib/ladspa/viper_dsp_ladspa.so"

echo 'PASS: CMake refuses host, symlinked, missing and wrong-prefix destinations; private DESTDIR stages successfully'
