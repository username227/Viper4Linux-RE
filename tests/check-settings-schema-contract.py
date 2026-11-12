#!/usr/bin/env python3
"""Golden read/write GUI settings-key inventory before Stage 3 refactoring.

These are existing shipped INI keys, not a proposal for a new schema.
No real user files are read or written.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
saved = (root / "src/sync/gui-settings-bridge.inc").read_text()
loaded = (root / "src/window/window-initialization.inc").read_text()
deq = (root / "src/window/window-dynamics.inc").read_text()
preset = (root / "src/window/window-presets.inc").read_text()
shared = (root / "src/sync/gui-simple-settings.inc").read_text()
shared_pairs = dict(re.findall(r'\{Phase::\w+,\s*"([^"]+)",\s*[^,]+,\s*Kind::\w+,\s*(-?\d+)', shared))
shared_keys = set(shared_pairs)
assert len(shared_keys) == 65, "Stage 3C bindings must remain unique and complete"

# The literal effect keys written by persistCurrentSettings on the Stage 2
# baseline. Dynamic DEQ and multiband keys are verified separately below.
canonical_keys = set("""
master_enabled enabled bass_mode strength_tenths frequency
eq_enabled eq_band_count eq/selected_preset
ddc_enabled speaker_enabled convolver_enabled convolver_cross_percent
bass_mono_enabled bass_mono_mode bass_mono_strength_centi bass_mono_frequency_hz
psycho_enabled psycho_cutoff_hz psycho_intensity_percent psycho_order psycho_original_percent
dynamic_enabled dynamic_x_low dynamic_x_high dynamic_y_low dynamic_y_high
dynamic_side_low_percent dynamic_side_high_percent dynamic_strength_percent
clarity_enabled clarity_mode clarity_gain_tenths
spectrum_enabled spectrum_frequency_hundreds spectrum_exciter_tenths
tube_enabled tube_model tube_drive_percent tube_mix_percent
analogx_enabled analogx_mode crossfeed_enabled crossfeed_preset
fet_enabled fet_threshold_db fet_ratio_tenths fet_knee_db fet_makeup_db fet_attack_ms fet_release_ms
imager_enabled imager_low_width_percent imager_mid_width_percent imager_high_width_percent
imager_low_crossover_hz imager_high_crossover_hz
deq_enabled deq_band_count headphone_enabled headphone_quality
diff_enabled diff_delay_ms diff_reverse diff_mix_percent diff_lp_hundreds
field_enabled field_widen_tenths field_mid_hundredths field_depth_step
reverb_enabled reverb_room_percent reverb_width_percent reverb_damp_percent reverb_wet_percent reverb_dry_percent
limiter_threshold_permille limiter_output_percent limiter_pan_percent
agc_enabled agc_strength_centi agc_max_gain_centi agc_threshold_permille
lufs_enabled lufs_target_tenths lufs_max_gain_tenths lufs_speed
""".split())
assert len(canonical_keys) == 86, f"Unexpected golden key count: {len(canonical_keys)}"

def literal_keys(source: str, method: str) -> set[str]:
    return set(re.findall(r'settings_\.' + method + r'\(\s*"([^"]+)"', source))

actual_saved = literal_keys(saved, "setValue") | shared_keys
assert actual_saved == canonical_keys, (
    f"Persisted keys changed: missing={sorted(canonical_keys - actual_saved)}; "
    f"extra={sorted(actual_saved - canonical_keys)}"
)
# DEQ is loaded by loadDeqSettings; DDC path is loaded by this method.
# Theme is read in the GUI constructor and updated in the initialization
# signal setup, not read in loadSavedEffectSettings().
actual_loaded = literal_keys(loaded, "value") | shared_keys
expected_loaded = (canonical_keys - {"deq_enabled", "deq_band_count"}) | {
    "ddc_profile"
}
assert actual_loaded == expected_loaded, (
    f"Loaded keys changed: missing={sorted(expected_loaded - actual_loaded)}; "
    f"extra={sorted(actual_loaded - expected_loaded)}"
)
gui = (root / "src/viper-gui.cpp").read_text()
assert 'settings_.value("ui/theme", "modern").toString()' in gui
assert 'settings_.setValue("ui/theme", index == 1 ? "classic-v4a" : "modern")' in loaded

for key, default, conversion in (
    ("master_enabled", "true", "toBool"),
    ("enabled", "true", "toBool"),
    ("eq_enabled", "false", "toBool"),
    ("eq_band_count", "10", "toInt"),
    ("spectrum_frequency_hundreds", "76", "toInt"),
    ("clarity_gain_tenths", "15", "toInt"),
    ("field_depth_step", "3", "toInt"),
    ("limiter_output_percent", "100", "toInt"),
    ("lufs_target_tenths", "-140", "toInt"),
    ("headphone_quality", "0", "toInt"),
):
    if key in shared_pairs:
        assert shared_pairs[key] == ("1" if default == "true" else "0" if default == "false" else default), key
    else:
        assert f'settings_.value("{key}", {default}).{conversion}()' in loaded, key

# Controls with special native/GUI units must preserve the existing mapping.
assert 'dyn_xlo_native_ = std::clamp(settings_.value("dynamic_x_low", 100).toInt(), 0, 2400);' in loaded
assert 'dyn_xlo_.slider->setValue(std::max(120, dyn_xlo_native_));' in loaded
assert 'settings_.value("imager_high_crossover_hz", 4000).toInt()/100' in loaded
assert 'saveEqLayout();' in saved and 'loadEqLayout();' in loaded
assert 'loadMultibandSettings("");' in loaded and 'saveMultibandSettings("");' in saved
assert 'loadDeqSettings();' in loaded
assert 'settings_.value("deq_enabled", false).toBool()' in deq
assert 'settings_.value("deq_band_count", 3).toInt()' in deq
for field in ("freq", "q", "gain", "threshold", "attack", "release", "type"):
    assert f'settings_.setValue(k+"{field}"' in saved, field
    assert f'settings_.value(k+"{field}"' in deq, field
assert 'settings_.setValue(k+"user_modified"' in saved
assert 'deq_user_modified_[i] = settings_.contains(k + "user_modified")' in deq
assert 'settings_.setValue("preset/eq_selected_preset", eq_selected_preset_)' in preset
assert 'settings_.setValue("preset/imager_high_crossover_hundreds", imager_high_xover_.slider->value())' in preset
print("PASS: GUI INI key inventory, default types and special native mappings")
