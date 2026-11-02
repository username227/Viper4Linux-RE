#!/usr/bin/env python3
"""Pin a DEB build to the actual published unified source and Git release tag."""
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile

if len(sys.argv) not in (3, 5):
    raise SystemExit('Usage: verify-deb-release-source.py VERSION RELEASE_JSON [TARBALL TAG_SHA]')
version, metadata = sys.argv[1:3]
if not re.fullmatch(r'\d+\.\d+\.\d+', version):
    raise SystemExit('Invalid release version')
release = json.loads(Path(metadata).read_text())
if release.get('tag_name') != f'v{version}' or release.get('draft') or release.get('prerelease'):
    raise SystemExit('Expected a published, non-prerelease tagged release')
filename = f'viper4linux-re-{version}-unified-source.tar.xz'
assets = [a for a in release.get('assets', []) if a.get('name') == filename]
if len(assets) != 1 or not re.fullmatch(r'sha256:[0-9a-f]{64}', assets[0].get('digest', '')):
    raise SystemExit('Release source asset or SHA256 metadata missing')
expected = assets[0]['digest'][len('sha256:'):]
if len(sys.argv) == 3:
    print(f'{expected}  {filename}')
    raise SystemExit(0)
archive = Path(sys.argv[3]); tag_sha = sys.argv[4]
if not re.fullmatch('[0-9a-f]{40}', tag_sha):
    raise SystemExit('Invalid tag commit SHA')
digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
if digest != expected:
    raise SystemExit(f'Source SHA256 does not match the published release: {digest}')
with tarfile.open(archive, 'r:xz') as tar:
    manifest = tar.extractfile(f'viper4linux-re-{version}/SOURCE-MANIFEST.txt')
    if manifest is None:
        raise SystemExit('Unified source is missing its source manifest')
    text = manifest.read().decode()
    if f'Application commit: {tag_sha}\n' not in text:
        raise SystemExit('Archive application commit differs from release tag')
    if not re.search(r'ViPERDSP commit: [0-9a-f]{40}', text):
        raise SystemExit('Pinned ViPERDSP revision not recorded')
print(f'PASS: published source SHA256 and v{version} commit {tag_sha} match')
