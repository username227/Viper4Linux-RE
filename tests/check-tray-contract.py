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
              'VDC correction profiles', 'View ViPER Log', 'Log Retention…', 'Effect Guide',
              'About Viper4Linux', 'Quit GUI'):
    assert entry in s, entry
assert s.index('View ViPER Log') < s.index('Log Retention…') < s.index('Effect Guide')
for key in ('ui/startupBehavior', 'ui/minimizeToTray', 'ui/closeBehavior'):
    assert key in s, key
assert 'Start hidden in system tray' in s
assert 'Start minimized to taskbar' in s
assert 'Minimize to tray instead of taskbar' in s
assert 'Hide to system tray' in s
# Preferences geometry and layout contracts. Cancel only saves dialog size.
prefs = s[s.index('    void showPreferences() {'):s.index('    QString logRetentionHelper() const {')]
for title in ('"Startup"', '"Window & tray behavior"',
              '"GUI exit & audio backend"', '"Audio Output memory"'):
    assert title in prefs, title
assert prefs.index('"Startup"') < prefs.index('"Window & tray behavior"')
assert prefs.index('"Window & tray behavior"') < prefs.index('"GUI exit & audio backend"')
assert prefs.index('"GUI exit & audio backend"') < prefs.index('"Audio Output memory"')
for token in ('QScrollArea', 'setWidgetResizable(true)', 'Qt::ScrollBarAlwaysOff',
              'QDialogButtonBox::ResetRole', '"Restore Defaults"',
              'ui/preferencesDialogSize',
              'settings_.setValue("ui/preferencesDialogSize", dialog.size());',
              'if (outcome != QDialog::Accepted) return;'):
    assert token in prefs, token
assert prefs.index('settings_.setValue("ui/preferencesDialogSize", dialog.size());') < prefs.index('if (outcome != QDialog::Accepted) return;')
assert prefs.index('if (outcome != QDialog::Accepted) return;') < prefs.index('settings_.setValue("ui/startupBehavior"')
assert 'If the desktop tray is unavailable' not in prefs
# Font-independent, theme-accented icons and prominent headings.
assert 'preferencesSectionHeading' in prefs
assert 'headingFont.setBold(true)' in prefs
assert 'glyph->setColor(dialog.palette().color(QPalette::Highlight));' in prefs
for icon in ('prefs_startup', 'prefs_window', 'prefs_backend', 'prefs_memory'):
    assert icon in prefs, icon
glyphs = (Path(__file__).resolve().parents[1] / 'src/widgets/effect-widgets.h').read_text()
for icon in ('prefs_startup', 'prefs_window', 'prefs_backend', 'prefs_memory'):
    assert f'kind_ == "{icon}"' in glyphs, icon
assert all(x in prefs for x in ('addChoices(startLayout', 'addChoices(windowLayout', 'addChoices(exitLayout'))
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
assert 'if (!prepareBackendForGuiExit()) return;' in s
assert 'qApp->quit(); // Only an explicit Stop choice signals the marked backend.' in s
assert 'quitGui();' in s[s.index('void closeEvent(QCloseEvent* event) override {'):s.index('void changeEvent(QEvent* event) override {')]
assert 'event->ignore();' in s[s.index('void closeEvent(QCloseEvent* event) override {'):s.index('void changeEvent(QEvent* event) override {')]
assert 'ui/backendOnExit' in s and 'Ask every time' in s
assert 'Remember this choice' in s and 'Stop backend' in s
assert 'viper_gui_exit::stopMarkedBackend(backendPid, error)' in s
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
# A native Wayland GUI cannot observe compositor-side title-bar minimization.
# Never switch the Qt platform automatically; allow a deliberate xcb override.
main = (Path(__file__).resolve().parents[1] / 'src/viper-gui.cpp').read_text()
entry = main[main.index('int main(int argc, char* argv[]) {'):]
assert 'QApplication app(argc, argv);' in entry
assert 'qputenv("QT_QPA_PLATFORM", "xcb");' not in main
assert 'windowSettings.value("ui/minimizeToTray"' not in entry
assert 'Minimize to tray instead of taskbar (X11 only)' in prefs
assert 'const bool x11Gui = QGuiApplication::platformName() == QLatin1String("xcb");' in prefs
assert 'minimize->setEnabled(x11Gui);' in prefs
# The app theme normally forces checkbox text to an active foreground color,
# so ensure disabled controls visibly use the existing muted palette.
theme = (Path(__file__).resolve().parents[1] / 'src/viper-theme.h').read_text()
assert 'QPushButton:disabled, QComboBox:disabled, QCheckBox:disabled { color:%7; }' in theme
assert 'minimize->setChecked(settings_.value("ui/minimizeToTray", false).toBool())' in prefs
assert 'if (x11Gui)\n            settings_.setValue("ui/minimizeToTray", minimize->isChecked());' in prefs
assert 'QGuiApplication::platformName() != QLatin1String("xcb")' in minimize_handler
assert minimize_handler.index('QMainWindow::changeEvent(event);') < minimize_handler.index('QGuiApplication::platformName() !=')
assert minimize_handler.index('QGuiApplication::platformName() !=') < minimize_handler.index('if (!is_state_change || !isMinimized()')
# Tray/close GUI code cannot launch, stop, or fingerprint the backend.
for begin, end in (('    void setupTray() {', '    QString autostartFile() const {'),
                   ('    void closeEvent(QCloseEvent* event) override {',
                    '    bool eventFilter(QObject* watched, QEvent* event) override {')):
    section = s[s.index(begin):s.index(end)]
    for banned in ('ensureBackend(', 'QProcess::start(', 'start.sh', 'stop.sh',
                   'publishToBackend(', 'running_clients('):
        assert banned not in section, (begin, banned)
print('PASS: compact menu and tray lifecycle, availability fallback, settings, no backend lifecycle calls')
