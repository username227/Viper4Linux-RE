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
for key in fields:
    exact = f"precision/{key}"
    assert exact in cli, f"CLI doesn't persist {key}"
    assert f'key == "{key}"' in backend, f"backend doesn't restore {key}"
    assert exact in gui, f"GUI doesn't preserve {key}"
    assert f'key == "{key}"' in output or f'|| key == "{key}"' in output, (
        f"Audio Output restoration doesn't persist {key}"
    )

backend_start = backend.index("void load_saved_settings(")
legacy = backend.index('key == "spectrum_frequency_hundreds"', backend_start)
precise = backend.index('precise = line == "[precision]"', backend_start)
assert precise > legacy, "Native precision must override legacy slider values"
assert 'file.seekg(0);' in backend[legacy:precise]
assert "editedControl == slider" in gui, "Only edited slider should quantize"
assert 'nativePrecision(' in gui, "Precise slider policy must be delegated"
assert 'if (before != liveUiSnapshot()) persistCurrentSettings();' in gui, "Readback must avoid spurious persistence"
assert 'if (pending_edit_) {' in gui, "Offline GUI edit must win on reconnect"
assert "replace_precision_on_push_" in gui
assert 'settings_.remove("precision");' in gui
assert 'XDG_CONFIG_HOME="$config/precision"' in mock
assert "assert(p->spectrum_frequency_hz.load() == 8024)" in native
assert "assert(p->field_depth.load() == 537)" in native
print("PASS: CLI precision, backend startup, GUI, Audio Outputs and mock startup linked")
