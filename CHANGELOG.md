# Changelog

This file tracks user-facing changes merged into `main` after the most recent
stable release. **Unreleased** entries are release-note candidates, not a
release announcement or a commitment to a particular version number.
Record new entries here when their changes merge; move them into a versioned
section when a release is approved.

## Unreleased — since v1.1.1

### Fixed

- **Desktop startup and window activation** ([PR #6](https://github.com/username227/Viper4Linux-RE/pull/6)):
  Fixed Cinnamon/X11's lingering busy cursor on repeated GUI launches and
  start-hidden-in-tray launches. Improved desktop identity and activation-token
  forwarding on native Wayland. This does not resolve the separate native
  Cinnamon Wayland menu, popup, or decoration issues.

### Added

- **Restart Backend** ([PR #7](https://github.com/username227/Viper4Linux-RE/pull/7)):
  Added a footer button and application-menu action to restart the ViPER audio
  backend and its Audio Output monitor while keeping the GUI open. The restart
  verifies a fresh monitor heartbeat and reconnects the interface; it does
  **not** restart the system PipeWire daemon. The existing launcher also
  supports the command `viper4linux --restart-audio`.

## v1.1.1 — release baseline

The `v1.1.1` tag is the baseline for the **Unreleased** section above.
Earlier development history and implementation notes are preserved in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and Git history.
