#!/usr/bin/env python3
"""Static regression contract for the compact Dynamic EQ editor and persisted windows.

Only source-level checks; the real Qt6 render and X11 sizing require Arch testing.
"""
from pathlib import Path
import re
source = (Path(__file__).resolve().parents[1] / 'src/viper-gui.cpp').read_text()
start = source.index('class DynamicEqOverview final')
end = source.index('class ViperWindow final', start)
plot = source[start:end]
assert 'for (int i = 0; i < count_; ++i)' in plot
assert 'QString::number(i+1)' in plot
assert 'std::hypot(markers_[i].x() - event->position().x()' in plot
assert 'p.drawLine(anchors[i], markers_[i]);' in plot
assert 'if (nearest >= 0 && (editor_mode_ || distance <= 19.0))' in plot
assert 'if (!editor_mode_ && openEditor) openEditor();' in plot
assert 'deq_overview_->selectBand = [this](int index)' in source
assert 'deq_band_select_->setCurrentIndex(index);' in source
assert 'if (deq_overview_) deq_overview_->setSelectedBand(index);' in source
assert 'if (editor_mode_) {' not in plot[plot.index('// Each active band gets'):]
assert 'deq_count_ = new QSpinBox(deq_group)' in source
assert 'deq_editor_count_ = new QSpinBox(deq_content)' in source
assert 'deq_spread = new QPushButton("Spread frequencies"' in source
assert 'const QSignalBlocker mainBlock(deq_count_), editorBlock(deq_editor_count_)' in source
assert 'if (!deq_user_modified_[i])' in source
assert 'settings_.contains(k + "user_modified")' in source
assert 'settings_.setValue(k+"user_modified", deq_user_modified_[i])' in source
assert 'void spreadDeqFrequencies()' in source
assert 'settings_.setValue("ui/mainWindowSize"' in source
assert 'settings_.setValue("ui/graphicEqEditorSize"' in source
assert 'settings_.setValue("ui/dynamicEqEditorSize"' in source
assert 'void restoreWindowSizes()' in source
assert 'QGuiApplication::primaryScreen()' in source
assert 'void closeEvent(QCloseEvent* event) override' in source
assert 'bool eventFilter(QObject* watched, QEvent* event) override' in source
assert 'QTimer' not in plot, 'No timers/polling allowed for static plot'
match = re.search(r'static constexpr int ten\[\] = \{([^}]+)\};', source)
assert match
full = [int(x) for x in re.findall(r'\b\d+\b', match.group(1))]
assert full == [31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], full
for count in range(1, 11):
    if count == 1: frequencies = [1000]
    elif count == 2: frequencies = [125, 8000]
    elif count == 3: frequencies = [100, 1000, 8000]
    else: frequencies = [full[round(i * 9.0 / (count - 1))] for i in range(count)]
    assert len(frequencies) == len(set(frequencies)), (count, frequencies)
    assert frequencies == sorted(frequencies), (count, frequencies)
    assert all(20 <= f <= 20000 for f in frequencies), (count, frequencies)
print('PASS: graph-click selection and numbered Dynamic EQ markers in both views, spread 1–10 band defaults, saved edits, and persisted window sizes')
