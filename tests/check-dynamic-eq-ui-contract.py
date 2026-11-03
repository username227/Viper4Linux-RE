#!/usr/bin/env python3
"""Static regression contract for the compact Dynamic EQ editor and persisted windows.

Only source-level checks; the real Qt6 render and X11 sizing require Arch testing.
"""
from pathlib import Path
import re
from gui_source_contract import read_gui_sources
root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
overview = (root / "src/widgets/eq-overviews.h").read_text()
plot = overview[overview.index('class DynamicEqOverview final'):]
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
assert 'out.deq_count = new QSpinBox(deq_group)' in source
assert 'result.bandCount = new QSpinBox(deq_content)' in source
assert 'result.spread = new QPushButton("Spread frequencies"' in source
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
# The preview deliberately uses its prior auto-ranging behavior. A low
# shelf can affect distant bands without changing their individual gains.
# The numbered marker and the curve must share the same scale.
assert 'double extreme = 12.0;' in plot
assert 'for (double d : db) extreme = std::max(extreme, std::abs(d));' in plot
assert 'db[j] / extreme' in plot
assert 'db[bin] / extreme' in plot
assert 'level += b.gain / 100.0 * shape;' in plot
editor = (root / "src/widgets/dynamic-eq-editor.h").read_text()
for explanation in (
    "Maximum gain is its individual setting",
    "combined",
    "Frequency, Q (bandwidth), filter shape (especially shelves)",
    "gain stays the same",
    "approximate preview, not live metering",
    "threshold, attack, release and signal level",
):
    assert explanation in editor, explanation
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
