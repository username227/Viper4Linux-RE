#!/usr/bin/env python3
"""Source-level integration contract for exact CLI values across GUI and DSP."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cli = (root / "src/viper-cli-settings.h").read_text()
backend_cpp = (root / "src/viper-ladspa.cpp").read_text()
assert '#include "backend/startup-settings.inc"' in backend_cpp
backend = (root / "src/backend/startup-settings.inc").read_text()
gui = "\n".join([
    (root / "src/viper-gui.cpp").read_text(),
    (root / "src/sync/gui-settings-bridge.inc").read_text(),
    (root / "src/sync/gui-backend-bridge.inc").read_text(),
    (root / "src/sync/gui-control-channel.h").read_text(),
    (root / "src/window/window-effect-actions.inc").read_text(),
])
output = (root / "src/viper-output-state.cpp").read_text()
precision = (root / "src/viper-precision-units.h").read_text()
mock = (root / "tests/check-mock.sh").read_text()
native = (root / "tests/mock/check.cpp").read_text()

fields = (
    "strength_centi",
    "bass_mono_strength_centi",
    "clarity_gain_milli",
    "spectrum_frequency_hz",
    "spectrum_exciter_milli",
    "diff_lp_hz",
    "field_depth",
)
field_names = (
    "Strength", "MonoStrength", "Clarity", "SpectrumFrequency",
    "SpectrumExciter", "DiffLowpass", "FieldDepth",
)
for key, field in zip(fields, field_names):
    exact = f"precision/{key}"
    assert f'Field::{field}, "{key}"' in precision, f"Native/legacy map missing {key}"
    assert f"viper_precision::Field::{field}, c.{key}" in cli, f"CLI doesn't persist {key}"
    assert f'key == "{key}"' in backend, f"backend doesn't restore {key}"
    assert exact in gui, f"GUI doesn't preserve {key}"
assert 'QStringLiteral("precision/")' in cli
assert 'if (precisionBinding(key))' in output
assert 'viper_precision::fromLegacy(spec->field, value)' in output

backend_start = backend.index("void load_saved_settings(")
legacy = backend.index('key == "spectrum_frequency_hundreds"', backend_start)
precise = backend.index('precise = line == "[precision]"', backend_start)
assert precise > legacy, "Native precision must override legacy slider values"
assert 'file.seekg(0);' in backend[legacy:precise]
assert "editedControl == slider" in gui, "Only edited slider should quantize"
assert 'nativePrecision(' in gui, "Precise slider policy must be delegated"
assert 'viper_precision::fromSlider(' in gui, "GUI edits must share native conversion"
assert 'viper_precision::toSlider(' in gui, "GUI readback must share native conversion"
# Visible widgets can remain unchanged while CLI updates exact Dynamic System
# low-X below the GUI slider's 120 Hz minimum. That native value must persist,
# but idle polling still must not continuously restart the disk-save timer.
assert 'const int beforeNativeLowX = dyn_xlo_native_;' in gui, "Capture native Dynamic System low-X before readback"
assert 'dyn_xlo_native_ = std::clamp(c->dynamic_x_low.load(), 0, 2400);' in gui, "Read back exact low-X from native IPC"
assert 'if (before != liveUiSnapshot() || beforeNativeLowX != dyn_xlo_native_) persistCurrentSettings();' in gui, "Readback must persist changed UI or native low-X only"
assert 'if (pending_edit_) {' in gui, "Offline GUI edit must win on reconnect"
assert "replace_precision_on_push_" in gui
assert 'settings_.remove("precision");' in gui
assert 'XDG_CONFIG_HOME="$config/precision"' in mock
assert "assert(p->spectrum_frequency_hz.load() == 8024)" in native
assert "assert(p->field_depth.load() == 537)" in native
print("PASS: CLI precision, backend startup, GUI, Audio Outputs and mock startup linked")
