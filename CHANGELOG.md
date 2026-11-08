# Changelog

This file summarizes the user-facing changes in each release. Internal build,
CI and documentation-only maintenance is generally excluded from these notes.

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
