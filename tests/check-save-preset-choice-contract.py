#!/usr/bin/env python3
"""Guard named-preset selection and overwrite confirmation, not dialog text."""
from pathlib import Path

dialog = (Path(__file__).resolve().parents[1] / "src/viper-preset-dialog.h").read_text()
save = dialog[dialog.index("QObject::connect(save, &QPushButton::clicked"):
              dialog.index("QObject::connect(assign, &QPushButton::clicked")]
# A new preset is the default; choosing an existing one requires confirmation
# before an in-memory snapshot is taken or the saved preset overwritten.
assert "choices, 0, false, &accepted" in save
assert "for (const auto& preset : existing) choices << preset.name;" in save
assert "if (choice == choices.first())" in save
assert "QMessageBox::question" in save
assert save.index("QMessageBox::question") < save.index("snapshotToPresetStaging();")
assert "actions.writeNamedPreset(name, path, &error)" in save
print("PASS: new preset default and overwrite confirmation")
