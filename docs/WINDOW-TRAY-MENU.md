# Application menu and desktop tray — development notes

The main window stays intentionally minimal. The top-right ☰ opens Preferences,
the IRS/VDC libraries, the backend log viewer, Effect Guide, About and Quit GUI.
Theme, master bypass, the six effect tabs, preset actions and connection status
remain on the primary window.

## Window & Tray preferences

The startup choice is exclusive: show normally, minimize to the taskbar, or
start hidden in the tray. The minimize button can independently minimize to the
tray. The close (X) button has an exclusive action: quit just the GUI, minimize
to taskbar, or hide to the tray. All settings are stored under `ui/*` in the
existing ViPERforLinux/ViPERClean Qt settings; DSP and presets are unchanged.

On Cinnamon/X11 and KDE Plasma, Qt's `QSystemTrayIcon` selects the system
tray integration provided by the desktop. **If Qt cannot find a tray**, hidden
startup instead displays the window, minimize falls back to the taskbar, and
close-to-tray leaves the window visible with an explanation. This guards
against invisible applications. Actual Cinnamon and KDE behavior must be
checked in a real desktop session; source contracts alone do not prove it.

The tray menu provides Show/Hide, Enable Processing (the existing master
switch), Preferences and Quit GUI. Clicking the icon also toggles the window.
Quit GUI never stops the separate PipeWire backend. Its fingerprint/upgrade
logic is untouched. The tray checkbox uses the normal master-switch handler.

An opt-in XDG autostart entry is written to:
`$XDG_CONFIG_HOME/autostart/viper4linux-re.desktop` (or ~/.config equivalent).
It launches the normal `viper4linux` command so the version-aware backend
launcher still runs. Unchecking autostart removes only this managed file.

## Manual checks before release

1. Test tray icon, left click, right click, GUI Quit, and backend continued
   playback under Cinnamon X11; repeat on KDE if possible.
2. Test all three startup and three close actions; explicitly test the
   minimize-to-tray checkbox with close-to-taskbar selected.
3. Temporarily disable the desktop tray and verify the window remains visible.
4. Verify autostart entry appears in desktop Startup Applications settings,
   launches only on login, and removes cleanly when unchecked.
5. Confirm that GUI-only upgrades do not restart the DSP backend.
