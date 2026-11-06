#!/usr/bin/env python3
"""Offline recovery tests. Never contact or signal a real audio process."""
import json
import os
from pathlib import Path
import sys
import time
from tempfile import TemporaryDirectory
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
import viper_launcher as v


with TemporaryDirectory(prefix='viper-audio-restart-') as tmp:
    base = Path(tmp)
    runtime = base / 'runtime'
    runtime.mkdir()
    state = base / 'state'
    project = base / 'checkout'
    (project / 'scripts').mkdir(parents=True)
    (project / 'src').mkdir()
    (project / 'src/viper_output_monitor.py').write_text('# monitor stub\n')
    (project / 'scripts/start.sh').write_text('#!/bin/sh\nexit 0\n')
    environment = {'XDG_RUNTIME_DIR': str(runtime), 'XDG_STATE_HOME': str(state)}
    desired = 'current-build-fingerprint'

    # Explicit recovery must restart even an already-current backend; the
    # ordinary launch path still reuses that process (check-launcher.py).
    backend = Mock(pid=654)
    backend.poll.return_value = None
    states = iter([{321: desired}] + [{654: desired}] * 40)
    with patch.dict(os.environ, environment), \
            patch.object(v, 'fingerprint', return_value=desired), \
            patch.object(v, 'running_clients',
                         side_effect=lambda: next(states, {654: desired})), \
            patch.object(v, 'wait_until_absent', return_value=True) as waited, \
            patch.object(v.os, 'kill') as kill, \
            patch.object(v.subprocess, 'Popen', return_value=backend) as spawned:
        assert v.ensure_backend(project, force_restart=True) == 654
        kill.assert_called_once_with(321, v.signal.SIGTERM)
        waited.assert_called_once_with({321})
        assert spawned.call_args.args[0] == [str(project / 'scripts/start.sh')]

    # Force-restarting the Audio Output manager must not reuse a currently
    # matching manager. It must terminate the old one before creating a new one.
    manager = Mock(pid=999)
    manager.poll.return_value = None
    seen = iter([{777: (654, 'matching-hash')}, {}, {}])
    with patch.dict(os.environ, environment), \
            patch.object(v, 'running_output_managers',
                         side_effect=lambda script: next(seen, {})), \
            patch.object(v.os, 'kill') as kill, \
            patch.object(v.subprocess, 'Popen', return_value=manager) as spawned:
        assert v.ensure_output_monitor(654, project, force_restart=True) == 999
        kill.assert_called_once_with(777, v.signal.SIGTERM)
        assert spawned.call_args.args[0][-3:] == [
            str(project / 'src/viper_output_monitor.py'), '--backend-pid', '654']

    # A matching heartbeat proves the newly launched manager reached its
    # status loop. A stale or missing manager must never count as success.
    status_file = runtime / 'viper4linux-output-status.json'
    status_file.write_text(json.dumps({
        'format': 1, 'timestamp': time.time(),
        'manager_pid': 999, 'backend_pid': 654,
    }))
    with patch.dict(os.environ, environment):
        v.wait_for_output_status(654, 999, time.time() - 1, timeout=0.2)
        with patch.object(v, 'running_output_managers', return_value={}):
            try:
                v.wait_for_output_status(654, 999, time.time() + 50, timeout=0.2)
            except RuntimeError as exc:
                assert 'exited before publishing' in str(exc)
            else:
                raise AssertionError('Stale monitor status accepted as live')

# Recovery is a separate no-GUI launcher mode. Both processes are forced to
# restart; the status must be verified before success is returned.
with patch.object(v, 'ensure_backend', return_value=321) as backend, \
        patch.object(v, 'ensure_output_monitor', return_value=456) as monitor, \
        patch.object(v, 'wait_for_output_status') as heartbeat:
    v.restart_audio_engine()
    backend.assert_called_once_with(force_restart=True)
    monitor.assert_called_once_with(321, force_restart=True)
    assert heartbeat.call_args.args[:2] == (321, 456)

with patch.object(v, 'restart_audio_engine') as restart, \
        patch.object(v, 'ensure_backend') as backend, \
        patch.object(v.os, 'execv') as exec_gui, \
        patch.object(sys, 'argv', ['viper4linux', '--restart-audio']):
    assert v.main() == 0
    restart.assert_called_once_with()
    backend.assert_not_called()
    exec_gui.assert_not_called()

# Both UI entry points must invoke the SAME asynchronous handler, with a
# disabled re-entry path and no stale-IPC writes during backend replacement.
gui = (ROOT / 'src/viper-gui.cpp').read_text()
shell = (ROOT / 'src/window/window-shell.inc').read_text()
build_info = (ROOT / 'src/viper-build-info.h.in').read_text()
assert 'viperRestartAudioButton' in gui
assert 'Restart Audio Engine' in shell
assert gui.count('[this] { restartAudioEngine(); }') == 1
assert shell.count('[this] { restartAudioEngine(); }') == 1
assert 'process->start();' in shell and 'process->waitForFinished(' not in shell
assert 'QStringLiteral("--restart-audio")' in shell
assert 'if (audio_restart_process_) return;' in gui
assert 'pending_edit_ = true; // Send edits to the new backend' in gui
assert 'VIPER4LINUX_SOURCE_LAUNCHER' in build_info
assert 'mapping_.close();' in shell
assert 'updateAudioOutputDisplay();' in shell and 'pollBackend();' in shell

print('PASS: explicit audio-engine restart, monitor heartbeat and GUI recovery contracts')
