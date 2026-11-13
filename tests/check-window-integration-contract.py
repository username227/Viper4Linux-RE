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
    ("window-initialization.inc", "void loadSavedEffectSettings()"),
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
# Constructor setup remains once-only and in its existing panel order.
correction = (root / "src/window/window-correction.inc").read_text()
for invocation, signature in (
    ("setupDdcLibrary(importDdc, refreshDdc);",
     "void setupDdcLibrary(QPushButton* importDdc, QPushButton* refreshDdc)"),
    ("setupIrsLibrary(browse, refresh);",
     "void setupIrsLibrary(QPushButton* browse, QPushButton* refresh)"),
):
    assert main.count(invocation) == 1, invocation
    assert correction.count(signature) == 1, signature
    assert invocation not in correction
for key in ("connect(ddc_watcher_", "connect(irs_watcher_",
            "connect(ddc_library_", "connect(convolver_library_",
            "connect(importDdc,", "connect(browse,"):
    assert correction.count(key) == 1, key
    assert key not in main, key
assert main.index('addEffectPanel(correctionLayout, correctionDdc.group, "ddc", ddc_enabled_)') < main.index('setupDdcLibrary(importDdc, refreshDdc);')
assert main.index('addEffectPanel(correctionLayout, correctionConvolver.group, "convolver", convolver_enabled_)') < main.index('setupIrsLibrary(browse, refresh);')
# The main-window footer, active tab migration, and status row are built
# only after the effect panels and before any saved effect values are loaded.
shell = (root / "src/window/window-shell.inc").read_text()
assert main.count("completeMainWindowLayout(central, layout);") == 1
assert shell.count("void completeMainWindowLayout(QWidget* central, QVBoxLayout* layout)") == 1
assert main.index("outputLayout->insertWidget(outputLayout->count() - 1, output_warning_);") < main.index("completeMainWindowLayout(central, layout);") < main.index("save_timer_ = new QTimer(this);") < main.index("loadSavedEffectSettings();")
for marker in (
    'audio_restart_button_ = new QPushButton("Restart Backend", central)',
    'presetsButton->setObjectName("viperNamedPresetsButton")',
    'footerDivider->setObjectName("viperFooterDivider")',
    'layout->addWidget(tabs_, 1);',
    'layout->addLayout(footerControls);',
    'settings_.value("ui/active_tab_name", "Output")',
    'settings_.setValue("ui/active_tab_name",',
    'status_ = new QLabel("Checking backend...", central)',
    'setCentralWidget(central);',
):
    assert marker in shell, marker
    assert marker not in main, marker
assert shell.index('layout->addWidget(tabs_, 1);') < shell.index('layout->addLayout(footerControls);') < shell.index('status_ = new QLabel("Checking backend...", central)') < shell.index('setCentralWidget(central);')

print("PASS: stage 7 main-window integration and once-only module composition")
