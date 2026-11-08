#!/usr/bin/env bash
# Foreground-only routing client. Never starts an additional PipeWire daemon.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
# Preserve a developer-edited source routing config, if present. New CMake
# checkouts generate the stock routing config only inside build/routing-config.
config_dir="$PWD/build/routing-config"
if [[ -r routing-config/filter-chain.conf ]]; then
  config_dir="$PWD/routing-config"
fi
[[ -s build/viper_dsp_ladspa.so && -x build/viper-ctl &&
   -r "$config_dir/filter-chain.conf" &&
   -r "$config_dir/filter-chain.conf.d/99-viper-smart.conf" ]] || {
  echo 'Missing source build or routing config; rerun cmake -S . -B build && cmake --build build' >&2
  exit 1
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
  echo 'ViPER for Linux backend already appears to be running. Use ./scripts/status.sh; do not launch a duplicate.' >&2
  exit 1
fi
# Timestamp backend stdout/stderr without wrapping or replacing its PipeWire PID.
# The background Bash reader inherits the original log fd, not the DSP pipe.
# Close the instance lock in that reader so it cannot delay a subsequent launch.
backend_pid=$$
exec > >(
  exec 9>&-
  while IFS= read -r line || [[ -n "$line" ]]; do
    printf -v timestamp '%(%Y-%m-%dT%H:%M:%S%z)T' -1
    printf '[%s:%s] [backend pid=%s] %s\n' "${timestamp:0:-2}" "${timestamp: -2}" "$backend_pid" "$line"
  done
  printf -v timestamp '%(%Y-%m-%dT%H:%M:%S%z)T' -1
  printf '[%s:%s] [backend pid=%s] output stream closed\n' "${timestamp:0:-2}" "${timestamp: -2}" "$backend_pid"
) 2>&1

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
  PIPEWIRE_CONFIG_DIR="$config_dir" \
  pipewire -c filter-chain.conf
