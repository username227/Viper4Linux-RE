# Developer guide and source audit

## Architecture and source layout

- `src/viper-gui.cpp`: Qt6 Widgets GUI, settings, presets, IRS/VDC libraries and tray behavior.
- `src/viper-ctl.cpp`: CLI commands for a subset of live controls.
- `src/viper-control.h`: shared-memory IPC contract used by GUI, backend and CLI.
- `src/viper-ladspa.cpp`: PipeWire filter-chain LADSPA bridge to ViPERDSP, nonblocking audio-thread handoff.
- `src/viper_launcher.py`: version-aware backend start/reuse/replacement logic.
- `src/viper-ir-wave.h`, `src/viper-ddc.h`, `src/viper-eq-presets.h`: file/preset readers and support.
- `resources/`, `routing-config/`, `packaging/arch/`: icons, PipeWire configuration, local Arch packaging.
- `tests/`: isolated DSP mock, parsing, GUI contract and launcher tests.

The full upstream processor source is **not embedded** in the main application
repository. `install.sh` and the local Arch PKGBUILD retrieve pinned ViPERDSP
commit `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`.

## Downstream patches to pinned ViPERDSP

| Patch | Purpose | DSP semantics |
|---|---|---|
| `patches/fix_lufs_speed.py` | Corrects an upstream `SetSpeed` control-path bug. | Can change the requested LUFS response speed. |
| `patches/preallocate_stereo_imager.py` | Allocates native scratch buffers before real-time processing. | Stereo width formula remains native. |
| `patches/preallocate_multiband_compressor.py` | Allocates compressor scratch buffers before real-time processing. | Compressor mathematics remain native. |

Neither the patches nor the mock suite certify the full processor as hard
real-time-safe. The application also manages temporary dry/wet handoffs during
updates; that is integration logic, not a replacement for native effects.

## Effect/CLI coverage audit

All **24 upstream top-level processing groups** have GUI and native adapter
paths. This is not complete parity for all upstream individual parameters:
advanced FET/Multiband compressor controls, Speaker Correction internal filter
settings, and Bass anti-pop are not independently configurable in the GUI.
A future Advanced dialog can expose these without expanding the main tabs.

The CLI is more limited than the GUI: it can toggle the effect groups and
configure selected parameters of Bass, EQ, Clarity, Spectrum, surround, limiter,
AGC/LUFS, Stereo Imager, FET and Multiband compressors, Tube, AnalogX and
Crossfeed. Individual Dynamic EQ bands, IRS/VDC file selection, complete
named effect presets, window/tray settings and themes remain GUI-managed.
`viper4linux-output` separately handles remembered physical outputs and
assignment of full named effect presets to them.

Some CLI parameters have finer precision than their GUI sliders. The CLI
persists the exact native units under the `precision/` settings group while
retaining legacy slider-compatible settings for older installations. Backend
initialization applies the precision group last. Unrelated GUI edits preserve
the exact native values; moving the corresponding slider quantizes only that
setting. Complete preset loads and Reset Effects to Defaults replace the
previous precision overrides. The per-output snapshot restore also refreshes
the precision keys to prevent values from leaking between devices.

## Build and test workflow

Development on Arch Linux requires C++17, CMake, Qt6 Widgets, PipeWire,
LADSPA headers and upstream ViPERDSP. For a direct-source build from a clean
checkout, run `./install.sh` (which runs the DSP mock tests and checks the LADSPA
export), then `./viper4linux`. For a system build, use
[Arch packaging](ARCH-PACKAGING.md).

Routine Arch package builds run a deliberately small, deterministic smoke suite
after compiling the application. It checks launcher process handling, offline
package staging and physical Audio Output memory/identity logic, and verifies
the LADSPA entry point. It does not start PipeWire or compile the isolated
mock DSP again.

For the complete native DSP, GUI, preset, snapshot and release-contract
regressions, run the developer suite explicitly when changing those systems:

```bash
bash tests/check-developer.sh
```

The developer script includes the package smoke checks and isolated mock tests,
but excludes repeated static checks already run by the mock script. Release
candidate CI runs the full suite *in addition to* normal Arch package checks.
A passing package smoke check does not certify a DSP change; run the developer
suite before merging native DSP or snapshot changes. Rebuilding an AUR package
should not force every user to run those expensive, more fragile tests.


A full live acceptance test must cover Qt rendering, real PipeWire audio,
Bluetooth reconnection, sample-rate changes, IRS/VDC loading, presets, tray
behavior, and the backend fingerprint on a GUI-only upgrade. Cinnamon/X11
tray and minimize-to-tray behavior have been tested; KDE still requires live
testing. The backend should not restart for GUI-only changes.

## Cleanup decisions

Historical layout notes, one-off milestone release notes and repeated
implementation status documents have been consolidated into the maintained
guides. Original versions remain in Git history. No commented-out bulk DSP
implementation was found in the current `src/` scan; obsolete setting-key
handling and legacy preset migration are **active compatibility code** and
should not be removed without migration tests. One unused teal SVG resource
was deleted; the PNG variants and blue SVG are used by Qt/Arch packaging.

The local PKGBUILD is a **development** recipe, not yet the published AUR
package. The user will review/clean that file separately; documentation
cleanup should not be used to redesign it. Remaining publication work:
redistribution/license review, stable tarball build rules, public Git VCS
source, build paths containing spaces, and real KDE tray testing.

The project's history was tracked from a later development snapshot rather
than from its earliest non-Git prototypes. Previous experiments and notes can
be inspected through the repository's earlier commits and removed Markdown
files, rather than being shipped as current documentation.
