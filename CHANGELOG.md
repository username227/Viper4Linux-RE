# Changelog

This file summarizes the user-facing changes in each release. Internal build,
CI and documentation-only maintenance is generally excluded from these notes.

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
