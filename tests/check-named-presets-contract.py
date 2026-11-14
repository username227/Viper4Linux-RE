#!/usr/bin/env python3
"""Protect the GUI's non-destructive, single-publish named-preset restore."""
from pathlib import Path
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
gui = read_gui_sources(root)
presets = (root / "src/window/window-presets.inc").read_text()
storage = (root / "src/viper-named-presets.h").read_text()

# Runtime check-named-presets.cpp exercises the file schema and migration.
# GUI restore must remain guarded and publish the complete staged state once.
restore = presets[presets.index("    void restoreFromPresetStaging() {"):
                  presets.index("    void applyStagedPresetControls() {")]
controls = presets[presets.index("    void applyStagedPresetControls() {"):
                   presets.index("    void resetEffectsWithConfirmation() {")]
assert 'if (!settings_.value("preset/valid", false).toBool()) return;' in restore
assert "QScopedValueRollback<bool> deferPublishes" in restore
assert restore.index("deferPublishes") < restore.index("applyStagedPresetControls();")
assert restore.count("push();") == 1
assert restore.index("push();") > restore.index("applyStagedPresetControls();")
assert "push();" not in controls
assert "if (suppress_named_preset_push_) return;" in gui
assert "requestDdcReload();" in controls and "requestIrsLoad();" in controls

# Never wipe the named-preset library as a side effect of saving/restoring.
for forbidden in ('settings_.clear();', 'settings.clear();',
                  'settings_.remove("presets")', 'settings.remove("presets")'):
    assert forbidden not in presets + storage, forbidden
print("PASS: guarded named-preset restore, one publish, no library deletion")
