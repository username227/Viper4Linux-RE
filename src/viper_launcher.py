#!/usr/bin/env python3
"""Launch the matching PipeWire client once, then open the Qt GUI.

Only processes with our exact PipeWire instance marker are eligible for a
restart. The fingerprint represents the *built backend*, not merely Git HEAD.
"""
from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

INSTANCE = b"VIPER_CLEAN_INSTANCE=viperlinux-unified-v1"
FINGERPRINT_ENV = b"VIPER_BACKEND_FINGERPRINT="
ROOT = Path(__file__).resolve().parent.parent
# The Arch package installs this module at /usr/lib/viper4linux; source-tree
# builds keep the original relative layout and never touch /usr.
SYSTEM_MODE = Path(__file__).resolve().parent == Path('/usr/lib/viper4linux')
SYSTEM_BACKEND = Path('/usr/lib/ladspa/viper_dsp_ladspa.so')
SYSTEM_CTL = Path('/usr/bin/viper4linux-ctl')
SYSTEM_ROUTING = Path('/usr/share/viper4linux/routing-config/filter-chain.conf.d')
PROC = Path('/proc')


def source_routing_dir(root: Path) -> Path:
    """Match scripts/start.sh: keep custom source config, otherwise use CMake."""
    legacy = root / 'routing-config'
    if (legacy / 'filter-chain.conf').is_file():
        return legacy
    return root / 'build/routing-config'


def fingerprint(root: Path = ROOT) -> str:
    # Include the native DSP binary, the IPC controller and the filter-chain
    # configuration. A rebuilt backend is detected even if Git HEAD is unchanged.
    if SYSTEM_MODE and root == ROOT:
        paths = [SYSTEM_BACKEND, SYSTEM_CTL]
        paths += sorted(SYSTEM_ROUTING.rglob('*.conf'))
        root = Path('/usr')  # stable path components in the fingerprint
    else:
        paths = [root / 'build/viper_dsp_ladspa.so', root / 'build/viper-ctl']
        routing = source_routing_dir(root)
        if not (routing / 'filter-chain.conf').is_file():
            raise RuntimeError('Source PipeWire routing config missing; rerun CMake configure.')
        paths += sorted(routing.rglob('*.conf'))
    if len(paths) < 3:
        raise RuntimeError('PipeWire configuration missing. Run cmake -S . -B build && cmake --build build first.')
    digest = hashlib.sha256(b'viper4linux-backend-fingerprint-v1\0')
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f'Missing backend build artifact: {path}. Run cmake -S . -B build && cmake --build build first.')
        digest.update(str(path.relative_to(root)).encode() + b'\0')
        with path.open('rb') as f:
            while chunk := f.read(1024 * 1024):
                digest.update(chunk)
    return digest.hexdigest()


def running_clients(proc_root: Path = PROC) -> dict[int, str | None]:
    """Only PipeWire filter-chain clients explicitly marked as ours."""
    result = {}
    uid = os.getuid()
    for process in proc_root.iterdir():
        if not process.name.isdecimal():
            continue
        try:
            if process.stat().st_uid != uid:
                continue
            argv = (process / 'cmdline').read_bytes().split(b'\0')
            if not argv or not any(Path(a.decode(errors='replace')).name == 'pipewire'
                                   for a in argv if a):
                continue
            if b'filter-chain.conf' not in argv:
                continue
            env = (process / 'environ').read_bytes().split(b'\0')
            if INSTANCE not in env:
                continue
            item = next((e[len(FINGERPRINT_ENV):] for e in env
                         if e.startswith(FINGERPRINT_ENV)), None)
            result[int(process.name)] = item.decode('ascii', errors='replace') if item else None
        except (FileNotFoundError, ProcessLookupError, PermissionError, OSError):
            continue
    return result


def wait_until_absent(pids: set[int], timeout: float = 5.0) -> bool:
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        if not (pids & running_clients().keys()):
            return True
        time.sleep(0.1)
    return not (pids & running_clients().keys())


def ensure_backend(root: Path = ROOT, *, force_restart: bool = False) -> int:
    desired = fingerprint(root)
    runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
    if not runtime.is_dir():
        raise RuntimeError(f'PipeWire runtime directory does not exist: {runtime}')

    # Serializes launchers from different source trees for the same user.
    with (runtime / 'viper4linux-launch.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        current = running_clients()
        if not force_restart and current and all(tag == desired for tag in current.values()) and len(current) == 1:
            print('Viper4Linux backend already running at the current build; reusing it.', flush=True)
            return next(iter(current))
        if current:
            print('Restarting Viper4Linux backend.' if force_restart else
                  'Previous or mismatched Viper4Linux backend detected; stopping it first.',
                  flush=True)
            for pid in current:
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            if not wait_until_absent(set(current)):
                raise RuntimeError('Old Viper4Linux backend did not stop. Refusing to start a duplicate.')

        state = Path(os.environ.get('XDG_STATE_HOME', str(Path.home() / '.local/state'))) / 'viper4linux'
        state.mkdir(parents=True, exist_ok=True)
        log_path = state / 'backend.log'
        backend_start = (Path('/usr/lib/viper4linux/start-backend')
                         if SYSTEM_MODE and root == ROOT else root / 'scripts/start.sh')
        with log_path.open('ab') as log:
            proc = subprocess.Popen([str(backend_start)], cwd=root, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
                                    close_fds=True)
        # Seeing the correct process marker proves that start.sh exec'd PipeWire.
        # Stabilize briefly to catch immediately failing client startup.
        deadline = time.monotonic() + 7.0
        first_seen = None
        while time.monotonic() < deadline:
            running = running_clients()
            if proc.poll() is not None:
                raise RuntimeError(f'Backend exited at startup (code {proc.returncode}); see {log_path}')
            if len(running) == 1 and running.get(proc.pid) == desired:
                if first_seen is None:
                    first_seen = time.monotonic()
                elif time.monotonic() - first_seen >= 0.35:
                    print(f'Viper4Linux backend started (PID {proc.pid}); log: {log_path}', flush=True)
                    return proc.pid
            else:
                first_seen = None
            time.sleep(0.1)
        # Do not leave a half-started filter-chain client behind.
        if proc.poll() is None:
            proc.terminate()
        raise RuntimeError(f'Backend did not become ready; see {log_path}')



def launch_preflight(root: Path = ROOT, proc_root: Path = PROC) -> str:
    """Read-only preview of ensure_backend's initial decision.

    The live launcher takes a lock and rechecks the backend before changing
    anything. This preview is only a momentary snapshot, not a reservation.
    """
    desired = fingerprint(root)
    system = SYSTEM_MODE and root == ROOT
    gui = Path('/usr/bin/viper4linux-gui') if system else root / 'build/viper-gui'
    start = Path('/usr/lib/viper4linux/start-backend') if system else root / 'scripts/start.sh'
    for name, path in (('GUI', gui), ('backend start command', start)):
        if not path.is_file() or not os.access(path, os.X_OK):
            raise RuntimeError(f'{name} missing or not executable: {path}')
    runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
    if not runtime.is_dir():
        raise RuntimeError(f'PipeWire runtime directory does not exist: {runtime}')
    current = running_clients(proc_root)
    lines = [
        'Viper4Linux launch preflight (read-only snapshot; no actions taken):',
        f'  Build: {"system package" if system else "source checkout"}',
        f'  Launcher backend: {start}',
        f'  Backend fingerprint: {desired}',
    ]
    if not current:
        lines.append('  Action on launch: START a backend (none detected).')
    elif len(current) == 1 and next(iter(current.values())) == desired:
        pid = next(iter(current))
        lines.append(f'  Action on launch: REUSE matching backend PID {pid}.')
    else:
        pids = ', '.join(str(pid) for pid in sorted(current))
        lines.append(f'  Action on launch: REPLACE existing backend PID(s) {pids}.')
        lines.append('  WARNING: Launch would stop the existing ViPER backend, briefly interrupting audio.')
    lines.append('  No GUI or backend was started, stopped, or modified.')
    lines.append('  Process state may change before an actual launch.')
    return '\n'.join(lines)


# An Audio Output manager is a separate process, never tied to the GUI.
OUTPUT_MANAGER_TAG = b'VIPER_AUDIO_OUTPUT_MANAGER_SHA='

def running_output_managers(script: Path, proc_root: Path = PROC) -> dict[int, tuple[int, str | None]]:
    result = {}
    for process in proc_root.iterdir():
        if not process.name.isdecimal():
            continue
        try:
            if process.stat().st_uid != os.getuid():
                continue
            argv = (process / 'cmdline').read_bytes().split(b'\0')
            if os.fsencode(script) not in argv or b'--backend-pid' not in argv:
                continue
            idx = argv.index(b'--backend-pid')
            backend = int(argv[idx + 1])
            env = (process / 'environ').read_bytes().split(b'\0')
            tag = next((e[len(OUTPUT_MANAGER_TAG):].decode('ascii', errors='replace')
                        for e in env if e.startswith(OUTPUT_MANAGER_TAG)), None)
            result[int(process.name)] = (backend, tag)
        except (OSError, ValueError, IndexError):
            continue
    return result

def ensure_output_monitor(backend_pid: int, root: Path = ROOT,
                          *, force_restart: bool = False) -> int:
    script = (Path('/usr/lib/viper4linux/viper_output_monitor.py')
              if SYSTEM_MODE and root == ROOT else root / 'src/viper_output_monitor.py')
    if not script.is_file():
        raise RuntimeError(f'Audio Output monitor missing: {script}')
    desired = hashlib.sha256(script.read_bytes()).hexdigest()
    runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
    with (runtime / 'viper4linux-output-launch.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        existing = running_output_managers(script)
        if not force_restart and len(existing) == 1 and next(iter(existing.values())) == (backend_pid, desired):
            return next(iter(existing))
        if existing:
            for pid in existing:
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            deadline = time.monotonic() + 5.0
            while time.monotonic() < deadline and running_output_managers(script):
                time.sleep(0.1)
            if running_output_managers(script):
                raise RuntimeError('Previous Audio Output monitor did not stop')
        log_dir = Path(os.environ.get('XDG_STATE_HOME', str(Path.home() / '.local/state'))) / 'viper4linux'
        log_dir.mkdir(parents=True, exist_ok=True)
        env = os.environ.copy()
        env['VIPER_AUDIO_OUTPUT_MANAGER_SHA'] = desired
        with (log_dir / 'output-monitor.log').open('ab') as log:
            proc = subprocess.Popen([sys.executable, str(script), '--backend-pid', str(backend_pid)],
                                    env=env, stdin=subprocess.DEVNULL, stdout=log,
                                    stderr=subprocess.STDOUT, start_new_session=True, close_fds=True)
        time.sleep(0.25)
        if proc.poll() is not None:
            raise RuntimeError(f'Audio Output monitor exited ({proc.returncode}); see {log_dir / "output-monitor.log"}')
        return proc.pid


def wait_for_output_status(backend_pid: int, manager_pid: int,
                           since: float, timeout: float = 8.0) -> None:
    """Do not report restart success until the new monitor publishes a heartbeat."""
    runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
    status_file = runtime / 'viper4linux-output-status.json'
    script = (Path('/usr/lib/viper4linux/viper_output_monitor.py')
              if SYSTEM_MODE else ROOT / 'src/viper_output_monitor.py')
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            status = json.loads(status_file.read_text(encoding='utf-8'))
            if (status.get('format') == 1 and
                status.get('backend_pid') == backend_pid and
                status.get('manager_pid') == manager_pid and
                float(status.get('timestamp', 0)) >= since):
                return
        except (OSError, ValueError, TypeError, AttributeError):
            pass
        if manager_pid not in running_output_managers(script):
            raise RuntimeError('Audio Output monitor exited before publishing a fresh status. '
                               'See output-monitor.log.')
        time.sleep(0.1)
    raise RuntimeError('Audio Output monitor did not publish a fresh status after restart. '
                       'See output-monitor.log.')


def restart_audio_engine() -> None:
    """Replace both owned processes; never launch a second GUI or system PipeWire."""
    started = time.time()
    backend_pid = ensure_backend(force_restart=True)
    manager_pid = ensure_output_monitor(backend_pid, force_restart=True)
    wait_for_output_status(backend_pid, manager_pid, started)
    print(f'Viper4Linux audio engine restarted (backend PID {backend_pid}, '
          f'Audio Output monitor PID {manager_pid}).', flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description='Viper4Linux unified launcher')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--fingerprint', action='store_true', help='print the built backend fingerprint')
    mode.add_argument('--preflight', action='store_true',
                      help='preview backend reuse/replacement without launching or changing audio')
    mode.add_argument('--restart-audio', action='store_true',
                      help='restart the ViPER backend and Audio Output monitor without opening a GUI')
    args = parser.parse_args()
    try:
        if args.fingerprint:
            print(fingerprint())
            return 0
        if args.preflight:
            print(launch_preflight())
            return 0
        if args.restart_audio:
            restart_audio_engine()
            return 0
        gui = Path('/usr/bin/viper4linux-gui') if SYSTEM_MODE else ROOT / 'build/viper-gui'
        if not gui.is_file() or not os.access(gui, os.X_OK):
            raise RuntimeError('GUI not built. Run cmake -S . -B build && cmake --build build first.')
        backend_pid = ensure_backend()
        try:
            ensure_output_monitor(backend_pid)
        except (OSError, RuntimeError) as exc:
            print(f'viper4linux: Audio Output monitor unavailable: {exc}', file=sys.stderr)
        # Never launch another GUI if the backend could not be verified.
        os.execv(str(gui), [str(gui)])
    except (RuntimeError, OSError) as exc:
        print(f'viper4linux: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
