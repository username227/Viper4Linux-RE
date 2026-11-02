#!/usr/bin/env python3
"""Exercise the combined native release bundle and its failure guards offline."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]
script = root / 'scripts/assemble-release-assets.sh'
version = '1.0.0'
sha = 'f5380ea0296322d5885794a3ee474a4d99595553'


def write_checksum(directory, fname, checksum_file):
    digest = hashlib.sha256((directory / fname).read_bytes()).hexdigest()
    (directory / checksum_file).write_text(f'{digest}  {fname}\n')


def invoke(artifacts, output, commit=sha):
    return subprocess.run(['bash', script, version, commit, str(artifacts), str(output)],
                          text=True, capture_output=True)


with tempfile.TemporaryDirectory(prefix='viper-assembled-native-') as d:
    temp = Path(d)
    artifacts = temp / 'collected'
    arch = artifacts / f'viper4linux-re-{version}-release-candidate'
    arch.mkdir(parents=True)
    (arch / 'SOURCE-COMMIT').write_text(f'{sha}\n')
    (arch / 'PKGBUILD').write_text(f'pkgver={version}\n')
    (arch / '.SRCINFO').write_text(f'pkgver = {version}\n')
    source = f'viper4linux-re-{version}-unified-source.tar.xz'
    manifest = temp / 'SOURCE-MANIFEST.txt'
    manifest.write_text(f'Application commit: {sha}\nViPERDSP commit: {"1"*40}\n')
    with tarfile.open(arch / source, 'w:xz') as tar:
        tar.add(manifest, arcname=f'viper4linux-re-{version}/SOURCE-MANIFEST.txt')
    write_checksum(arch, source, 'SHA256SUMS')
    arch_filename = f'viper4linux-re-{version}-1-x86_64.pkg.tar.zst'
    (arch / arch_filename).write_bytes(b'mock arch binary payload')
    write_checksum(arch, arch_filename, 'ARCH-SHA256SUMS')
    debs = {}
    for target, suffix in [('ubuntu24.04-mint22', '1~ubuntu24.04mint22'),
                           ('debian13-lmde7', '1~debian13lmde7')]:
        directory = artifacts / f'viper4linux-re-{version}-{target}-deb'
        directory.mkdir()
        filename = f'viper4linux-re_{version}-{suffix}_amd64.deb'
        (directory / filename).write_bytes(f'mock {target} package'.encode())
        write_checksum(directory, filename, 'SHA256SUMS')
        debs[target] = directory / filename

    output = temp / 'complete'
    res = invoke(artifacts, output)
    assert res.returncode == 0, res.stderr + res.stdout
    assert 'PASS:' in res.stdout
    assert len(list(output.glob('*.deb'))) == 2
    assert (output / arch_filename).exists()
    assert (output / '.SRCINFO').exists()
    assert all((output / f'DEB-SHA256SUMS-{target}').is_file() for target in debs)

    res = invoke(artifacts, temp / 'bad-commit', '0'*40)
    assert res.returncode != 0 and 'Source commit does not match' in res.stderr

    original = debs['debian13-lmde7'].read_bytes()
    debs['debian13-lmde7'].write_bytes(b'modified Debian payload')
    res = invoke(artifacts, temp / 'bad-hash')
    assert res.returncode != 0 and 'FAILED' in res.stdout
    debs['debian13-lmde7'].write_bytes(original)

    (arch / 'viper4linux-re-debug-1.0.0-1-x86_64.pkg.tar.zst').write_bytes(b'not for distribution')
    res = invoke(artifacts, temp / 'no-debug')
    assert res.returncode != 0 and 'Debug-symbol package' in res.stderr

print('PASS: complete release bundle, three packages, checksum enforcement, revision and debug exclusion')
