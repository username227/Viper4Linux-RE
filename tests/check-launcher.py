#!/usr/bin/env python3
"""Offline launcher checks; never touch actual system PipeWire."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import Mock, patch
import os
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
import viper_launcher as v

with TemporaryDirectory() as tmp:
    root = Path(tmp)
    (root / 'build').mkdir()
    (root / 'routing-config/subdir').mkdir(parents=True)
    (root / 'build/viper_dsp_ladspa.so').write_bytes(b'backend version A')
    (root / 'build/viper-ctl').write_bytes(b'ipc version A')
    (root / 'routing-config/subdir/viper.conf').write_bytes(b'config A')
    a = v.fingerprint(root)
    assert len(a) == 64 and a == v.fingerprint(root)
    (root / 'build/viper_dsp_ladspa.so').write_bytes(b'backend version B')
    assert a != v.fingerprint(root), 'same Git commit, changed binary must trigger upgrade'
    b = v.fingerprint(root)
    (root / 'routing-config/subdir/viper.conf').write_bytes(b'config B')
    assert b != v.fingerprint(root), 'changed PipeWire config must trigger upgrade'
    c = v.fingerprint(root)
    (root / 'build/viper-ctl').write_bytes(b'ipc version B')
    assert c != v.fingerprint(root), 'changed controller must trigger upgrade'
    desired = v.fingerprint(root)

    proc = root / 'fake-proc'
    proc.mkdir()
    def client(pid, marker=True, fingerprint=None, command=b'pipewire\0-c\0filter-chain.conf\0'):
        p = proc / str(pid)
        p.mkdir()
        (p / 'cmdline').write_bytes(command)
        env = [v.INSTANCE] if marker else [b'NOT_VIPER=1']
        if fingerprint:
            env.append(v.FINGERPRINT_ENV + fingerprint.encode())
        (p / 'environ').write_bytes(b'\0'.join(env) + b'\0')
    client(100, fingerprint=desired)
    client(101)  # older Viper4Linux backend without a fingerprint
    client(102, marker=False)
    client(103, command=b'firefox\0-c\0filter-chain.conf\0')
    assert v.running_clients(proc) == {100: desired, 101: None}

    runtime = root / 'runtime'
    runtime.mkdir()
    state = root / 'state'
    fake_env = {'XDG_RUNTIME_DIR': str(runtime), 'XDG_STATE_HOME': str(state)}
    # Matching backend is reused without launching a second one.
    with patch.dict(os.environ, fake_env), patch.object(v, 'running_clients', return_value={111: desired}), \
            patch.object(v.subprocess, 'Popen') as spawn, patch.object(v.os, 'kill') as kill:
        v.ensure_backend(root)
        spawn.assert_not_called()
        kill.assert_not_called()

    # Old backend must exit before the new client is spawned.
    events = []
    new_proc = Mock()
    new_proc.pid = 444
    new_proc.poll.return_value = None
    def spawn_new(*a, **kw):
        events.append('spawn')
        return new_proc
    def stop_old(pid, sig):
        events.append('kill')
        assert pid == 333 and sig == v.signal.SIGTERM
    counts = iter([{333: None}] + [{444: desired}] * 40)
    with patch.dict(os.environ, fake_env), patch.object(v, 'running_clients', side_effect=lambda: next(counts, {444: desired})), \
            patch.object(v, 'wait_until_absent', return_value=True) as wait, \
            patch.object(v.os, 'kill', side_effect=stop_old), \
            patch.object(v.subprocess, 'Popen', side_effect=spawn_new):
        v.ensure_backend(root)
        assert events == ['kill', 'spawn'], events
        wait.assert_called_once_with({333})

    # If the old backend refuses to stop, never launch two clients.
    with patch.dict(os.environ, fake_env), patch.object(v, 'running_clients', return_value={333: None}), \
            patch.object(v, 'wait_until_absent', return_value=False), \
            patch.object(v.os, 'kill'), patch.object(v.subprocess, 'Popen') as spawn:
        try:
            v.ensure_backend(root)
        except RuntimeError as err:
            assert 'Refusing to start a duplicate' in str(err)
        else:
            raise AssertionError('should refuse a duplicate backend')
        spawn.assert_not_called()

print('PASS: launcher fingerprints build, reuses matching backend, restarts old one, refuses duplicates')
