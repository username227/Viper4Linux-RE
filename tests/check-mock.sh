#!/usr/bin/env bash
# Compile and test against a deliberately simplified DSP mock. Does not
# claim parity with the real upstream algorithms or test Bluetooth timing.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
work=$(mktemp -d)
config=$(mktemp -d)
trap 'rm -rf -- "$work" "$config"' EXIT
g++ -pthread -std=c++17 -DVIPER_MOCK_TEST_IPC -O2 -fPIC -shared -Wall -Wextra -Itests/mock -Isrc \
  src/viper-ladspa.cpp tests/mock/mock_viper.cpp -o "$work/viper_mock.so"
g++ -pthread -std=c++17 -DVIPER_MOCK_TEST_IPC -O2 -Wall -Wextra -Itests/mock -Isrc \
  tests/mock/check.cpp -ldl -o "$work/check"
mkdir -p "$config/ViPERforLinux"
python3 - "$config/impulse.irs" <<'PY_WAV'
import sys, wave, struct
with wave.open(sys.argv[1], 'wb') as f:
    f.setnchannels(2); f.setsampwidth(2); f.setframerate(48000)
    f.writeframes(struct.pack('<'+'h'*64, *([8192,0]+[0]*62)))
PY_WAV
python3 - "$config/impulse-alt.irs" <<'PY_ALT'
import sys, wave, struct
with wave.open(sys.argv[1], 'wb') as f:
    f.setnchannels(2); f.setsampwidth(2); f.setframerate(44100)
    f.writeframes(struct.pack('<'+'h'*64, *([8192,0]+[0]*62)))
PY_ALT
cat > "$config/ViPERforLinux/ViPERClean.conf" <<'INI'
[General]
enabled=true
master_enabled=false
strength_tenths=50
frequency=90
bass_mode=2
input_percent=30 # Legacy input attenuation is deliberately ignored; verify unity input.
bass_mono_enabled=false
bass_mono_mode=1
bass_mono_strength_centi=200
bass_mono_frequency_hz=75
psycho_enabled=false
psycho_cutoff_hz=90
psycho_intensity_percent=30
psycho_order=4
psycho_original_percent=100
dynamic_enabled=false
dynamic_x_low=100
dynamic_x_high=6000
dynamic_y_low=100
dynamic_y_high=150
dynamic_side_low_percent=50
dynamic_side_high_percent=50
dynamic_strength_percent=0
clarity_enabled=true
clarity_mode=2
clarity_gain_tenths=25
spectrum_enabled=true
spectrum_frequency_hundreds=68
spectrum_exciter_tenths=20
headphone_enabled=true
headphone_quality=3
diff_enabled=true
diff_delay_ms=8
diff_reverse=true
diff_mix_percent=60
diff_lp_hundreds=40
field_enabled=true
field_widen_tenths=20
field_mid_hundredths=125
field_depth_step=5
reverb_enabled=true
reverb_room_percent=60
reverb_width_percent=70
reverb_damp_percent=40
reverb_wet_percent=20
reverb_dry_percent=90
limiter_threshold_permille=500
limiter_output_percent=100
limiter_pan_percent=0
agc_enabled=false
agc_strength_centi=100
agc_max_gain_centi=300
agc_threshold_permille=900
lufs_enabled=false
lufs_target_tenths=-140
lufs_max_gain_tenths=60
lufs_speed=1
convolver_enabled=false
convolver_cross_percent=0
eq_precision_hundredths=true
eq_enabled=false
eq_band_count=15
analogx_enabled=false
analogx_mode=0
eq_levels_15="500,0,0,0,0,0,0,0,0,0,0,0,0,0,0"
INI
printf 'convolver_kernel=%s/impulse.irs\n' "$config" >> "$config/ViPERforLinux/ViPERClean.conf"
python3 tests/check-dynamic-eq-ui-contract.py
python3 tests/check-about-and-deq-handoff.py
python3 tests/check-tray-contract.py
python3 tests/check-native-effect-routing.py
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc tests/check-eq-presets.cpp -o "$work/check-eq-presets"
"$work/check-eq-presets"
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc tests/check-ir-reader.cpp -o "$work/check-ir-reader"
"$work/check-ir-reader" "$work"
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc tests/check-ddc-reader.cpp -o "$work/check-ddc-reader"
"$work/check-ddc-reader"
XDG_CONFIG_HOME="$config" "$work/check" "$work/viper_mock.so"

# The CLI test uses a PID-scoped mock IPC channel and never connects to live audio.
g++ -pthread -std=c++17 -DVIPER_MOCK_TEST_IPC -O2 -fPIC -Wall -Wextra -Werror -Isrc \
  $(pkg-config --cflags Qt6Core) tests/check-cli-controls.cpp \
  -o "$work/check-cli-controls" $(pkg-config --libs Qt6Core)
XDG_CONFIG_HOME="$config/cli-basic" "$work/check-cli-controls"

# The command tokenizer and whole-sequence validator are independent of Qt.
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc \
  tests/check-cli-batch.cpp -o "$work/check-cli-batch"
"$work/check-cli-batch"

# CLI persistence is tested in its own temporary config home.
g++ -pthread -std=c++17 -DVIPER_MOCK_TEST_IPC -O2 -fPIC -Wall -Wextra -Werror -Isrc \
  $(pkg-config --cflags Qt6Core) tests/check-cli-settings.cpp \
  -o "$work/check-cli-settings" $(pkg-config --libs Qt6Core)
XDG_CONFIG_HOME="$config/cli-only" "$work/check-cli-settings"

# Preset selection and complete status use another isolated mock channel/store.
g++ -pthread -std=c++17 -DVIPER_MOCK_TEST_IPC -O2 -fPIC -Wall -Wextra -Werror -Isrc \
  $(pkg-config --cflags Qt6Core) tests/check-cli-eq-presets.cpp \
  -o "$work/check-cli-eq-presets" $(pkg-config --libs Qt6Core)
XDG_CONFIG_HOME="$config/cli-presets" "$work/check-cli-eq-presets"
