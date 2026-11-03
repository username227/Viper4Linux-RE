#!/usr/bin/env python3
"""PipeWire Audio Output manager; persists/restores native DSP state when opted in.

The manager outlives the Qt GUI but is supervised by the ordinary viper4linux
launcher and stops when its own DSP backend exits. No systemd unit or GUI
automation, and no audio-thread callbacks are involved.
"""
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
from viper_output_memory import OutputMemory

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

class NativeEffects:
    """Strict child-process adapter for the QtCore shared-memory state utility."""
    def __init__(self, helper: Path):
        self.helper = helper
        self.backend_pid = None

    def _call(self, action: str, snapshot: dict | None = None) -> str:
        if not self.helper.is_file() or not os.access(self.helper, os.X_OK):
            raise RuntimeError('Native Audio Output state helper is not installed')
        payload = json.dumps(snapshot, separators=(',', ':')) if snapshot is not None else None
        result = subprocess.run([str(self.helper), action], input=payload,
                                text=True, capture_output=True, timeout=3,
                                check=False)
        if result.returncode:
            raise RuntimeError(f'Native Audio Output {action} failed: ' +
                               (result.stderr.strip() or f'exit {result.returncode}')[:280])
        return result.stdout

    def capture(self) -> dict:
        state = json.loads(self._call('capture'))
        if not isinstance(state, dict) or state.get('format') != 1:
            raise RuntimeError('Native Audio Output capture returned an incompatible snapshot')
        owner = state.pop('backend_pid', None)
        if not isinstance(owner, int) or owner <= 0:
            raise RuntimeError('Native Audio Output capture did not identify its DSP session')
        if self.backend_pid is not None and self.backend_pid != owner:
            self.backend_pid = owner
            raise RuntimeError('DSP session changed; reestablishing saved output settings')
        self.backend_pid = owner
        return state

    def restore(self, snapshot: dict) -> None:
        self._call('restore', snapshot)

    def preset(self, path: Path) -> dict:
        if not self.helper.is_file() or not os.access(self.helper, os.X_OK):
            raise RuntimeError('Native Audio Output state helper is not installed')
        result = subprocess.run([str(self.helper), 'preset', str(path)],
                                text=True, capture_output=True, timeout=3, check=False)
        if result.returncode:
            raise RuntimeError('Named preset conversion failed: ' + result.stderr.strip()[:280])
        state = json.loads(result.stdout)
        if state.get('format') != 1 or not isinstance(state.get('backend_pid'), int):
            raise ValueError('Invalid converted native preset snapshot')
        if self.backend_pid is not None and self.backend_pid != state['backend_pid']:
            raise RuntimeError('DSP session changed during preset conversion')
        self.backend_pid = state.pop('backend_pid')
        return state

def helper_path() -> Path:
    here = Path(__file__).resolve().parent
    if here == Path('/usr/lib/viper4linux'):
        return Path('/usr/bin/viper4linux-output-state')
    return here.parent / 'build/viper-output-state'

def status_for(graph: PipeWireGraph, store: OutputStore, backend_pid: int,
               *, action: str = 'disabled', revision: int = 0, error: str = '') -> dict:
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
            'outputs': outputs, 'memory_enabled': store.enabled(),
            'memory_action': action, 'memory_revision': revision,
            'memory_error': error}

def process_assignments(runtime: Path, store: OutputStore, effects: NativeEffects,
                        memory: OutputMemory, active, settled: bool) -> bool:
    """Handle requests on the same thread as monitor checkpoints."""
    library = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'viper4linux/presets'
    active_applied = False
    for request in sorted(runtime.glob('viper4linux-assign-*.request.json')):
        identity = request.name[len('viper4linux-assign-'):-len('.request.json')]
        response = runtime / f'viper4linux-assign-{identity}.result.json'
        try:
            if request.is_symlink() or response.exists():
                raise ValueError('Duplicate or unsafe preset assignment request')
            content = json.loads(request.read_text(encoding='utf-8'))
            if not isinstance(content, dict) or set(content) != {'target', 'preset'}:
                raise ValueError('Invalid preset assignment request')
            key = content['target']
            if not isinstance(key, str) or key not in store.inventory():
                raise ValueError('Unknown remembered Audio Output')
            if not store.enabled():
                raise ValueError('Per-output memory is disabled')
            selected = content['preset']
            if not isinstance(selected, str):
                raise ValueError('Invalid preset filename')
            preset = Path(selected)
            if (preset.suffix != '.ini' or preset.is_symlink() or
                preset.parent.resolve() != library.resolve() or not preset.is_file()):
                raise ValueError('Preset is not in the named preset library')
            snapshot = effects.preset(preset)
            if active and active.key == key:
                if not settled or memory.current is None or memory.current.key != key:
                    raise RuntimeError('Audio Output is transitioning; retry assignment')
                effects.restore(snapshot)
                store.save_settings(key, snapshot)
                active_applied = True
            else:
                store.save_settings(key, snapshot)
            result = {'ok': True, 'target': key}
        except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired, KeyError,
                json.JSONDecodeError) as exc:
            result = {'ok': False, 'error': str(exc)[:280]}
        finally:
            try:
                write_status(response, result)
            except OSError as exc:
                print('Preset assignment acknowledgment failed: ' + str(exc), file=sys.stderr)
            request.unlink(missing_ok=True)
    return active_applied

def stop_handler(_signal, _frame):
    global STOP
    STOP = True

def service(graph: PipeWireGraph, store: OutputStore, effects: NativeEffects,
            backend_pid: int, path: Path, pipe: subprocess.Popen) -> None:
    """One-second status heartbeat; require two consecutive settled observations."""
    memory = OutputMemory(store, effects)
    enabled_at_last_tick = store.enabled()
    stable_key = None
    stable_since = time.monotonic()
    last_checkpoint = 0.0
    last_status = 0.0
    last_action, error, revision = 'disabled', '', 0
    stream = JsonArrayStream()
    remembered_inventory = set(store.inventory())
    while not STOP and backend_alive(backend_pid):
        readable, _, _ = select.select([pipe.stdout], [], [], 0.75)
        if readable:
            chunk = os.read(pipe.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError('PipeWire monitor exited unexpectedly')
            for batch in stream.feed(chunk):
                graph.update(batch)
        now = time.monotonic()
        if now - last_status < 1.0:
            continue
        # Keep disconnected outputs available for preset assignment later.
        connected = graph.available()
        for candidate in connected:
            if candidate.key not in remembered_inventory:
                store.remember(candidate)
                remembered_inventory.add(candidate.key)
        active = graph.active()
        key = active.key if active else None
        if key != stable_key:
            stable_key, stable_since = key, now
        enabled = store.enabled()
        # On explicit disable, keep the last edits without changing sound.
        if enabled_at_last_tick and not enabled and memory.current and active and memory.current.key == key:
            try:
                store.save_settings(memory.current.key, effects.capture())
            except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
                error = str(exc)[:280]
        # The *user* is enabling from the GUI while monitor already runs.
        # Initialize the currently heard sound; do not unexpectedly load an old one.
        if enabled and not enabled_at_last_tick and memory.current and active and memory.current.key == key:
            try:
                memory.checkpoint()
                last_action, error = 'initialized', ''
            except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
                error = str(exc)[:280]
        enabled_at_last_tick = enabled
        if active and now - stable_since >= 1.0:
            try:
                action = memory.observe(active)
                if action == 'restored':
                    revision += 1
                if action not in ('same', 'disabled'):
                    last_action = action
                if action == 'disabled':
                    last_action = 'disabled'
                error = ''
                if enabled and now - last_checkpoint >= 3.0:
                    memory.checkpoint()
                    last_checkpoint = now
            except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
                # A missing DSP channel while playback is idle is normal.
                # Invalidate the cached session so the next active mapping
                # gets a real restore rather than overwriting the saved sound.
                memory.current = None
                last_action, error = 'pending', str(exc)[:280]
                print('Audio Output memory: ' + error, file=sys.stderr, flush=True)
        elif not enabled:
            last_action = 'disabled'
        if process_assignments(path.parent, store, effects, memory, active,
                               bool(active and now - stable_since >= 1.0)):
            revision += 1
            last_action, error = 'restored', ''
        try:
            write_status(path, status_for(graph, store, backend_pid,
                        action=last_action, revision=revision, error=error))
        except (OSError, ValueError) as exc:
            print(f'Audio Output status error: {exc}', file=sys.stderr, flush=True)
        last_status = now

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
        graph, store = PipeWireGraph(), OutputStore()
        path = runtime / STATUS_FILE
        proc = subprocess.Popen(['pw-dump', '--no-colors', '-m'],
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, close_fds=True)
        try:
            service(graph, store, NativeEffects(helper_path()), args.backend_pid, path, proc)
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
