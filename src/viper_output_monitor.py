#!/usr/bin/env python3
"""Read-only PipeWire Audio Output monitor tied to ViPER's backend lifetime."""
from __future__ import annotations
import argparse
from dataclasses import asdict
import fcntl
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time
from viper_output_graph import JsonArrayStream, PipeWireGraph
from viper_output_store import OutputStore

INSTANCE = b'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1'
STOP = False
STATUS_FILE = 'viper4linux-output-status.json'

def runtime_dir() -> Path:
    return Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))

def backend_alive(pid: int, proc_root: Path = Path('/proc')) -> bool:
    """Never track an unrelated process or a reused PID."""
    if pid <= 0:
        return False
    entry = proc_root / str(pid)
    try:
        if entry.stat().st_uid != os.getuid():
            return False
        command = (entry / 'cmdline').read_bytes().split(b'\0')
        if not any(Path(arg.decode(errors='replace')).name == 'pipewire'
                   for arg in command if arg) or b'filter-chain.conf' not in command:
            return False
        return INSTANCE in (entry / 'environ').read_bytes().split(b'\0')
    except (OSError, PermissionError, FileNotFoundError):
        return False

def write_status(path: Path, state: dict) -> None:
    fd, tmp = tempfile.mkstemp(prefix='.viper-output-', dir=str(path.parent))
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            json.dump(state, out, sort_keys=True)
            out.write('\n')
            out.flush()
            os.fsync(out.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)

def status_for(graph: PipeWireGraph, store: OutputStore, backend_pid: int) -> dict:
    connected = graph.available()
    keys = {o.key for o in connected}
    outputs = [asdict(o) for o in connected]
    for key, entry in store.inventory().items():
        if key not in keys:
            outputs.append({'key': key, 'description': entry['description'],
                            'node_name': entry['node_name'], 'node_id': None,
                            'connected': False})
    outputs.sort(key=lambda o: (o['description'].casefold(), o['key']))
    active = graph.active()
    return {'format': 1, 'timestamp': time.time(),
            'manager_pid': os.getpid(), 'backend_pid': backend_pid,
            'active': asdict(active) if active else None,
            'outputs': outputs, 'memory_enabled': store.enabled()}

def stop_handler(_signal, _frame):
    global STOP
    STOP = True

def main() -> int:
    parser = argparse.ArgumentParser(description='ViPER Audio Output session monitor')
    parser.add_argument('--backend-pid', type=int, required=True)
    args = parser.parse_args()
    runtime = runtime_dir()
    if not runtime.is_dir() or runtime.stat().st_uid != os.getuid():
        print(f'Invalid PipeWire runtime directory: {runtime}', file=sys.stderr)
        return 1
    fd = os.open(runtime / 'viper4linux-output-monitor.lock',
                 os.O_CREAT | os.O_RDWR | os.O_CLOEXEC, 0o600)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return 0
        if not backend_alive(args.backend_pid):
            print('DSP backend not running', file=sys.stderr)
            return 1
        signal.signal(signal.SIGTERM, stop_handler)
        signal.signal(signal.SIGINT, stop_handler)
        graph, stream, store = PipeWireGraph(), JsonArrayStream(), OutputStore()
        path = runtime / STATUS_FILE
        proc = subprocess.Popen(['pw-dump', '--no-colors', '-m'],
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, close_fds=True)
        previous, last_status = None, 0.0
        try:
            while not STOP and backend_alive(args.backend_pid):
                readable, _, _ = select.select([proc.stdout], [], [], 0.75)
                if readable:
                    chunk = os.read(proc.stdout.fileno(), 65536)
                    if not chunk:
                        raise RuntimeError('PipeWire monitor exited unexpectedly')
                    for batch in stream.feed(chunk):
                        graph.update(batch)
                now = time.monotonic()
                if now - last_status < 1.0:
                    continue
                connected = graph.available()
                inventory = tuple((o.key, o.description, o.node_name) for o in connected)
                if inventory != previous:
                    for output in connected:
                        store.remember(output)
                    previous = inventory
                write_status(path, status_for(graph, store, args.backend_pid))
                last_status = now
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            try:
                existing = json.loads(path.read_text(encoding='utf-8'))
                if existing.get('manager_pid') == os.getpid():
                    path.unlink(missing_ok=True)
            except (OSError, ValueError):
                pass
        return 0
    finally:
        os.close(fd)

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError) as exc:
        print(f'Audio Output monitor: {exc}', file=sys.stderr)
        sys.exit(1)
