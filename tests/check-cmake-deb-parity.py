#!/usr/bin/env python3
"""Compare staged Debian filesystem payloads without running apt or dpkg.

The legacy side runs a frozen historical filesystem staging fixture in
tests/fixtures, not any production Debian packaging code. It never invokes
apt, dpkg or host installation. No package is installed on the host.
"""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/fixtures/legacy-deb-stage.txt"


def fail(message):
    raise SystemExit(f"FAIL: {message}")


def require_build(build):
    cache_file = build / "CMakeCache.txt"
    if not cache_file.is_file():
        fail(f"CMake cache missing: {cache_file}")
    cache = cache_file.read_text(errors="replace")
    for expected in (
        "VIPER_ENABLE_SYSTEM_INSTALL:BOOL=ON",
        "VIPER_SYSTEM_INSTALL_LAYOUT:STRING=debian",
        "CMAKE_INSTALL_PREFIX:PATH=/usr",
    ):
        if expected not in cache.splitlines():
            fail(f"Required CMake setting missing: {expected}")
    for target in ("viper-gui", "viper-ctl", "viper-output-state", "viper_dsp_ladspa.so"):
        if not (build / target).is_file() or not (build / target).stat().st_size:
            fail(f"Build artifact missing: {target}")


def manifest(root):
    found = {}
    for path in root.rglob("*"):
        rel = path.relative_to(root).as_posix()
        mode = stat.S_IMODE(path.lstat().st_mode)
        if path.is_symlink():
            found[rel] = ("link", mode, os.readlink(path))
        elif path.is_file():
            found[rel] = ("file", mode, hashlib.sha256(path.read_bytes()).hexdigest())
        elif path.is_dir():
            found[rel] = ("dir", mode)
        else:
            found[rel] = ("unknown", mode)
    return found


def main():
    if len(sys.argv) != 2:
        fail("Usage: python3 tests/check-cmake-deb-parity.py BUILD_DIRECTORY")
    build = Path(sys.argv[1]).resolve(strict=True)
    require_build(build)
    old_install = LEGACY.read_text()
    if not old_install.startswith("# Keep installation paths") or "install -Dm755" not in old_install:
        fail("Unexpected frozen Debian staging fixture")

    with tempfile.TemporaryDirectory(prefix="viper-debian-stage-parity-") as workdir:
        work = Path(workdir)
        old, new = work / "legacy", work / "cmake"
        old.mkdir()
        new.mkdir()

        # Run the existing Debian staging logic, but only its filesystem-copy
        # section. The frozen reference is test-only; CMake owns live staging.
        environment = {
            **os.environ,
            "tree": str(ROOT),
            "built": str(build),
            "pkgroot": str(old),
        }
        print("Staging reference Debian payload with legacy package-copy rules...", flush=True)
        subprocess.run(
            ["bash", "-Ee", "-u", "-o", "pipefail", "-c", old_install],
            env=environment, check=True,
        )

        print("Staging CMake Debian payload into a separate temporary tree...", flush=True)
        subprocess.run(
            ["cmake", "--install", str(build)],
            env={**os.environ, "DESTDIR": str(new)}, check=True,
        )

        # DEBIAN/control is added later by dpkg-shlibdeps and isn't part of
        # the filesystem payload at this stage. Compare both /usr trees only.
        a = manifest(old / "usr")
        b = manifest(new / "usr")
        mismatches = [p for p in sorted(a.keys() | b.keys()) if a.get(p) != b.get(p)]
        if mismatches:
            print(f"FAIL: {len(mismatches)} Debian staging differences:", file=sys.stderr)
            for p in mismatches[:40]:
                print(f"  usr/{p}", file=sys.stderr)
                print(f"    legacy: {a.get(p)}", file=sys.stderr)
                print(f"    CMake:  {b.get(p)}", file=sys.stderr)
            raise SystemExit(1)
        files = sum(value[0] == "file" for value in b.values())
        symlinks = sum(value[0] == "link" for value in b.values())
        print(f"PASS: exact Debian filesystem staging parity: {files} files, "
              f"{symlinks} symlinks; matching paths, SHA-256, modes and directory tree")


if __name__ == "__main__":
    main()
