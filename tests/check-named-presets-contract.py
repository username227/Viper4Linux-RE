#!/usr/bin/env python3
"""Verify named-presets UI/storage/migration guards without a Qt runtime."""
from pathlib import Path
from gui_source_contract import read_gui_sources
root = Path(__file__).resolve().parents[1]
s = read_gui_sources(root)
presets = "\n".join((
    (root / "src/window/window-presets.inc").read_text(),
    (root / "src/viper-named-presets.h").read_text(),
    (root / "src/viper-preset-dialog.h").read_text(),
))
assert 'QStandardPaths::GenericDataLocation' in presets
assert '"/viper4linux/presets"' in presets
assert 'QSettings::IniFormat' in presets
assert 'QUuid::createUuid()' in presets
assert 'preset.clear();' in presets
assert 'preset.setValue("effects/" + key.mid(7), settings.value(key))' in presets
assert 'migrateLegacyPreset(&error)' in presets
assert 'presets/legacy_migrated' in presets
assert 'if (settings.value("preset/valid", false).toBool()) {' in presets
assert 'settings.setValue("presets/legacy_migrated", true);' in presets
assert '"Previously Saved Preset"' in presets
assert 'if (!writeNamedPreset(settings, name, path, error)) return false;' in presets
assert 'preset.contains("effects/master_enabled")' in presets
assert 'preset.contains("effects/limiter_output_percent")' in presets
assert 'settings.remove("preset");' in presets
assert '"Save Preset…"' in presets
assert 'QInputDialog::getItem(&dialog, "Save Effect Preset"' in presets
assert '"New Preset…"' in presets
assert 'for (const auto& preset : existing) choices << preset.name;' in presets
assert 'QInputDialog::getText(&dialog, "New Effect Preset"' in presets
assert '"Preset name:"' in presets
assert 'name.compare(name, Qt::CaseInsensitive)' in presets
assert '"Replace Preset"' in presets
assert 'QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel' in presets
assert '"Load Effect Preset"' in presets
assert '"This replaces the current effect settings. Unsaved changes will be lost.' in presets
assert '"Rename Preset"' in presets
assert '"Delete Preset"' in presets
assert '"Reset Effects to Defaults…"' in presets
assert '"Your named presets will not be deleted."' in presets
assert 'confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));' in presets
# Confirm before replacing current controls with the current defaults:
# master on, optional Bass off, and all other optional effects off.
assert presets.index('if (confirm.clickedButton() != resetButton) return;') < presets.index('applySettings(true, false, 0, 100, 65')
assert 'resetOutputControls();' in presets
assert 'resetNewBassControls();' in presets
assert 'settings_.clear();' not in presets
assert 'settings.clear();' not in presets
assert 'settings_.remove("presets")' not in presets
assert 'settings.remove("presets")' not in presets
assert '"Presets…"' in s
assert 'showPresetManager();' in s
# New preset library only changes GUI and personal settings; no DSP source involved.
print('PASS: named presets, collision confirmation, guarded loads, legacy migration, reset safety')
