#!/usr/bin/env bash
# Foreground-only routing client. Never starts an additional PipeWire daemon.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
[[ -s build/viper_dsp_ladspa.so && -x build/viper-ctl && -r routing-config/filter-chain.conf ]] || {
  echo 'Not built yet; run ./install.sh' >&2; exit 1;
}
command -v pipewire >/dev/null || { echo 'pipewire executable missing' >&2; exit 1; }
[[ ${PIPEWIRE_DAEMON:-false} != true ]] || {
  echo 'PIPEWIRE_DAEMON=true: refusing to start the server rather than a client' >&2; exit 1;
}

# Prevent two instances of the unified backend, including detached/orphaned clients.
runtime=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
[[ -d $runtime ]] || { echo "Runtime directory $runtime missing" >&2; exit 1; }
exec 9>"$runtime/viperlinux-unified-v1.lock"
if ! flock -n 9; then
  echo 'ViPER for Linux backend already appears to be running. Use ./status.sh; do not launch a duplicate.' >&2
  exit 1
fi
printf 'Running ViPER for Linux smart filter (one foreground client, current default physical output).\n'
printf 'Saved preferences are read from ~/.config/ViPERforLinux/ViPERClean.conf.\n'
printf 'Settings can be changed in the Qt6 GUI without restarting the filter.\n'
printf 'Stop this foreground backend with your terminal interrupt shortcut.\n\n'

# Identify the actual compiled DSP/control/config so a future launcher can
# safely replace an outdated backend, even when installed from another tree.
backend_fingerprint=$(python3 src/viper_launcher.py --fingerprint)
# Retain the unified instance marker so existing control tools remain compatible. Discard
# PIPEWIRE_DAEMON; the client only connects to your existing PipeWire server.
exec env -u PIPEWIRE_DAEMON \
  VIPER_CLEAN_INSTANCE=viperlinux-unified-v1 \
  VIPER_BACKEND_FINGERPRINT="$backend_fingerprint" \
  LADSPA_PATH="$PWD/build${LADSPA_PATH:+:$LADSPA_PATH}" \
  PIPEWIRE_CONFIG_DIR="$PWD/routing-config" \
  pipewire -c filter-chain.conf
