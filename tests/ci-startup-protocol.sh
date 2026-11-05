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
    export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1
    export XDG_CURRENT_DESKTOP=sway
    unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET
    printf 'set $mod Mod4\noutput * resolution 1280x800\n' > "$runtime/sway.conf"
    # Ubuntu 24.04 Sway 1.9 creates the Wayland socket automatically.
    sway --unsupported-gpu --config "$runtime/sway.conf" \
      > startup-diagnostics/sway.log 2>&1 &
    sway_pid=$!
    socket=''
    for attempt in $(seq 1 80); do
        socket="$(find "$runtime" -maxdepth 1 -type s -name 'wayland-*' -print -quit)"
        [[ -n "$socket" ]] && break
        kill -0 "$sway_pid" 2>/dev/null || { cat startup-diagnostics/sway.log; exit 1; }
        sleep 0.2
    done
    [[ -n "$socket" && -S "$socket" ]] || { cat startup-diagnostics/sway.log; exit 1; }
    export WAYLAND_DISPLAY="${socket##*/}"
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
