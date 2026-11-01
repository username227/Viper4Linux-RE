#!/usr/bin/env bash
# One useful status report; no repeated manual checks needed.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
python3 - <<'PY'
import os, pathlib
own = os.getuid()
current, legacy = [], []
for p in pathlib.Path('/proc').iterdir():
    if not p.name.isdecimal(): continue
    try:
        if p.stat().st_uid != own: continue
        cmd = (p/'cmdline').read_bytes().replace(b'\0', b' ').decode('utf-8', 'replace')
        if 'filter-chain.conf' not in cmd or 'pipewire' not in cmd: continue
        env = (p/'environ').read_bytes().split(b'\0')
        marker = b'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1'
        if marker in env: current.append((p.name, cmd.strip()))
        elif any(b'viper-bass-prototype' in e or b'viper' in e.lower() for e in env if e.startswith((b'PIPEWIRE_CONFIG_DIR=', b'LADSPA_PATH='))):
            legacy.append((p.name, cmd.strip()))
    except (FileNotFoundError, PermissionError, ProcessLookupError): pass
if current:
    print('ViPER for Linux running:', ', '.join(pid for pid, _ in current))
else: print('ViPER for Linux: not running')
if legacy:
    print('WARNING: Other ViPER filter-chain client(s) detected:', ', '.join(pid for pid, _ in legacy))
    print('Stop them before using the new prototype to avoid double processing.')
PY
if [[ -x build/viper-ctl ]]; then
  echo 'DSP control channel:'
  ./build/viper-ctl status || true
else
  echo 'Not built yet: ./install.sh'
fi
