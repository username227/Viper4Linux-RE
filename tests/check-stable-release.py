#!/usr/bin/env python3
"""End-to-end offline release preparation test using a local Git/DSP fixture."""
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]


def run(*args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(f'{args}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')
    return result.stdout


# Release packaging must preserve the main Arch package from makepkg and
# upload it alongside the unified source. The debug-symbol package is not a
# release asset; these checks guard against accidentally distributing it.
workflow = (root / '.github/workflows/prepare-stable-release.yml').read_text()
arch_build = (root / 'scripts/test-stable-arch.sh').read_text()
assert 'dist/viper4linux-re-${{ inputs.version }}-*.pkg.tar.zst' in workflow, 'Main Arch package missing from upload-artifact'
assert 'dist/*.pkg.tar.zst' not in workflow, 'Wildcard would upload debug package'
assert 'dist/ARCH-SHA256SUMS' in workflow, 'Arch binary checksums missing from upload-artifact'
assert 'sha256sum --check ARCH-SHA256SUMS' in workflow, 'Arch checksum validation missing'
assert 'ARCH-SHA256SUMS' in arch_build, 'Arch job does not generate checksum file'
assert 'install -m644 -- "$package" "$release_dir/$name"' in arch_build, 'Main Arch package not staged'
assert '"${debug_packages[@]}"' not in arch_build, 'Debug package must not be staged'
assert '  assemble:' in workflow and 'needs: [prepare, deb]' in workflow, 'Unified release assembly missing'
assert 'viper4linux-re-${{ inputs.version }}-complete-release-candidate' in workflow
assert 'scripts/assemble-release-assets.sh' in workflow
assert 'scripts/assemble-release-assets.sh "$VERSION" "$GITHUB_SHA"' in workflow

with tempfile.TemporaryDirectory(prefix='viper-release-e2e-') as temp:
    temp = Path(temp)
    app, dsp, out = (temp / x for x in ('app', 'ViPERDSP', 'out'))
    shutil.copytree(root, app, ignore=shutil.ignore_patterns('.git', '__pycache__', 'build', '*.pyc'))
    dsp.mkdir()
    out.mkdir()
    # In the exported unified-source CI job the DSP directory contains real
    # files, whereas a Git checkout may contain only its submodule directory.
    # The fixture deliberately replaces either with a local test submodule.
    shutil.rmtree(app / "third_party/ViPERDSP")
    (dsp / 'CMakeLists.txt').write_text('project(ViPERDSP LANGUAGES CXX)\n')
    run('git', 'init', '-q', dsp)
    run('git', 'add', '.', cwd=dsp)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'upstream fixture', cwd=dsp)
    run('git', 'init', '-q', app)
    run('git', 'add', '.', cwd=app)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'app fixture', cwd=app)
    run('git', '-c', 'protocol.file.allow=always', 'submodule', 'add', '-q',
        str(dsp), 'third_party/ViPERDSP', cwd=app)
    run('git', 'add', '.', cwd=app)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'pin fixture DSP', cwd=app)
    output = run('bash', 'scripts/prepare-stable-release.sh', '1.1.0', str(out), cwd=app)
    assert 'Release assets prepared' in output, output
    assert (out / 'SOURCE-COMMIT').read_text().strip() == run('git', 'rev-parse', 'HEAD', cwd=app).strip()
    assert "pkgver=1.1.0\n" in (out / 'PKGBUILD').read_text()
    assert run('sha256sum', '-c', 'SHA256SUMS', cwd=out).strip().endswith(': OK')
    with tarfile.open(out / 'viper4linux-re-1.1.0-unified-source.tar.xz') as tar:
        assert 'viper4linux-re-1.1.0/third_party/ViPERDSP/CMakeLists.txt' in tar.getnames()
        assert tar.extractfile('viper4linux-re-1.1.0/RELEASE-VERSION').read() == b'1.1.0\n'
        assert 'viper4linux-re-1.1.0/cmake/Version.cmake' in tar.getnames()
    # Fail rather than silently publishing the wrong revision under an existing tag.
    run('git', 'tag', 'v1.1.0', cwd=app)
    (app / 'README.md').write_text((app / 'README.md').read_text() + '\nfixture change\n')
    run('git', 'add', 'README.md', cwd=app)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'past tag', cwd=app)
    bad = subprocess.run(['bash', 'scripts/prepare-stable-release.sh', '1.1.0', str(temp / 'wrong')],
                         cwd=app, text=True, capture_output=True)
    assert bad.returncode != 0 and 'different commit' in bad.stderr
    print('PASS: end-to-end offline release assets, SHA256, packaged source and tag/HEAD safety')
