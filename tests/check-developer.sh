#!/usr/bin/env bash
# Comprehensive developer/CI regression suite, not the routine makepkg gate.
# All native mock DSP instances use isolated IPC and temporary preferences.
# Optional group: package | mock | contracts | release; no argument = full suite.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

run_package() {
    bash tests/check-package.sh
}

run_mock() {
    bash tests/check-mock.sh
}

run_contracts() {
    python3 tests/check-native-state-contract.py
    python3 tests/check-precision-persistence-contract.py
    python3 tests/check-gui-sync-contract.py
    python3 tests/check-window-integration-contract.py
    python3 tests/check-version.py
    python3 tests/check-single-gui.py
    python3 tests/check-gui-backend-exit.py
    python3 tests/check-theme-contract.py
    python3 tests/check-ui-contract.py
    python3 tests/check-named-presets-contract.py
    python3 tests/check-preset-reset-contract.py
    python3 tests/check-save-preset-choice-contract.py
    python3 tests/test_filter_idle_config.py
    # check-mock.sh already covers tray, Dynamic EQ UI, About and native
    # effect dispatch. check-package.sh covers source and package contracts.
}

run_release() {
    # Unified source archives do not necessarily contain the Actions workflow.
    if [[ -f .github/workflows/prepare-stable-release.yml ]]; then
        python3 tests/check-native-deb-workflow.py
        python3 tests/check-assembled-release.py
        python3 tests/check-stable-release.py
    fi
}

if (( $# > 1 )); then
    echo 'Usage: tests/check-developer.sh [package|mock|contracts|release]' >&2
    exit 2
fi

case ${1:-all} in
    all)
        run_package
        run_mock
        run_contracts
        run_release
        echo 'PASS: full developer regression suite'
        ;;
    package)
        run_package
        echo 'PASS: developer package checks'
        ;;
    mock)
        run_mock
        echo 'PASS: developer native mock DSP checks'
        ;;
    contracts)
        run_contracts
        echo 'PASS: developer application contracts'
        ;;
    release)
        run_release
        echo 'PASS: developer release contracts (when present)'
        ;;
    *)
        echo 'Usage: tests/check-developer.sh [package|mock|contracts|release]' >&2
        exit 2
        ;;
esac
