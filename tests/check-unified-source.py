#!/usr/bin/env python3
"""Offline Git fixture tests for the unified-source release artifact."""
from pathlib import Path
import os
import subprocess
import tarfile
import tempfile

script = Path(__file__).resolve().parents[1] / 'scripts/make-unified-source.sh'

def run(*cmd, cwd=None):
    try:
        return subprocess.check_output(cmd, cwd=cwd, stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f'Command {cmd} failed:\n{exc.output}') from exc

with tempfile.TemporaryDirectory(prefix='v4l-release-test-') as tmp:
    tmp = Path(tmp)
    upstream = tmp / 'upstream'
    app = tmp / 'app'
    output = tmp / 'output'
    upstream.mkdir()
    app.mkdir()
    output.mkdir()
    run('git', 'init', '-q', str(upstream))
    (upstream / 'CMakeLists.txt').write_text('project(ViPERDSP)\n')
    run('git', 'add', '.', cwd=upstream)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com', 'commit', '-qm', 'upstream', cwd=upstream)
    upstream_sha = run('git', 'rev-parse', 'HEAD', cwd=upstream).strip()
    run('git', 'init', '-q', str(app))
    (app / 'README.md').write_text('Development README\n')
    (app / 'scripts').mkdir()
    (app / 'scripts/make-unified-source.sh').write_bytes(script.read_bytes())
    (app / 'scripts/make-unified-source.sh').chmod(0o755)
    run('git', 'add', '.', cwd=app)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com', 'commit', '-qm', 'app', cwd=app)
    run('git', '-c', 'protocol.file.allow=always', 'submodule', 'add', '-q', str(upstream), 'third_party/ViPERDSP', cwd=app)
    run('git', 'add', '.', cwd=app)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com', 'commit', '-qm', 'submodule', cwd=app)
    output_message = run('bash', str(app / 'scripts/make-unified-source.sh'), '0.1.0', str(output), cwd=app)
    name = output / 'viper4linux-re-0.1.0-unified-source.tar.xz'
    assert name.is_file(), output_message
    with tarfile.open(name, 'r:xz') as tar:
        members = tar.getnames()
        base = 'viper4linux-re-0.1.0/'
        assert base + 'third_party/ViPERDSP/CMakeLists.txt' in members
        assert base + 'RELEASE-REVISION' in members
        assert base + 'SOURCE-MANIFEST.txt' in members
        assert all('/.git/' not in n and not n.endswith('/.git') for n in members)
        assert base + '.gitmodules' not in members
        manifest = tar.extractfile(base + 'SOURCE-MANIFEST.txt').read().decode()
        assert upstream_sha in manifest
    print('PASS: unified source archive includes recorded upstream revision and no Git metadata')
