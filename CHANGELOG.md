# Changelog

This file tracks user-facing changes merged into `main` after the most recent
stable release. **Unreleased** entries are release-note candidates, not a
release announcement or a commitment to a particular version number.
Record new entries here when their changes merge; move them into a versioned
section when a release is approved.

## Unreleased — since v1.1.1

### Fixed

- **Audio Output snapshot compatibility** ([PR #15](https://github.com/username227/Viper4Linux-RE/pull/15)):
  Saved per-device DSP settings from control version 26 can now be restored
  with version 27, whose new audio-activity counter does not change stored
  effect parameters. Unknown control versions and invalid settings remain
  rejected before modification.

- **Accurate audio-activity status** ([PR #13](https://github.com/username227/Viper4Linux-RE/pull/13)):
  The GUI now distinguishes incoming audio from silent DSP callbacks. When a
  player such as Elisa is paused, the footer changes from green **Processing
  audio** to blue **Connected — no audio signal (DSP ready)**, with separate
  colors for light and dark themes. A short grace period avoids flicker during
  musical rests. The existing DSP idle/offline states, effects and Audio Output
  memory remain unchanged. Signal detection is not proof of audible output.

- **Desktop startup and window activation** ([PR #6](https://github.com/username227/Viper4Linux-RE/pull/6)):
  Fixed Cinnamon/X11's lingering busy cursor on repeated GUI launches and
  start-hidden-in-tray launches. Improved desktop identity and activation-token
  forwarding on native Wayland. This does not resolve the separate native
  Cinnamon Wayland menu, popup, or decoration issues.

### Added

- **Age-based diagnostic log retention and unified log** ([PR #14](https://github.com/username227/Viper4Linux-RE/pull/14)):
  The application menu now groups **View ViPER Log** with **Log Retention…**.
  Logs are kept for seven days by default, or for a user-selected number of
  days (0 means unlimited retention). Backend, launcher and Audio Output
  monitor messages now share one timestamped log with distinct source/PID
  labels for easier troubleshooting; older monitor history is imported.
  The backend keeps running during cleanup.

- **Timestamped backend diagnostics** ([PR #12](https://github.com/username227/Viper4Linux-RE/pull/12)):
  New backend and Audio Output monitor log messages include local timestamps
  with timezone offsets and process identifiers. The launcher also records
  backend start, stop and readiness transitions, making intermittent audio
  interruptions easier to investigate. Existing historical log entries are
  preserved.

- **Restart Backend** ([PR #7](https://github.com/username227/Viper4Linux-RE/pull/7)):
  Added a footer button and application-menu action to restart the ViPER audio
  backend and its Audio Output monitor while keeping the GUI open. The restart
  verifies a fresh monitor heartbeat and reconnects the interface; it does
  **not** restart the system PipeWire daemon. The existing launcher also
  supports the command `viper4linux --restart-audio`. The compact footer
  layout ([PR #8](https://github.com/username227/Viper4Linux-RE/pull/8))
  positions Restart Backend to the left of Presets and adds a subtle divider
  above the buttons that stays anchored across all six tabs. The original
  window width and resizing behavior are preserved.

## v1.1.1 — release baseline

The `v1.1.1` tag is the baseline for the **Unreleased** section above.
Earlier development history and implementation notes are preserved in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and Git history.
