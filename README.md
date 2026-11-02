# ViPER for Linux (reverse-engineered DSP edition)

A Qt6 desktop interface and PipeWire filter-chain backend built around the upstream
[ViPERDSP](https://github.com/likelikeslike/ViPERDSP) C++ engine. The application
provides 24 native processing effects in six tabs: **Output**, **Bass**,
**EQ & Tone**, **Spatial**, **Dynamics**, and **Sound Correction**.

This is a development build, **not yet a published stable release**. The Arch
package is currently called `viper4linux-re-git`; the installed launcher remains
`viper4linux`. The project uses the upstream ViPERDSP commit
`6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`, with three documented local
patches. Redistribution rights must be resolved before public packaging.

## Quick start

On Arch Linux, see [Arch packaging](docs/ARCH-PACKAGING.md) for the current
**local-development** PKGBUILD. Build it from a separate directory rather than
inside the source tree. To compile a checkout directly (not as a system package):

```bash
sudo pacman -S --needed base-devel cmake git pkgconf ladspa qt6-base pipewire
./install.sh
./viper4linux
```

`install.sh` fetches ViPERDSP and needs an initially clean build directory.
When using a system package, launch `viper4linux` from the application menu
or terminal; use `viper4linux-ctl status` to inspect the running backend.

**Start with a low listening volume.** Some effects and makeup-gain settings
can substantially increase signal level. The output limiter does not guarantee
a safe listening level.

## Documentation

- [User guide](docs/USER-GUIDE.md) — interface, launcher, tray, themes, and troubleshooting
- [Effects reference](docs/EFFECTS.md) — every native effect, limitations, and listening cautions
- [Presets and libraries](docs/PRESETS-AND-LIBRARIES.md) — named presets, defaults, EQ curves, IRS and VDC files
- [Arch packaging](docs/ARCH-PACKAGING.md) — local builds and future release/package naming
- [Development and audit](docs/DEVELOPMENT.md) — architecture, build patches, tests, and unresolved items
- `man viper4linux` and `man viper4linux-ctl` — installed launcher and command-line reference

All guides are ordinary Markdown suitable for future adaptation to a GitHub Wiki.
Older milestone notes were removed from the current tree and remain recoverable
from the Git history.

## Important distinctions

The GUI automatically saves active effect settings. **Named presets** are
separate, manually saved snapshots under `~/.local/share/viper4linux/presets/`.
The `IRS/` and `VDC/` libraries under the same data directory store external
impulse responses and correction profiles. The GUI also controls theme,
window/tray behavior and login autostart; the CLI currently exposes only a
subset of live DSP controls, not preset or UI management.

A compatible running PipeWire backend is reused on GUI-only launches. Exiting
the GUI does not stop the audio backend. JamesDSP routing conflicts have been
observed; simultaneous reliable operation is not yet established. KDE Plasma
tray behavior still needs a native live test; Cinnamon/X11 has been tested.
