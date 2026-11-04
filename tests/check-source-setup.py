#!/usr/bin/env python3
"""Isolated source setup contract; no host installation, launcher or audio."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/setup-source-checkout.sh'


def invoke(path, action, env):
    return subprocess.run(['bash', str(path), action], env=env,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


with tempfile.TemporaryDirectory(prefix='viper source setup ') as tmp:
    work = Path(tmp)
    tree = work / 'source checkout'
    (tree / 'scripts').mkdir(parents=True)
    (tree / 'routing-config').mkdir()
    (tree / 'build').mkdir()
    script = tree / 'scripts/setup-source-checkout.sh'
    shutil.copyfile(SCRIPT, script)
    template = work / 'system filter chain.conf'
    template.write_text('PipeWire source fixture\n')
    bindir = work / 'private bin'
    env = {**os.environ, 'VIPER_PIPEWIRE_FILTER_TEMPLATE': str(template),
           'XDG_BIN_HOME': str(bindir)}
    missing = invoke(script, '--check', env)
    assert missing.returncode != 0 and 'Missing source build output' in missing.stderr
    assert not bindir.exists()

    files = ('viper-gui', 'viper-ctl', 'viper-output-state', 'viper_dsp_ladspa.so')
    for name in files:
        f = tree / 'build' / name
        f.write_text('fixture')
        f.chmod(0o755)
    launcher = tree / 'viper4linux'
    launcher.write_text('#!/bin/sh\nexit 0\n')
    launcher.chmod(0o755)

    assert invoke(script, '--check', env).returncode != 0
    assert invoke(script, '--prepare-routing', env).returncode == 0
    config = tree / 'routing-config/filter-chain.conf'
    assert config.read_text() == template.read_text()
    config.write_text('user-customized and preserved\n')
    assert invoke(script, '--prepare-routing', env).returncode == 0
    assert config.read_text() == 'user-customized and preserved\n'
    assert invoke(script, '--check', env).returncode == 0

    # A symlink must never redirect writes to a file outside the checkout.
    config.unlink()
    external = work / 'outside.txt'
    external.write_text('leave me alone\n')
    config.symlink_to(external)
    failed = invoke(script, '--prepare-routing', env)
    assert failed.returncode != 0 and 'Refusing to replace symlink' in failed.stderr
    assert external.read_text() == 'leave me alone\n'
    config.unlink()
    config.write_text('source routing fixture\n')

    assert invoke(script, '--link-commands', env).returncode == 0
    assert os.readlink(bindir / 'viper4linux') == str(launcher)
    assert os.readlink(bindir / 'v4l-cli') == str(tree / 'build/viper-ctl')
    assert invoke(script, '--link-commands', env).returncode == 0

    # Preflight must reject collisions before creating either alias.
    conflicting = work / 'collision bin'
    conflicting.mkdir()
    (conflicting / 'viper4linux').write_text('user executable\n')
    collision_env = {**env, 'XDG_BIN_HOME': str(conflicting)}
    failed = invoke(script, '--link-commands', collision_env)
    assert failed.returncode != 0 and 'Unrelated command exists' in failed.stderr
    assert not (conflicting / 'v4l-cli').exists()
    assert (conflicting / 'viper4linux').read_text() == 'user executable\n'

    # Relative or system paths must be rejected even with complete build output.
    for bad in ('relative-bin', '/usr/bin', '/usr/local/bin'):
        failed = invoke(script, '--link-commands', {**env, 'XDG_BIN_HOME': bad})
        assert failed.returncode != 0 and 'Refusing non-private' in failed.stderr

    # Missing native build never creates a bin directory or runs a launcher.
    (tree / 'build/viper-ctl').unlink()
    absent = work / 'no-build bin'
    failed = invoke(script, '--link-commands', {**env, 'XDG_BIN_HOME': str(absent)})
    assert failed.returncode != 0 and 'Missing source build output' in failed.stderr
    assert not absent.exists()

print('PASS: source checkout routing, repeated setup, symlink safety, and private command alias checks')
