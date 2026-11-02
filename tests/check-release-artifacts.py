#!/usr/bin/env python3
"""Verify archive provenance, contents, checksums and generated PKGBUILD."""
import hashlib
from pathlib import Path
import re
import sys
import tarfile


def verify(version: str, dest: Path):
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid stable release version')
    basename = f'viper4linux-re-{version}-unified-source.tar.xz'
    tarpath = dest / basename
    pkg = (dest / 'PKGBUILD').read_text()
    digest = hashlib.file_digest(tarpath.open('rb'), 'sha256').hexdigest()
    expected_line = f'{digest}  {basename}'
    assert (dest / 'SHA256SUMS').read_text().strip() == expected_line
    assert f'pkgver={version}\n' in pkg
    assert f"sha256sums=('{digest}')" in pkg
    assert f'/releases/download/v${{pkgver}}/{basename.replace(version, "${pkgver}")}' in pkg
    assert 'REPLACE_WITH_RELEASE_SHA256' not in pkg
    assert 'git submodule' not in pkg
    prefix = f'viper4linux-re-{version}/'
    with tarfile.open(tarpath, 'r:xz') as archive:
        members = set(archive.getnames())
        required = {'README.md', 'LICENSE', 'THIRD-PARTY-NOTICES.md',
                    'CMakeLists.txt', 'SOURCE-MANIFEST.txt', 'RELEASE-REVISION',
                    'third_party/ViPERDSP/CMakeLists.txt', 'src/viper-gui.cpp',
                    'src/viper_output_monitor.py', 'src/viper-output-state.cpp',
                    'packaging/arch/viper4linux-output', 'packaging/arch/install-package.sh',
                    'man/viper4linux.1', 'man/viper4linux-ctl.1'}
        missing = sorted(prefix + path for path in required if prefix + path not in members)
        assert not missing, f'Missing source files: {missing}'
        assert not any(name.endswith('/.git') or '/.git/' in name or name.endswith('/.gitmodules')
                       for name in members), 'Archive contains Git metadata'
        manifest = archive.extractfile(prefix + 'SOURCE-MANIFEST.txt').read().decode()
        assert re.search(r'ViPERDSP commit: [0-9a-f]{40}', manifest)
        assert re.search(r'Application commit: [0-9a-f]{40}', manifest)
        cmake = archive.extractfile(prefix + 'CMakeLists.txt').read().decode()
        assert f'set(V4L_BASE_VERSION "{version}")' in cmake, 'CMake version mismatch'
    print(f'PASS: unified source, 1.0.0-style version, manifest, and SHA256: {digest}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('Usage: check-release-artifacts.py VERSION DIST_DIRECTORY')
    verify(sys.argv[1], Path(sys.argv[2]))
