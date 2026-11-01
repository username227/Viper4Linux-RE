#!/usr/bin/env python3
"""Launch the matching PipeWire client once, then open the Qt GUI.

Only processes with our exact PipeWire instance marker are eligible for a
restart. The fingerprint represents the *built backend*, not merely Git HEAD.
"""
from __future__ import annotations

import argparse
import fcntl
import hashlib
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

INSTANCE = b"VIPER_CLEAN_INSTANCE=viperlinux-unified-v1"
FINGERPRINT_ENV = b"VIPER_BACKEND_FINGERPRINT="
ROOT = Path(__file__).resolve().parent.parent
PROC = Path('/proc')


def fingerprint(root: Path = ROOT) -> str:
    # Include the native DSP binary, the IPC controller and the filter-chain
    # configuration. A rebuilt backend is detected even if Git HEAD is unchanged.
    paths = [root / 'build/viper_dsp_ladspa.so', root / 'build/viper-ctl']
    paths += sorted((root / 'routing-config').rglob('*.conf'))
    if len(paths) < 3:
        raise RuntimeError('PipeWire configuration missing. Run ./install.sh first.')
    digest = hashlib.sha256(b'viper4linux-backend-fingerprint-v1\0')
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f'Missing backend build artifact: {path}. Run ./install.sh first.')
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


def ensure_backend(root: Path = ROOT) -> None:
    desired = fingerprint(root)
    runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
    if not runtime.is_dir():
        raise RuntimeError(f'PipeWire runtime directory does not exist: {runtime}')

    # Serializes launchers from different source trees for the same user.
    with (runtime / 'viper4linux-launch.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        current = running_clients()
        if current and all(tag == desired for tag in current.values()) and len(current) == 1:
            print('Viper4Linux backend already running at the current build; reusing it.', flush=True)
            return
        if current:
            print('Previous or mismatched Viper4Linux backend detected; stopping it first.', flush=True)
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
        with log_path.open('ab') as log:
            proc = subprocess.Popen([str(root / 'start.sh')], cwd=root, stdin=subprocess.DEVNULL,
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
                    return
            else:
                first_seen = None
            time.sleep(0.1)
        # Do not leave a half-started filter-chain client behind.
        if proc.poll() is None:
            proc.terminate()
        raise RuntimeError(f'Backend did not become ready; see {log_path}')


def main() -> int:
    parser = argparse.ArgumentParser(description='Viper4Linux unified launcher')
    parser.add_argument('--fingerprint', action='store_true', help='print the built backend fingerprint')
    args = parser.parse_args()
    try:
        if args.fingerprint:
            print(fingerprint())
            return 0
        gui = ROOT / 'build/viper-gui'
        if not gui.is_file() or not os.access(gui, os.X_OK):
            raise RuntimeError('GUI not built. Run ./install.sh first.')
        ensure_backend()
        # Never launch another GUI if the backend could not be verified.
        os.execv(str(gui), [str(gui)])
    except (RuntimeError, OSError) as exc:
        print(f'viper4linux: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
