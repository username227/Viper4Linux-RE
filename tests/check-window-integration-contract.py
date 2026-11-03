#!/usr/bin/env python3
"""Structural contract: main window composes exactly-once internal modules."""
from pathlib import Path
from gui_source_contract import read_gui_sources
root = Path(__file__).resolve().parents[1]
main = (root / "src/viper-gui.cpp").read_text()
modules = (
    ("window-presets.inc", "void resetEffectsWithConfirmation()"),
    ("window-layout.inc", "void applyTheme(bool classic)"),
    ("window-correction.inc", "void refreshIrsLibrary()"),
    ("window-shell.inc", "void showPreferences()"),
    ("window-graphic-eq.inc", "void migrateEqPrecision()"),
    ("window-dynamics.inc", "void refreshDeqBandSelector()"),
    ("window-effect-actions.inc", "void applySettings(bool master"),
)
for name, marker in modules:
    path = root / "src/window" / name
    data = path.read_text()
    assert marker in data, (name, marker)
    assert main.count('#include "window/' + name + '"') == 1, name
    assert marker not in main, (name, "implementation retained in main")
assert 'ViperWindow() : settings_("ViPERforLinux", "ViPERClean")' in main
assert 'void push() {' in main
assert 'void showOffline() {' in main
assert 'bool eventFilter(QObject* watched, QEvent* event) override' in main
assert '#include "sync/gui-backend-bridge.inc"' in main
assert 'ViperWindow window;' in main
assert 'window.showAtStartup();' in main
all_source = read_gui_sources(root)
for marker in (
    'void persistCurrentSettings()', 'bool publishToBackend()',
    'void readBackend()', 'void pollBackend()',
    'void resetEffectsWithConfirmation()', 'void requestIrsLoad()',
    'void applyTheme(bool classic)', 'void updateEqOverview()',
    'void updateDeqOverview()', 'void applySettings(bool master',
):
    assert all_source.count(marker) == 1, marker
print("PASS: stage 7 main-window integration and once-only module composition")
