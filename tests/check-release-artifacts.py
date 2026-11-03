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
                    'RELEASE-VERSION', 'cmake/Version.cmake',
                    'third_party/ViPERDSP/CMakeLists.txt', 'src/viper-gui.cpp',
                    'src/gui-single-instance.h',
                    'src/window/window-presets.inc',
                    'src/window/window-layout.inc',
                    'src/window/window-correction.inc',
                    'src/window/window-shell.inc',
                    'src/window/window-graphic-eq.inc',
                    'src/window/window-dynamics.inc',
                    'src/window/window-effect-actions.inc',
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
        release_version = archive.extractfile(prefix + 'RELEASE-VERSION').read().decode().strip()
        assert release_version == version, 'Archive release version mismatch'
        cmake = archive.extractfile(prefix + 'CMakeLists.txt').read().decode()
        assert 'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake")' in cmake
        assert 'set(V4L_BASE_VERSION "' not in cmake, 'Stale hardcoded CMake release version'
        revision = archive.extractfile(prefix + 'RELEASE-REVISION').read().decode().strip()
        assert re.fullmatch(r'[0-9a-f]{12,40}', revision), 'Invalid release revision'
        assert f'Application commit: ' in manifest and revision in manifest
    print(f'PASS: unified source, SemVer version, manifest, and SHA256: {digest}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('Usage: check-release-artifacts.py VERSION DIST_DIRECTORY')
    verify(sys.argv[1], Path(sys.argv[2]))
