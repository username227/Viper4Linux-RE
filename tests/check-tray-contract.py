#!/usr/bin/env python3
"""Keep only safety-critical tray and Preferences lifecycle guards."""
from pathlib import Path
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
assert "app.setQuitOnLastWindowClosed(false);" in source
assert "window.showAtStartup();" in source
assert "QSystemTrayIcon::isSystemTrayAvailable()" in source
assert "if (!trayAvailable()) return;" in source

# Preferences cancellation must not apply changes, but may persist geometry.
prefs = source[source.index("    void showPreferences() {"):source.index("    QString logRetentionHelper() const {")]
assert prefs.index('settings_.setValue("ui/preferencesDialogSize", dialog.size());') < prefs.index("if (outcome != QDialog::Accepted) return;")
assert prefs.index("if (outcome != QDialog::Accepted) return;") < prefs.index('settings_.setValue("ui/startupBehavior"')

# Never hide the window if the tray cannot accept it.
assert "System tray unavailable; showing the main window instead." in source
handler = source[source.index("    void changeEvent(QEvent* event) override {"):
                 source.index("    bool eventFilter(QObject* watched, QEvent* event) override {")]
assert "tray_restore_maximized_ = was_maximized;" in handler
assert "hide();" in handler
assert "setWindowState(" not in handler
assert "force_taskbar_minimize_" in handler

# Native Wayland must remain native unless the user selects XWayland.
main = (root / "src/viper-gui.cpp").read_text()
assert 'qputenv("QT_QPA_PLATFORM", "xcb");' not in main
assert 'const bool x11Gui = QGuiApplication::platformName() == QLatin1String("xcb");' in prefs
assert "minimize->setEnabled(x11Gui);" in prefs
assert 'QGuiApplication::platformName() != QLatin1String("xcb")' in handler

# Tray and close actions must never directly manage the audio backend.
for begin, end in (
    ("    void setupTray() {", "    QString autostartFile() const {"),
    ("    void closeEvent(QCloseEvent* event) override {",
     "    bool eventFilter(QObject* watched, QEvent* event) override {"),
):
    section = source[source.index(begin):source.index(end)]
    for forbidden in ("ensureBackend(", "QProcess::start(", "start.sh",
                      "stop.sh", "publishToBackend(", "running_clients("):
        assert forbidden not in section, (begin, forbidden)
print("PASS: tray fallback, Preferences cancel, Wayland and backend isolation")
