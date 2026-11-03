#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
[[ -x build/viper-gui ]] || { echo 'Build first with ./install.sh' >&2; exit 1; }
exec ./build/viper-gui
