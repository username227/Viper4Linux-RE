#!/usr/bin/env python3
"""Guard default reset against accidental settings or preset data loss."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
presets = (root / "src/window/window-presets.inc").read_text()
dialog = (root / "src/viper-preset-dialog.h").read_text()
assert "actions.resetEffectsWithConfirmation();" in dialog
start = presets.index("    void resetEffectsWithConfirmation() {")
end = presets.index("    void showPresetManager()", start)
reset = presets[start:end]

# Cancel must be the default; effects must not change before confirmation.
assert "confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));" in reset
assert "if (confirm.clickedButton() != resetButton) return;" in reset
assert reset.index("if (confirm.clickedButton() != resetButton) return;") < reset.index("applySettings(")

# Reset DSP values, not the user's saved library or application preferences.
assert 'settings_.clear()' not in reset
assert 'settings_.remove("preset")' not in reset
assert 'applySettings(true, false, 0, 100, 65,' in reset
for required in ("resetOutputControls();", "resetNewBassControls();",
                 "resetFetSettings();", "resetMultibandSettings();", "resetDeq();"):
    assert required in reset, required
print("PASS: defaults reset requires confirmation and preserves presets")
