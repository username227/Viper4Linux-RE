#!/usr/bin/env python3
"""Structural contract: main window composes exactly-once internal modules."""
from pathlib import Path
from gui_source_contract import read_gui_sources
root = Path(__file__).resolve().parents[1]
main = (root / "src/viper-gui.cpp").read_text()
# The class fragments must still be composed exactly once. Method ownership
# within a particular .inc isn't a functional requirement; definitions are
# checked against the full composed source below.
modules = (
    "window-presets.inc",
    "window-layout.inc",
    "window-correction.inc",
    "window-shell.inc",
    "window-graphic-eq.inc",
    "window-initialization.inc",
    "window-dynamics.inc",
    "window-effect-actions.inc",
)
for name in modules:
    assert main.count('#include "window/' + name + '"') == 1, name
assert 'ViperWindow() : settings_("ViPERforLinux", "ViPERClean")' in main
for marker in ('loadSavedEffectSettings();',
               'connectEffectSignals(eq_reset, deq_spread);',
               'setupThemeMonitoring();'):
    assert main.count(marker) == 1, marker
assert (main.index('loadSavedEffectSettings();') <
        main.index('connectEffectSignals(eq_reset, deq_spread);') <
        main.index('setupThemeMonitoring();'))
initialization = (root / "src/window/window-initialization.inc").read_text()
for method in ('void loadSavedEffectSettings()',
               'void connectEffectSignals(QPushButton* eq_reset, QPushButton* deq_spread)',
               'void setupThemeMonitoring()'):
    assert initialization.count(method) == 1, method
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
    'void push() {', 'void showOffline() {',
    'void showPreferences()', 'void migrateEqPrecision()',
    'void refreshDeqBandSelector()', 'void loadSavedEffectSettings()',
):
    assert all_source.count(marker) == 1, marker
# The DDC/IRS watchers and handlers must still be wired once, with their
# setup after the corresponding panel exists. Either .inc may own the code.
for invocation, signature in (
    ("setupDdcLibrary(importDdc, refreshDdc);",
     "void setupDdcLibrary(QPushButton* importDdc, QPushButton* refreshDdc)"),
    ("setupIrsLibrary(browse, refresh);",
     "void setupIrsLibrary(QPushButton* browse, QPushButton* refresh)"),
):
    assert main.count(invocation) == 1, invocation
    assert all_source.count(signature) == 1, signature
for key in ("connect(ddc_watcher_", "connect(irs_watcher_",
            "connect(ddc_library_", "connect(convolver_library_",
            "connect(importDdc,", "connect(browse,"):
    assert all_source.count(key) == 1, key
assert main.index('addEffectPanel(correctionLayout, correctionDdc.group, "ddc", ddc_enabled_)') < main.index('setupDdcLibrary(importDdc, refreshDdc);')
assert main.index('addEffectPanel(correctionLayout, correctionConvolver.group, "convolver", convolver_enabled_)') < main.index('setupIrsLibrary(browse, refresh);')
print("PASS: stage 7 main-window integration and once-only module composition")
