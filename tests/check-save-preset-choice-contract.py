#!/usr/bin/env python3
"""Contract checks for choosing an existing named preset when saving."""
from pathlib import Path

gui = (Path(__file__).resolve().parents[1] / "src/viper-preset-dialog.h").read_text()
start = gui.index('QObject::connect(save, &QPushButton::clicked')
end = gui.index('QObject::connect(assign, &QPushButton::clicked', start)
save = gui[start:end]
assert 'QStringList choices = {"New Preset…"};' in save
assert 'for (const auto& preset : existing) choices << preset.name;' in save
assert 'QInputDialog::getItem' in save
assert 'choices, 0, false, &accepted' in save  # default New Preset; no free-form list entry
assert 'if (choice == choices.first())' in save
assert 'QInputDialog::getText' in save[save.index('if (choice == choices.first())'):]
assert 'preset.name.compare(name, Qt::CaseInsensitive) == 0' in save
assert 'QMessageBox::question(&dialog, "Replace Preset"' in save
assert save.index('QMessageBox::question(&dialog, "Replace Preset"') < save.index('snapshotToPresetStaging();')
assert 'actions.writeNamedPreset(name, path, &error)' in save
print("PASS: Save Preset dropdown/new/overwrite contract")
