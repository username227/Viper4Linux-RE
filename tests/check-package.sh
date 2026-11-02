#!/usr/bin/env bash
# Fast, deterministic, non-audio checks for routine makepkg builds.
# No mock C++ compilation and no PipeWire or physical audio access.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

python3 tests/check-launcher.py
python3 tests/check-arch-install.py
python3 -m unittest discover -s tests -p 'test_output_*.py' -v

echo 'PASS: package smoke checks (launcher, staging, per-output logic)'
