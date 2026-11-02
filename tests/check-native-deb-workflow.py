#!/usr/bin/env python3
"""Offline checks for the native Ubuntu/Mint and Debian/LMDE .deb workflow."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
workflow = (root / '.github/workflows/prepare-deb-release.yml').read_text()
main_workflow = (root / '.github/workflows/prepare-stable-release.yml').read_text()
build_script = (root / 'scripts/build-native-deb.sh').read_text()
verify = root / 'scripts/verify-deb-release-source.py'
assert 'ubuntu:24.04' in workflow and 'ubuntu24.04-mint22' in workflow
assert 'debian:trixie' in workflow and 'debian13-lmde7' in workflow
assert 'actions/upload-artifact@v4' in workflow
assert 'needs: prepare' in main_workflow, 'Main stable release must build .deb after Arch source preparation'
assert 'actions/download-artifact@v4' in main_workflow
assert 'debian:trixie' in main_workflow and 'ubuntu:24.04' in main_workflow
assert 'scripts/build-native-deb.sh' in main_workflow
assert 'actions/checkout@v4' in workflow
assert 'release.json' in workflow and 'EXPECTED-SHA256' in workflow
assert 'download/v${VERSION}' in workflow, 'Must build from published unified source'
assert 'gh release create' not in workflow and 'git push' not in workflow
assert 'dpkg-shlibdeps -O' in build_script
assert 'dpkg-deb --build --root-owner-group' in build_script
assert 'apt-get install -y --no-install-recommends "$package"' in build_script
assert 'python3 /usr/lib/viper4linux/viper_launcher.py --fingerprint' in build_script
assert 'VIPERDSP_SOURCE_DIR=' in build_script
assert 'usr/lib/ladspa/viper_dsp_ladspa.so' in build_script
assert 'qt6-qpa-plugins' in build_script

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
