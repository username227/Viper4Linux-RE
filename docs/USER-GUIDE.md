# User guide

## Running ViPER for Linux

Launch the installed application with `viper4linux`. It checks the installed
backend fingerprint (DSP module, controller and PipeWire routing configuration),
reuses a matching running backend, and replaces an older marked ViPER backend
when necessary. This does **not** restart PipeWire itself. GUI-only updates
should leave the backend and music playing uninterrupted.

The main window has six effect tabs. Each effect has an independent switch:
collapsing a panel does not disable its processing. **Master** bypasses effects,
though the native master limiter remains in the output path. **Theme** selects
Modern Teal or Classic Viper and follows supported desktop light/dark settings.
The compact **☰ menu** provides Preferences, IRS/VDC library shortcuts,
backend log, Effect Guide, About, Presets, and Quit GUI.

## Window, tray, and login options

Open **☰ → Preferences** to configure:

- Startup: show window, start minimized to the taskbar, or start hidden in the tray.
- Title-bar minimize: normal taskbar minimize or minimize to tray.
- Close (X): quit the GUI, minimize to taskbar, or hide to tray.
- Optional login autostart using the user's XDG autostart directory.

The tray menu provides Show/Hide, the Master processing toggle, Preferences,
and Quit GUI. The tray click action toggles window visibility. If Qt reports
no usable system tray, the application should keep its window accessible rather
than hide it permanently. On Cinnamon/X11 the black taskbar-window regression
was fixed by not clearing the minimized state after hiding the window.
KDE Plasma tray behavior remains to be verified on a real KDE session.

**Quit GUI** closes the interface; it does not stop the independent audio backend.
For a full backend stop during development, see the scripts below. To verify
login autostart you only need to log out and back in, not reboot.

## CLI and troubleshooting

The installed command is `viper4linux-ctl`. Try:

```bash
viper4linux-ctl status
viper4linux-ctl off
viper4linux-ctl on
viper4linux-ctl bass-off
viper4linux-ctl bass-on
viper4linux-ctl --help
man viper4linux-ctl
```

The CLI writes **live shared-memory controls**. It does not yet offer full GUI
parity or persistent named presets; GUI changes and CLI changes may supersede
each other. The commands `on` and `off` control the **global ViPER bypass**;
`bass-on`, `bass-off`, and `bass-toggle` control only **ViPER Bass**.
Legacy `all-on`/`all-off` and `toggle` aliases remain accepted for scripts.

For a direct-source development build, useful scripts are `./status.sh`,
`./verify.sh`, `./stop.sh` and `./start.sh`. The source installer builds
`build/viper-ctl`, `build/viper-gui`, and `build/viper_dsp_ladspa.so`.

Backend startup logs are under `$XDG_STATE_HOME/viper4linux/backend.log`
(normally `~/.local/state/viper4linux/backend.log`). Verify that the expected
PipeWire sink is active and that the audio-block counter in CLI status advances
while audio plays. If routing becomes ineffective while JamesDSP is active,
check routing before assuming an effect implementation has failed.

## Effect and file guidance

See [Effects](EFFECTS.md) for processor behavior and restrictions. For
importing `.irs` and `.vdc` files, see [Presets and libraries](PRESETS-AND-LIBRARIES.md).
