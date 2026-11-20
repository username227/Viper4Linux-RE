#!/usr/bin/env python3
"""Offline contracts for the standalone Debian .deb and three Ubuntu PPA jobs."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
main_workflow = (root / '.github/workflows/prepare-stable-release.yml').read_text()
build_script = (root / 'scripts/build-native-deb.sh').read_text()
verify = root / 'scripts/verify-deb-release-source.py'
assert not (root / '.github/workflows/prepare-deb-release.yml').exists(), 'Separate Debian-only workflow must not reappear'
assert 'ubuntu24.04-mint22' not in main_workflow, 'Future releases must not duplicate Ubuntu PPA with standalone .deb'
assert 'debian:trixie' in main_workflow and 'debian13-lmde7' in main_workflow
assert '  launchpad-source:' in main_workflow
assert all(series in main_workflow for series in ('series: noble', 'series: resolute', 'series: stonking'))
assert 'ppa-${{ inputs.version }}-${{ matrix.series }}-unsigned-REVIEW' in main_workflow
assert 'git ca-certificates' in main_workflow, 'PPA build containers must install git'
assert 'actions/upload-artifact@v4' in main_workflow
assert 'needs: prepare' in main_workflow, 'Main stable release must build .deb after Arch source preparation'
assert 'actions/download-artifact@v4' in main_workflow
assert 'debian:trixie' in main_workflow and 'ubuntu:24.04' in main_workflow
assert 'scripts/build-native-deb.sh' in main_workflow
assert 'actions/checkout@v4' in main_workflow
assert '  assemble:' in main_workflow and 'needs: [prepare, deb, launchpad-source]' in main_workflow
assert 'SOURCE-COMMIT' in main_workflow and 'GITHUB_SHA' in main_workflow
assert 'complete-release-candidate' in main_workflow
assert 'scripts/assemble-release-assets.sh' in main_workflow
assert 'sha256sum --check ARCH-SHA256SUMS' in main_workflow
assert 'sha256sum --check SHA256SUMS' in main_workflow
assemble_script = (root / 'scripts/assemble-release-assets.sh').read_text()
assert 'sha256sum -- "$archive"' in assemble_script
assert 'rm -- "$out/SHA256SUMS" "$out/ARCH-SHA256SUMS"' in assemble_script
assert 'DEB-SHA256SUMS-' not in assemble_script
assert 'test "$(wc -l < SHA256SUMS)" -eq 3' in assemble_script
assert 'ubuntu24.04-mint22' not in assemble_script
assert 'gh release create' not in main_workflow and 'git push' not in main_workflow
# CMake is the only production application staging and DSP preparation path.
assert 'DESTDIR="$pkgroot" cmake --install "$built"' in build_script
assert '-DVIPER_ENABLE_SYSTEM_INSTALL=ON' in build_script
assert '-DVIPER_SYSTEM_INSTALL_LAYOUT=debian' in build_script
assert 'VIPER_BUILD_DEVELOPER_TESTS' not in build_script
# Full offline CTests include checks that invoke git in temporary repositories.
# Require git in the *container's build dependencies*, not the installed package.
_build_deps = build_script.split('apt-get install -y --no-install-recommends', 1)[1].split('work=$(mktemp', 1)[0].split()
assert 'git' in _build_deps, 'Debian build container must install git for offline CTests'
assert 'ctest --test-dir "$built" --output-on-failure --no-tests=error' in build_script
assert 'bash tests/check-mock.sh' not in build_script
assert 'VIPER_DEB_STAGING_BACKEND' not in build_script
assert 'ViPERDSP-patched' not in build_script
assert 'install -Dm755' not in build_script
assert 'dpkg-shlibdeps -O' in build_script
assert 'dpkg-deb --build --root-owner-group' in build_script
assert 'apt-get install -y --no-install-recommends "$package"' in build_script
assert 'viper4linux --fingerprint' in build_script
assert 'usr/lib/ladspa/viper_dsp_ladspa.so' in build_script
assert 'qt6-qpa-plugins' in build_script
assert 'v4l-cli --help >/dev/null' in build_script

with tempfile.TemporaryDirectory(prefix='viper-deb-verifier-') as d:
    work = Path(d)
    version = '1.0.0'
    app_sha = 'f5380ea0296322d5885794a3ee474a4d99595553'
    upstream_sha = '6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933'
    filename = f'viper4linux-re-{version}-unified-source.tar.xz'
    archive = work / filename
    contents = work / f'viper4linux-re-{version}'
    contents.mkdir()
    (contents / 'SOURCE-MANIFEST.txt').write_text(
        f'ViPERDSP commit: {upstream_sha}\nApplication commit: {app_sha}\n')
    with tarfile.open(archive, 'w:xz') as f:
        f.add(contents, arcname=contents.name)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    release_json = work / 'release.json'
    release_json.write_text(json.dumps({'tag_name': f'v{version}', 'draft': False,
        'prerelease': False,
        'assets': [{'name': filename, 'digest': f'sha256:{digest}'}]}))
    expected = subprocess.run([sys.executable, verify, version, release_json],
                              text=True, capture_output=True, check=True).stdout
    assert expected == f'{digest}  {filename}\n'
    result = subprocess.run([sys.executable, verify, version, release_json, archive, app_sha],
                            text=True, capture_output=True, check=True)
    assert 'PASS:' in result.stdout
    bad = subprocess.run([sys.executable, verify, version, release_json, archive, '0'*40],
                         text=True, capture_output=True)
    assert bad.returncode != 0 and 'differs from release tag' in bad.stderr
    release = json.loads(release_json.read_text())
    release['assets'][0]['digest'] = 'sha256:'+'0'*64
    release_json.write_text(json.dumps(release))
    bad = subprocess.run([sys.executable, verify, version, release_json, archive, app_sha],
                         text=True, capture_output=True)
    assert bad.returncode != 0 and 'does not match' in bad.stderr
print('PASS: distro matrix, source verification, release safety and native package contract')
