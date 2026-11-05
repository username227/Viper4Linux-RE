#!/usr/bin/env bash
# Graphical startup diagnosis; does NOT start viper4linux Python launcher,
# PipeWire, the LADSPA backend, or any other audio process.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
mode="${1:?usage: ci-startup-protocol.sh x11|wayland}"
mkdir -p startup-diagnostics
runtime="$(mktemp -d)"
chmod 700 "$runtime"
sway_pid=''
cleanup() {
    if [[ -n "$sway_pid" ]]; then kill "$sway_pid" 2>/dev/null || :; wait "$sway_pid" 2>/dev/null || :; fi
    rm -rf "$runtime"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$runtime"
export XDG_SESSION_TYPE="$mode"
if [[ "$mode" == x11 ]]; then
    export QT_QPA_PLATFORM=xcb
    echo 'X11 server:' > startup-diagnostics/session.txt
    xdpyinfo | head -n 4 >> startup-diagnostics/session.txt
elif [[ "$mode" == wayland ]]; then
    export QT_QPA_PLATFORM=wayland
    export WAYLAND_DISPLAY=wayland-ci
    export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1
    export XDG_CURRENT_DESKTOP=sway
    unset DISPLAY
    printf 'set $mod Mod4\noutput * resolution 1280x800\n' > "$runtime/sway.conf"
    sway --unsupported-gpu --config "$runtime/sway.conf" --socket "$WAYLAND_DISPLAY" \
      > startup-diagnostics/sway.log 2>&1 &
    sway_pid=$!
    for attempt in $(seq 1 80); do
        [[ -S "$runtime/$WAYLAND_DISPLAY" ]] && break
        kill -0 "$sway_pid" 2>/dev/null || { cat startup-diagnostics/sway.log; exit 1; }
        sleep 0.2
    done
    [[ -S "$runtime/$WAYLAND_DISPLAY" ]] || { cat startup-diagnostics/sway.log; exit 1; }
    wayland-info > startup-diagnostics/wayland-info.txt 2>&1
    grep -q xdg_activation_v1 startup-diagnostics/wayland-info.txt || {
        echo 'The test compositor did not expose xdg_activation_v1' >&2
        cat startup-diagnostics/wayland-info.txt >&2
        exit 1
    }
else
    echo 'Unsupported session type' >&2
    exit 2
fi
python3 tests/ci-startup-protocol.py "$mode" build/viper-gui
