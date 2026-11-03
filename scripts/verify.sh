#!/usr/bin/env bash
# Verify an isolated fresh build and report live DSP data (no system changes).
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
printf '== Complete source and three renamed outputs ==\n'
for f in CMakeLists.txt src/viper-ladspa.cpp src/viper-gui.cpp src/viper-ctl.cpp src/viper-control.h src/viperdsp_compat.h \
         build/viper_dsp_ladspa.so build/viper-gui build/viper-ctl; do
  [[ -s "$f" ]] || { echo "MISSING: $f" >&2; exit 1; }
  printf 'OK: %s\n' "$f"
done
if grep -q 'plugin = viper_dsp_ladspa' routing-config/filter-chain.conf.d/99-viper-smart.conf; then
  echo 'OK: renamed PipeWire plugin reference'
else
  echo 'ERROR: wrong PipeWire plugin filename' >&2; exit 1
fi
if [[ -r build/CMakeCache.txt ]]; then
  grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' build/CMakeCache.txt || :
fi
if [[ -e build/viperbass_ladspa.so || -e build/viperbass-ctl || -e build/viperbass-gui ]]; then
  echo 'ERROR: legacy build artifact found!' >&2; exit 1
fi
printf '\n== Mock regression suite ==\n'
./tests/check-mock.sh
printf '\n== DSP backend and callback counter (play Rhythmbox first) ==\n'
./scripts/status.sh
sleep 3
./scripts/status.sh
printf '\n== PipeWire links for ViPER and Rhythmbox ==\n'
if command -v pw-link >/dev/null; then
  pw-link -l | grep -E -A 3 'Rhythmbox:output_|viperbass.smart.capture:input_|viperbass.smart.playback:output_' || :
fi
cat <<'EOF'

Manual acceptance checks (system installation or source checkout):
  [ ] Six tabs: Output, Bass, EQ & Tone, Spatial, Dynamics, Sound Correction
  [ ] Every effect switch is independent of its panel's expanded state
  [ ] Master bypass and Native Master Limiter work independently as documented
  [ ] FET and Multiband compressors are disabled by default; compact band editor works
  [ ] Graphic EQ and Dynamic EQ editors open; changes restore correctly
  [ ] IRS/VDC library selectors import and load; invalid profiles fail safely
  [ ] Named presets Save/Load/Rename/Delete and Reset-to-Defaults confirmation work
  [ ] Theme selector, dropdown arrows, menu and tray behavior render correctly
  [ ] Minimize to tray leaves no black taskbar-listed window on Cinnamon X11
  [ ] GUI Quit leaves backend playback active; restart GUI reconnects
  [ ] Auto-start/login and absent-tray fallback behave correctly
  [ ] CLI 'status' shows advancing audio blocks while music plays
  [ ] Bluetooth reconnection and stream changes maintain PipeWire routing
  [ ] JamesDSP routing interactions, if present, tested independently
EOF
