#!/usr/bin/env bash
# Comprehensive developer/CI regression suite, not the routine makepkg gate.
# All native mock DSP instances use isolated IPC and temporary preferences.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

bash tests/check-package.sh
bash tests/check-mock.sh
python3 tests/check-native-state-contract.py
python3 tests/check-precision-persistence-contract.py
python3 tests/check-gui-sync-contract.py
python3 tests/check-window-integration-contract.py
python3 tests/check-theme-contract.py
python3 tests/check-ui-contract.py
python3 tests/check-named-presets-contract.py
python3 tests/check-preset-reset-contract.py
python3 tests/check-save-preset-choice-contract.py
python3 tests/test_filter_idle_config.py
# check-mock.sh already runs tray, Dynamic EQ UI, About, and native dispatch
# contracts. Avoid invoking the same tests a second time.
if [[ -f .gitmodules ]]; then
    python3 tests/check-packaging-contract.py
fi
if [[ -f .github/workflows/prepare-stable-release.yml ]]; then
    python3 tests/check-native-deb-workflow.py
    python3 tests/check-assembled-release.py
    python3 tests/check-stable-release.py
fi

echo 'PASS: full developer regression suite'
