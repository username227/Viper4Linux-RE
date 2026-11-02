#!/usr/bin/env python3
"""Render a stable, release-asset PKGBUILD with the archive's real SHA256."""
import argparse
import hashlib
from pathlib import Path
import re
import sys


def render(template: str, version: str, digest: str) -> str:
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        raise ValueError('Expected release version MAJOR.MINOR.PATCH')
    if not re.fullmatch(r'[a-f0-9]{64}', digest):
        raise ValueError('Invalid SHA256 digest')
    output, count = re.subn(r'(?m)^pkgver=[^\n]+$', 'pkgver=' + version, template)
    if count != 1:
        raise ValueError(f'Expected exactly one pkgver assignment, found {count}')
    needle = "sha256sums=('REPLACE_WITH_RELEASE_SHA256')"
    if output.count(needle) != 1:
        raise ValueError('Missing or duplicate SHA256 placeholder')
    output = output.replace(needle, f"sha256sums=('{digest}')")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('archive', type=Path)
    parser.add_argument('template', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    expected = f'viper4linux-re-{args.version}-unified-source.tar.xz'
    if args.archive.name != expected:
        parser.error(f'Expected archive named {expected}, got {args.archive.name}')
    digest = hashlib.file_digest(args.archive.open('rb'), 'sha256').hexdigest()
    try:
        content = render(args.template.read_text(), args.version, digest)
    except ValueError as exc:
        parser.error(str(exc))
    args.output.write_text(content)
    print(f'{digest}  {expected}')


if __name__ == '__main__':
    main()
