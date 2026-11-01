#!/usr/bin/env bash
# Verify an isolated fresh build and report live DSP data (no system changes).
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
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
./status.sh
sleep 3
./status.sh
printf '\n== PipeWire links for ViPER and Rhythmbox ==\n'
if command -v pw-link >/dev/null; then
  pw-link -l | grep -E -A 3 'Rhythmbox:output_|viperbass.smart.capture:input_|viperbass.smart.playback:output_' || :
fi
cat <<'EOF'

Manual tests:
  [ ] GUI launches and shows Enhancements, Surround, and Output tabs
  [ ] Four Surround effects and three Enhancements have working controls
  [ ] Collapsing a panel does not disable the effect; state persists on GUI reopen
  [ ] Master Limiter shows greyed ON switch but its card expands/collapses
  [ ] With AGC/LUFS OFF, test threshold at quiet listening volume; limiter stays active
  [ ] AGC alone changes output level and preserves other effect settings
  [ ] LUFS alone follows target (check pw-top for CPU/xruns); keep off when not testing
  [ ] Enabling both AGC and LUFS shows warning; disable both afterward
  [ ] Master on/off bypasses effects without disabling the output limiter
  [ ] Master on/off audibly changes sound; input is not adjusted unintentionally
  [ ] Save and Restore preset, then reopen GUI
  [ ] With GUI OPEN: ./stop.sh ; ./start.sh in backend terminal ; GUI reconnects
  [ ] During playback: audio blocks count increments; Rhythmbox -> ViPER -> EDIFIER
  [ ] JamesDSP fully quit, not just bypassed
  [ ] Previously working source directory untouched until new build validated
EOF
