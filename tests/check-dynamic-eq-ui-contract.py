#!/usr/bin/env python3
"""Guard Dynamic EQ edits and saved state without pinning drawing implementation."""
from pathlib import Path
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
plot = (root / "src/widgets/eq-overviews.h").read_text().split("class DynamicEqOverview final", 1)[1]

# The preview must let a user select bands and open the dedicated editor.
assert "selectBand(nearest)" in plot
assert "if (!editor_mode_ && openEditor) openEditor();" in plot
assert "deq_overview_->selectBand = [this](int index)" in source
assert "deq_band_select_->setCurrentIndex(index);" in source

# Changes in either editor must persist without recursive widget updates.
assert "const QSignalBlocker mainBlock(deq_count_), editorBlock(deq_editor_count_)" in source
assert "if (!deq_user_modified_[i])" in source
assert 'settings_.contains(k + "user_modified")' in source
assert 'settings_.setValue(k+"user_modified", deq_user_modified_[i])' in source
assert "void spreadDeqFrequencies()" in source
assert 'settings_.setValue("ui/dynamicEqEditorSize"' in source
assert "void restoreWindowSizes()" in source
print("PASS: Dynamic EQ selection, edits, and persistence")
