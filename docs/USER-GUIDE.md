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

The installed CLI command is `viper4linux-ctl`, with the shorter `v4l-cli`
symlink providing the same commands and options. Try:

```bash
viper4linux-ctl status
v4l-cli status
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
  running, but its DSP control channel has not appeared yet. The smart filter
  is configured to initialize the DSP even without music, so this should be
  transient at startup rather than requiring playback. The status command
  returns success (exit code 0), but effect-setting commands fail (exit code 1)
  until the control channel exists. A persistent state warrants checking the
  backend log and filter-chain configuration.
- **Offline:** no marked ViPER backend process is detected. Start `viper4linux`.

`viper4linux-ctl status` is instantaneous and reports the current callback
counter rather than claiming active/idle from a single sample. Run it twice
if you need to check whether the number is increasing. The GUI observes changes
automatically. Closing a music player does not unload or restart the DSP.

If audio is playing but the status remains **Ready to process audio**, inspect
routing and the backend log: a process can be alive without a working DSP.

The CLI writes **live shared-memory controls** and immediately saves
successful changes to the same per-user Qt settings store as the GUI, even when
the GUI is closed. It does not manage named presets, IRS/VDC file selection, or
all advanced GUI parameters. If the settings store is unwritable, the command
reports a persistence failure even though the live DSP change may already have
taken effect. GUI changes and CLI changes may supersede each other. The commands `on` and `off` control the **global ViPER bypass**;
`bass-on`, `bass-off`, and `bass-toggle` control only **ViPER Bass**.
Legacy `all-on`/`all-off` and `toggle` aliases remain accepted for scripts.

For Headphone Surround+, the CLI uses the **same Level 1 through Level 5**
numbering shown by the GUI. For instance, `viper4linux-ctl headphone-level 2`
selects GUI Level 2 (the backend stores index 1). The only supported command
name is `headphone-level`; the older `headphone-quality` name was retired
during the CLI cleanup. Invalid values return exit status 2 without changing
the current level. Use `status` to inspect the live level before and after
setting it.

## Audio Output memory and preset assignment

**Preferences → Remember settings per Audio Output** enables independent DSP
settings for each positively identified physical output. The monitored
selection follows PipeWire's physical sink and continues after the GUI closes.
The monitor waits for the output to settle before saving or restoring settings;
switching outputs while playback is paused is supported.

Open **☰ → Presets…** to save complete effect settings. **Save Preset…**
defaults to **New Preset…**, or you can pick an existing preset for a confirmed
overwrite. To assign a saved preset to internal speakers, headphones, or a
remembered disconnected output, select it and choose **Apply to Audio Output…**.
Confirm the target device. If it's active, settings are applied immediately;
otherwise, the existing audio stays unchanged until that output becomes active.
Saved output settings subsequently evolve independently of the named preset.

For an inspection of current outputs use `viper4linux-output --json`.
The companion `viper4linux-output --enable-memory` and `--disable-memory`
switches control the opt-in store; `--assign-preset FILE --target OUTPUT_KEY`
uses the monitored, acknowledged assignment path. The keys are exact,
remembered physical identifiers, not display-name matches.

For a direct-source development build, useful scripts are `./scripts/status.sh`,
`./scripts/verify.sh`, `./scripts/stop.sh` and `./scripts/start.sh`. The source installer builds
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
them on its 500 ms polling cycle and saves changed controls. When the GUI is closed, successful CLI changes still save directly to the
GUI's Qt preferences; they survive a backend restart without reopening the
GUI. For settings such as a DDC file or convolver impulse response, select
the file in the GUI; the CLI can toggle the effect after it is loaded.

The CLI returns status `0` for a successful command, `1` if no backend is
running, and `2` for invalid commands or arguments. See
`man viper4linux-ctl` for the available switches and argument ranges.
