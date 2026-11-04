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
    (root / 'routing-config/filter-chain.conf').write_bytes(b'stock config')
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
    # New CMake checkouts use build-local routing until a custom source
    # filter-chain file is explicitly supplied.
    custom_file = root / 'routing-config/filter-chain.conf'
    custom_file.unlink()
    built_routing = root / 'build/routing-config'
    (built_routing / 'filter-chain.conf.d').mkdir(parents=True)
    (built_routing / 'filter-chain.conf').write_bytes(b'CMake stock template A')
    (built_routing / 'filter-chain.conf.d/99-viper-smart.conf').write_bytes(b'smart source config')
    assert v.source_routing_dir(root) == built_routing
    routed_a = v.fingerprint(root)
    (built_routing / 'filter-chain.conf').write_bytes(b'CMake stock template B')
    assert v.fingerprint(root) != routed_a, 'CMake routing update must change fingerprint'
    custom_file.write_bytes(b'user custom config')
    assert v.source_routing_dir(root) == root / 'routing-config'
    assert v.fingerprint(root) != routed_a, 'User custom source routing must take priority'
    custom_file.unlink()
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
        # Verify actual subprocess argument, not just the source text.
        assert a[0] == [str(root / 'scripts/start.sh')], a
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

# All five relocated scripts must exist and remain executable in a checkout.
# This runs from any working directory; it does not start audio.
checkout = Path(__file__).resolve().parents[1]
for name in ('start.sh', 'stop.sh', 'status.sh', 'gui.sh', 'verify.sh'):
    relocated = checkout / 'scripts' / name
    assert relocated.is_file(), relocated
    assert os.access(relocated, os.X_OK), relocated
    assert not (checkout / name).exists(), name
source_launcher = (checkout / 'src/viper_launcher.py').read_text()
assert "else root / 'scripts/start.sh'" in source_launcher
assert "else root / 'start.sh'" not in source_launcher

print('PASS: launcher fingerprints build, reuses matching backend, restarts old one, refuses duplicates')
