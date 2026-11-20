# Changelog

This file summarizes the user-facing changes in each release. Internal build,
CI and documentation-only maintenance is generally excluded from these notes.

## v1.2.1

### Fixed

- **Prevent high CPU usage after audio becomes idle**
  ([PR #49](https://github.com/username227/Viper4Linux-RE/pull/49),
  contributed by [ZeroAnarchy](https://github.com/ZeroAnarchy)):
  Enable flush-to-zero handling for extremely small floating-point values
  in the DSP audio callback. This prevents lingering filter signals from
  triggering costly denormal arithmetic after playback stops, without
  changing audible audio or saved effect settings. Includes a dedicated
  regression test for the floating-point environment.

## v1.2.0

### Added

- **Native CLI and launcher lifecycle controls**
  ([PR #43](https://github.com/username227/Viper4Linux-RE/pull/43)):
  The installed `viper4linux` launcher now uses the native controller rather
  than the previous Python launcher. `v4l-cli --quit-gui` closes only the
  interface and leaves processing running; `--quit-all` closes the interface
  and stops ViPER's marked backend; `--restart-backend` restarts the ViPER
  backend and Audio Output monitor. None of these commands restarts the
  system PipeWire daemon. Launch preflight remains read-only.

- **Complete named-effect preset management from the CLI**
  ([PR #43](https://github.com/username227/Viper4Linux-RE/pull/43)):
  `v4l-cli --preset` lists the GUI's shared preset library; save, explicit
  overwrite, load, rename and delete operations are available through
  `--preset ACTION NAME`. A separate `--reset-effects` command restores
  DSP defaults without deleting named presets or imported IRS/VDC files.
  The CLI reset is noninteractive; save a preset beforehand if needed.

- **Broader effect-by-effect CLI control**
  ([PR #33](https://github.com/username227/Viper4Linux-RE/pull/33)
  and [PR #43](https://github.com/username227/Viper4Linux-RE/pull/43)):
  Effect-scoped commands cover all 24 DSP effect groups, including detailed
  FET and Multiband Compressor settings, indexed Dynamic EQ bands, independent
  10/15/25/31-band Graphic EQ curves, EQ preset operations, and IRS/VDC
  selection or import. Per-effect help, expanded read-only status and
  validated multi-effect command sequences make these controls usable
  without opening the GUI. Physical Audio Output memory and preset
  assignment continue to use the separate `viper4linux-output` tool.

- **Independent Reset button in the GUI:** Reset Effects to Defaults now
  has its own footer button with a cancel-first confirmation and a reminder
  to save the current sound as a named preset. Presets and imported
  correction files are not deleted.

### Changed

- **Consistent effect-scoped CLI syntax**
  ([PR #33](https://github.com/username227/Viper4Linux-RE/pull/33)):
  Use `v4l-cli --bass on gain=3`,
  `v4l-cli --headphone on level=2`, or
  `v4l-cli --eq bands=31 level=9:-2`. Commands validate every option in
  a sequence before applying changes and report the final effect states
  once. Legacy value-taking positional forms are no longer accepted;
  documented no-argument aliases remain available for older scripts.

- **Explicit tray exit choices:** The tray menu now distinguishes
  **Quit GUI & Leave ViperDSP Running** from
  **Quit GUI & Close ViperDSP Completely**. These actions bypass the
  remembered interactive exit choice without changing that preference.

- **More accurate and readable GUI audio status**
  ([PR #45](https://github.com/username227/Viper4Linux-RE/pull/45)):
  Incoming audio and connection readiness use a green status indicator.
  When Master effects are bypassed, the GUI shows a separate yellow
  **effects bypassed** notice instead of implying that DSP effects are
  processing the detected signal. Light/dark variants improve contrast;
  signal detection is still not proof of audible speaker output.

### Fixed

- **More reliable Cinnamon/X11 tray and taskbar restoration**
  ([PR #40](https://github.com/username227/Viper4Linux-RE/pull/40)):
  Improved startup-notification handling for tray-only launch, restoration
  of hidden/minimized windows, and native Wayland activation behavior.
  Platform-specific native Wayland tray/window limitations remain.

- **More reliable CLI state synchronization:** Native worker updates are
  acknowledged before successful completion is reported, with a bounded
  timeout. Persisted effect values, GUI readback and preset restoration
  share the live DSP control state; a runtime failure is reported rather
  than silently presented as a successful change.

## v1.1.3

### Changed

- **Clearer Dynamic System controls:**
  Bass strength is now the primary adjustment. Setting Low X to **120 Hz**
  selects simple bass processing, while higher values enable the advanced
  filters and additional controls. Settings that do not apply in simple mode
  are visibly greyed out, making it easier to understand which adjustments
  are active.

- **Better handling of native Wayland limitations:**
  The **Minimize to tray instead of taskbar** preference is now clearly marked
  as **X11 only** and disabled when running under native Wayland. Existing
  X11 preferences are preserved. Users can manually run with XWayland to
  maintain minimize to tray settings.

- **Refined main-window layout:**
  The application logo is larger, and the current Audio Output is positioned
  directly beneath the application title. The layout provides clearer visual
  organization while retaining the existing themes and controls.

### Fixed

- **Master OFF now completely bypasses audio processing:**
  Turning Master OFF now passes audio through unchanged after a brief
  transition, without applying the limiter, output gain, channel balance or
  their processing delay.

- **Correct initial gain for ViPER Bass and ViPER Bass Mono:**
  Both effects now apply the selected bass strength correctly when first
  enabled, rather than briefly starting with an incorrect gain value.
  Subsequent adjustments retain their normal smooth transitions.

- **Restored proper Psychoacoustic Bass harmonic processing:**
  Corrected a calculation that could discard small fractional values used to
  generate bass harmonics. This restores the intended processing behavior and
  corrects the effect's initial response.

- **Fixed FET Compressor gain reduction:**
  Corrected a signal-level calculation that could prevent the compressor from
  responding properly to ordinary audio signals. Threshold and
  compression-ratio adjustments now produce the intended gain reduction
  instead of leaving much of the signal effectively uncompressed.

- **More reliable Multiband Compressor initialization:**
  The compressor now loads the selected settings for all active frequency
  bands before enabling processing. This prevents incorrect initial values
  when first enabling the effect or switching between three-band and five-band
  operation. Adjustments made during playback continue to transition
  smoothly.

- **Preserved precise Dynamic System values entered through the CLI:**
  Low X frequency values below the GUI's 120 Hz minimum are no longer
  accidentally overwritten when the GUI reads or saves settings. The GUI
  displays its minimum value while preserving the more precise underlying
  setting until the user deliberately changes the slider.

## v1.1.2

### Added

- **Restart the ViPER audio backend without closing the GUI**
  ([PR #7](https://github.com/username227/Viper4Linux-RE/pull/7)):
  A **Restart Backend** button and application-menu action restart the ViPER
  DSP backend and Audio Output monitor, then reconnect the open GUI after
  verifying the new monitor is running. The launcher also supports
  `viper4linux --restart-audio`. This restarts **only ViPER's components**,
  not the system PipeWire service; audio may pause briefly during the restart.

- **Manage diagnostic log retention**
  ([PR #14](https://github.com/username227/Viper4Linux-RE/pull/14)):
  The application menu provides **View ViPER Log** and **Log Retention…**.
  Logs are retained for **seven days by default**; users can choose another
  period or **0** to keep them indefinitely. Retention changes take effect
  immediately, and background cleanup runs periodically while the monitor
  is active. The cleanup is designed not to interrupt the audio backend.

- **Combined, timestamped troubleshooting log**
  ([PR #12](https://github.com/username227/Viper4Linux-RE/pull/12),
  [PR #14](https://github.com/username227/Viper4Linux-RE/pull/14)):
  Backend, launcher and Audio Output monitor diagnostics now appear in a
  single `backend.log`, with timestamps, source labels and process IDs to
  help investigate startup, restarts and unexpected interruptions. Existing
  history from the previous separate monitor log is imported rather than
  discarded. Log records help with diagnosis but do not, by themselves,
  establish why a process exited.

### Changed

- **Clearer Preferences dialog**
  ([PR #16](https://github.com/username227/Viper4Linux-RE/pull/16)):
  Startup, window/tray behavior, GUI-exit/backend behavior and Audio Output
  memory now have distinct, labeled sections with theme-accented icons.
  The dialog can be resized and remembers its size even when dismissed with
  **Cancel**. On smaller displays, its contents scroll while **Restore
  Defaults**, **Cancel** and **OK** remain accessible. **Restore Defaults**
  changes the pending preferences only until **OK** is pressed and does not
  delete named effect presets or saved Audio Output snapshots.

- **More useful playback status**
  ([PR #13](https://github.com/username227/Viper4Linux-RE/pull/13)):
  The GUI now distinguishes detected incoming audio from silent processing
  callbacks. Pausing playback changes the footer from **Processing audio**
  to **Connected — no audio signal (DSP ready)** after a short grace period,
  reducing misleading activity indications and flicker during quiet passages.
  The separate idle/offline states remain. Signal detection is not a
  measurement of audible output.

- **Tidier footer layout**
  ([PR #8](https://github.com/username227/Viper4Linux-RE/pull/8)):
  **Restart Backend** sits to the left of **Presets**, with a subtle divider
  above the control row across all six effect tabs. Existing window sizing
  behavior is preserved.

### Fixed

- **Restore older Audio Output memories after the DSP control update**
  ([PR #15](https://github.com/username227/Viper4Linux-RE/pull/15)):
  Per-device settings saved using control version 26 can be restored by the
  version-27 backend. The new audio-activity counter does not alter stored
  DSP settings. Unknown versions and malformed snapshots remain rejected.

- **Cinnamon/X11 startup and repeat-launch busy cursor**
  ([PR #6](https://github.com/username227/Viper4Linux-RE/pull/6)):
  Starting directly into the system tray or opening ViPER again to activate
  an existing GUI no longer leaves Cinnamon showing an unnecessary busy
  cursor. Desktop identity and activation-token handling were also improved
  on native Wayland, but this does **not** resolve its separate menu, popup
  and window-decoration compatibility problems.

## v1.1.1 — previous release

Earlier development history and technical notes are preserved in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and Git history.
