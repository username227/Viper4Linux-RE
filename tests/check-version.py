#!/usr/bin/env python3
"""Offline test: Git-tagged development versions and versioned source archives."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1] / 'cmake' / 'Version.cmake'
assert source.is_file()


def run(*args, cwd=None, success=True):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if success:
        assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result


def version_from(directory: Path):
    (directory / 'check.cmake').write_text(
        'include("${CMAKE_CURRENT_LIST_DIR}/cmake/Version.cmake")\n'
        'message("V4L_RESULT:${V4L_BASE_VERSION}|${V4L_REVISION}|${V4L_DEV_VERSION}")\n'
    )
    result = run('cmake', '-P', 'check.cmake', cwd=directory)
    match = re.search(r'V4L_RESULT:([^|\n]+)\|([^|\n]+)\|([^\n]+)', result.stderr)
    assert match, (result.stdout, result.stderr)
    return match.groups()


with tempfile.TemporaryDirectory(prefix='v4l-version-') as tmp:
    base = Path(tmp)
    checkout = base / 'checkout'
    (checkout / 'cmake').mkdir(parents=True)
    shutil.copy2(source, checkout / 'cmake/Version.cmake')
    run('git', 'init', '-q', str(checkout))
    (checkout / 'tracked.txt').write_text('first\n')
    run('git', 'add', 'tracked.txt', cwd=checkout)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'first', cwd=checkout)
    version, sha, label = version_from(checkout)
    assert version == '0.0.0' and label == f'{version}-{sha}', label
    run('git', 'tag', 'v2.3.4', cwd=checkout)
    (checkout / 'tracked.txt').write_text('second\n')
    run('git', 'add', 'tracked.txt', cwd=checkout)
    run('git', '-c', 'user.name=Test', '-c', 'user.email=test@example.com',
        'commit', '-qm', 'second', cwd=checkout)
    expected_sha = run('git', 'rev-parse', '--short=12', 'HEAD', cwd=checkout).stdout.strip()
    assert version_from(checkout) == ('2.3.4', expected_sha, f'2.3.4-{expected_sha}')
    run('git', 'tag', 'v2.3.5-rc1', cwd=checkout)
    assert version_from(checkout) == ('2.3.5-rc1', expected_sha, f'2.3.5-rc1-{expected_sha}')

    # Archive copies have no Git directory. The workflow-created file is
    # authoritative for versioning, and RELEASE-REVISION retains provenance.
    archive = base / 'archive'
    (archive / 'cmake').mkdir(parents=True)
    shutil.copy2(source, archive / 'cmake/Version.cmake')
    (archive / 'RELEASE-VERSION').write_text('9.8.7\n')
    (archive / 'RELEASE-REVISION').write_text('abcdef123456\n')
    assert version_from(archive) == ('9.8.7', 'abcdef123456', '9.8.7-abcdef123456')
    (archive / 'RELEASE-VERSION').write_text('9.8.7-rc1\n')
    assert version_from(archive) == ('9.8.7-rc1', 'abcdef123456', '9.8.7-rc1-abcdef123456')
    (archive / 'RELEASE-VERSION').write_text('not-a-version\n')
    failure = run('cmake', '-P', 'check.cmake', cwd=archive, success=False)
    assert failure.returncode != 0 and 'Invalid RELEASE-VERSION' in failure.stderr

print('PASS: tag-derived development identity, archive release metadata and invalid-version rejection')
