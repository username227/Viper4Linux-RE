#!/usr/bin/env bash
# Stop ONLY the viperlinux-unified-v1 PipeWire client, not system PipeWire.
set -euo pipefail
python3 - <<'PY'
import os, signal, pathlib
own = os.getuid()
found = []
for p in pathlib.Path('/proc').iterdir():
    if not p.name.isdecimal(): continue
    try:
        if p.stat().st_uid != own: continue
        data = (p/'environ').read_bytes().split(b'\0')
        cmd = (p/'cmdline').read_bytes().replace(b'\0', b' ')
        if b'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1' in data and b'filter-chain.conf' in cmd:
            found.append(int(p.name))
    except (PermissionError, FileNotFoundError, ProcessLookupError): pass
if not found:
    print('No ViPER for Linux instance is running.')
else:
    for pid in found:
        os.kill(pid, signal.SIGTERM)
        print('Sent SIGTERM to ViPER for Linux client PID', pid)
print('System PipeWire and any older prototypes were not touched.')
PY
