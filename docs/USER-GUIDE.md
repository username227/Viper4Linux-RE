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

Backend status distinguishes the following conditions:

- **Processing audio (GUI):** the backend control channel is present and its
  `audio_callbacks` counter advances between 500ms GUI polls. This proves DSP
  callbacks are running, **not** that non-silent music is audible.
- **Connected — idle (GUI):** the DSP channel is present but its audio callback
  counter has not advanced since the previous GUI poll; effects are ready.
- **Connected — checking audio activity (GUI):** the first sample after attaching
  or restarting the DSP. A second sample is needed to classify active versus idle.

- **Ready to process audio:** the dedicated, marked PipeWire backend process is
  running, but its DSP control channel is not active yet. This is expected
  before the first audio stream activates the filter-chain processor. GUI and
  `viper4linux-ctl status` show this state instead of inaccurately saying the
  backend is offline. The status command returns success (exit code 0), but
  effect-setting commands fail (exit code 1) until the control channel exists.
- **Offline:** no marked ViPER backend process is detected. Start `viper4linux`.

`viper4linux-ctl status` is instantaneous and reports the current callback
counter rather than claiming active/idle from a single sample. Run it twice
if you need to check whether the number is increasing. The GUI observes changes
automatically. Closing a music player does not unload or restart the DSP.

If audio is playing but the status remains **Ready to process audio**, inspect
routing and the backend log: a process can be alive without a working DSP.

The CLI writes **live shared-memory controls**. It does not yet offer full GUI
parity or persistent named presets; GUI changes and CLI changes may supersede
each other. The commands `on` and `off` control the **global ViPER bypass**;
`bass-on`, `bass-off`, and `bass-toggle` control only **ViPER Bass**.
Legacy `all-on`/`all-off` and `toggle` aliases remain accepted for scripts.

For Headphone Surround+, the CLI uses the **same Level 1 through Level 5**
numbering shown by the GUI. For instance, `viper4linux-ctl headphone-level 2`
selects GUI Level 2 (the backend stores index 1). The historical command name
`headphone-quality` is accepted as an alias, but its argument **also uses 1..5**:
existing scripts using its former 0..4 values must add one. Invalid values
return exit status 2 without changing the current level. Use `status` to
inspect the live level before and after setting it.

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

## Automating effects with the CLI

`viper4linux-ctl` can toggle every effect group and configure many common
parameters without starting the GUI. The master `on`/`off` bypass controls
are distinct from `bass-on`/`bass-off`, and Headphone Surround+ uses the same
one-based `headphone-level 1..5` as the GUI.

```sh
viper4linux-ctl on
viper4linux-ctl headphone-level 2
viper4linux-ctl imager-on
viper4linux-ctl imager-mid 149
```

The interface applies changes to the **running** backend. An open GUI reflects
them on its 500 ms polling cycle and saves changed controls. When the GUI is
closed, the CLI does **not** write Qt preferences: reopen the GUI before the
backend restarts if you want those CLI changes saved for later sessions. For
settings such as a DDC file or convolver impulse response, select the file in
the GUI; the CLI can toggle the effect after it is loaded.

The CLI returns status `0` for a successful command, `1` if no backend is
running, and `2` for invalid commands or arguments. See
`man viper4linux-ctl` for the available switches and argument ranges.
