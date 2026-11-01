#!/usr/bin/env python3
"""Source-level regression for confirmed, non-destructive defaults reset."""
from pathlib import Path
s = (Path(__file__).resolve().parents[1] / 'src/viper-gui.cpp').read_text()
assert 'Reset to test defaults' not in s
assert 'Reset Effects to Defaults…' in s
start = s.index('    void resetEffectsWithConfirmation() {')
end = s.index('    void showPresetManager()', start)
reset = s[start:end]
assert 'confirm.setText("Reset all effect settings?")' in reset
assert 'Your named presets will not be deleted.' in reset
assert 'confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));' in reset
assert 'if (confirm.clickedButton() != resetButton) return;' in reset
assert reset.index('if (confirm.clickedButton() != resetButton) return;') < reset.index('applySettings(')
assert 'settings_.clear()' not in reset
assert 'settings_.remove("preset")' not in reset
assert 'resetOutputControls();' in reset
assert 'resetNewBassControls();' in reset
assert 'resetFetSettings();' in reset
assert 'resetMultibandSettings();' in reset
assert 'resetDeq();' in reset
assert 'statusMessage("Effect settings reset to defaults; named presets preserved.")' in reset
print('PASS: defaults reset requires confirmation; named preset files remain intact')
