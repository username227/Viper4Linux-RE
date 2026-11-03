#!/usr/bin/env python3
"""Source-level tray/menu lifecycle contracts (live Cinnamon/KDE still needed)."""
from pathlib import Path
from gui_source_contract import read_gui_sources
s = read_gui_sources(Path(__file__).resolve().parents[1])
assert 'app.setQuitOnLastWindowClosed(false);' in s
assert 'window.showAtStartup();' in s
assert 'menuButton->setMenu(makeApplicationMenu(menuButton));' in s
assert 'QToolButton::InstantPopup' in s
assert 'viperAboutButton' not in s
for entry in ('Preferences…', 'Open Library', 'IRS impulse responses',
              'VDC correction profiles', 'View Backend Log', 'Effect Guide',
              'About Viper4Linux', 'Quit GUI'):
    assert entry in s, entry
for key in ('ui/startupBehavior', 'ui/minimizeToTray', 'ui/closeBehavior'):
    assert key in s, key
assert 'Start hidden in system tray' in s
assert 'Start minimized to taskbar' in s
assert 'Minimize to tray instead of taskbar' in s
assert 'Hide to system tray' in s
assert 'QSystemTrayIcon::isSystemTrayAvailable()' in s
assert 'if (!trayAvailable()) return;' in s
assert 'System tray unavailable; the window stays open.' in s
assert 'System tray unavailable; showing the main window instead.' in s
assert 'QSystemTrayIcon::Trigger' in s
assert 'QSystemTrayIcon::DoubleClick' in s
assert 'tray_processing_action_->setCheckable(true);' in s
assert 'global_->setChecked(enabled);' in s
assert 'viper4linux-re.desktop' in s
assert 'QSaveFile file(target);' in s
assert 'Exec=viper4linux' in s
assert 'TryExec=viper4linux' in s
assert 'qApp->quit(); // Does not touch the separate PipeWire backend.' in s
# Minimize-to-tray withdraws the window without clearing its state while
# hidden (Qt/X11 could otherwise leave a blank ghost in Cinnamon's taskbar).
minimize_handler = s[s.index('    void changeEvent(QEvent* event) override {'):
                     s.index('    bool eventFilter(QObject* watched, QEvent* event) override {')]
assert 'QWindowStateChangeEvent' in s
assert 'tray_restore_maximized_ = was_maximized;' in minimize_handler
assert 'hide();' in minimize_handler
assert 'setWindowState(' not in minimize_handler
assert 'showNormal();' in s[s.index('    void restoreFromTray() {'):
                            s.index('    void quitGui() {')]
assert 'showMaximized();' in s[s.index('    void restoreFromTray() {'):
                               s.index('    void quitGui() {')]
# Explicit taskbar minimization paths still bypass the tray preference.
assert 'force_taskbar_minimize_ = true;' in s
assert 'force_taskbar_minimize_' in minimize_handler
# Tray/close GUI code cannot launch, stop, or fingerprint the backend.
for begin, end in (('    void setupTray() {', '    QString autostartFile() const {'),
                   ('    void closeEvent(QCloseEvent* event) override {',
                    '    bool eventFilter(QObject* watched, QEvent* event) override {')):
    section = s[s.index(begin):s.index(end)]
    for banned in ('ensureBackend(', 'QProcess::start(', 'start.sh', 'stop.sh',
                   'publishToBackend(', 'running_clients('):
        assert banned not in section, (begin, banned)
print('PASS: compact menu and tray lifecycle, availability fallback, settings, no backend lifecycle calls')
