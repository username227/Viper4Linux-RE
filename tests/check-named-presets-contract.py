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
# Reset requires positive confirmation and cannot erase saved presets.
dialog = (root / "src/viper-preset-dialog.h").read_text()
assert "actions.resetEffectsWithConfirmation();" in dialog
start = presets.index("    void resetEffectsWithConfirmation() {")
end = presets.index("    void showPresetManager()", start)
reset = presets[start:end]
assert "confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));" in reset
assert "if (confirm.clickedButton() != resetButton) return;" in reset
assert reset.index("if (confirm.clickedButton() != resetButton) return;") < reset.index("applySettings(")
assert "settings_.clear()" not in reset
assert 'settings_.remove("preset")' not in reset
assert "applySettings(true, false, 0, 100, 65," in reset
for required in ("resetOutputControls();", "resetNewBassControls();",
                 "resetFetSettings();", "resetMultibandSettings();", "resetDeq();"):
    assert required in reset, required

# Choosing an existing preset must confirm overwrite before saving.
save = dialog[dialog.index("QObject::connect(save, &QPushButton::clicked"):
              dialog.index("QObject::connect(assign, &QPushButton::clicked")]
assert "choices, 0, false, &accepted" in save
assert "for (const auto& preset : existing) choices << preset.name;" in save
assert "if (choice == choices.first())" in save
assert "QMessageBox::question" in save
assert save.index("QMessageBox::question") < save.index("snapshotToPresetStaging();")
assert "actions.writeNamedPreset(name, path, &error)" in save

print("PASS: guarded preset restore, reset, overwrite, and preserved library")
