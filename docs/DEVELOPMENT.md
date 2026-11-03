# Developer guide and source audit

## Architecture and source layout

- `src/viper-gui.cpp`: Qt6 `ViperWindow` entry point, central state and
  composition of extracted presentation/behavior modules.
- `src/panels/`: six tab-specific panel builders (Output, Bass, EQ & Tone,
  Spatial, Dynamics and Sound Correction).
- `src/widgets/`: reusable effect widgets, compact EQ previews, Graphic/Dynamic
  EQ popup editors and IRS selector.
- `src/window/`: seven ViperWindow-owned implementation sections for
  presets, layout, correction, shell/tray, Graphic EQ, dynamics and effects.
- `src/sync/`: settings/backend bridges and precision/reconnect policies.
- `src/viper-theme.h`, `src/viper-named-presets.h`,
  `src/viper-preset-dialog.h`, `src/gui-single-instance.h`: extracted
  theme/preset utilities and a new single-GUI owner guard.
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

## Unreleased change log — refactor branch versus `main`

**Tracking only; not a release announcement.** This list records the small
observable changes in `refactor/gui-modularization` compared with the current
`main` baseline. Move the relevant entries into release notes only when the
maintainer approves an actual release. The large structural split is not a
new DSP-feature release.

### User-facing improvements and fixes

- **One GUI per user session (new behavior):** launching `viper4linux` or
  `viper-gui` again activates the running GUI, including when hidden in the
  system tray, instead of opening a second instance with competing settings.
  The second process exits; backend and Audio Output monitor lifetimes remain
  separate. Stale-lock/crash recovery is covered by the integration test.
- **Graphic EQ comparison and editor fixes:** the compact overview now has
  fixed +/-12 dB vertical axes, a centered 0 dB reference and a consistent
  logarithmic 20 Hz-20 kHz horizontal mapping, so gain curves remain comparable
  across presets/layouts. When switching among 10/15/25/31 bands, the open
  fader editor immediately shows or hides the appropriate columns; the
  first-open visibility issue was also corrected. These affect rendering
  and editor state, not the native EQ DSP.
- **Dynamic EQ editor explanation (UI copy):** identifies each band's
  Maximum gain as an individual setting, while the graph approximates the
  *combined* response. Explains frequency, Q, shelf-filter interaction and
  other bands, and states explicitly that this is not live audio metering.
  No DSP or Dynamic EQ graph algorithm change.
- **Source-checkout launcher fix:** after developer scripts moved into
  `scripts/`, the launcher once again invokes `scripts/start.sh`
  correctly. This is a regression fix, not a new audio feature.

### Build, packaging and engineering changes

- CMake About/build version is derived from the nearest reachable
  `vMAJOR.MINOR.PATCH` Git tag plus revision in checkouts; unified source
  archives carry `RELEASE-VERSION` and `RELEASE-REVISION`. Release workflow
  input drives the archive and generated stable PKGBUILD without manually
  changing CMake. Automated tests cover the version identities.
- GUI implementation was reorganized into the six panels, five widget files,
  seven window implementation files, three synchronization files, and four
  standalone helper headers listed below. This is predominantly a
  behavior-preserving extraction; no newly implemented audio effects, DSP
  algorithms, shared IPC layout or ViPERDSP submodule update.
- Developer tests now cover module composition, GUI/backend synchronization,
  Graphic EQ behavior, a two-process Qt single-instance scenario, and
  version metadata. Stable release candidate `1.1.1` passed all four GitHub
  Actions jobs (Arch build/full developer suite, Ubuntu/Mint .deb,
  Debian/LMDE .deb, complete asset assembly) on
  [run 41719470272](https://github.com/username227/Viper4Linux-RE/actions/runs/41719470272).
  This certifies CI execution, not every possible live device combination.

### GUI source-size snapshot — 2026-11-03

Measured as **actual Git blob sizes in bytes** at `main`
(`2927a750`) and the refactor tip (`f93d2e5`). The refactored GUI is
still a single Qt/C++ translation unit: extracted `.inc` modules are
included in `ViperWindow` rather than separately compiled. These values
are a historical checkpoint, not sizes that should be maintained manually.

- `src/viper-gui.cpp` before: **285,388 bytes; 5,082 lines**.
- `src/viper-gui.cpp` after: **65,679 bytes; 1,219 lines**.
  The original file itself shrank by **77.0%**.
- The 25 newly extracted GUI source/header files total **256,545 bytes**;
  combined with the main file, the refactored set is **322,224 bytes**
  versus the original 285,388-byte monolith. The combined total is *larger*
  because there are now independent helpers, interfaces, one-GUI logic and
  documentation/comments within code; it is not a 77% reduction in total
  project source size.

| Group | Files | Bytes |
|---|---:|---:|
| Main GUI source | 1 | 65,679 |
| Six effect-panel headers | 6 | 37,843 |
| Five widget headers | 5 | 38,807 |
| Seven window implementation files | 7 | 103,605 |
| Three synchronization files | 3 | 41,242 |
| Four other helper headers | 4 | 35,048 |
| **Total refactored GUI source set** | **26** | **322,224** |

Individual extracted-file sizes for future source-audit comparisons:

| File | Bytes |
|---|---:|
| `src/panels/bass-panels.h` | 4,582 |
| `src/panels/correction-panels.h` | 7,186 |
| `src/panels/dynamics-panels.h` | 6,208 |
| `src/panels/eq-tone-panels.h` | 8,922 |
| `src/panels/output-panels.h` | 3,769 |
| `src/panels/spatial-panels.h` | 7,176 |
| `src/widgets/dynamic-eq-editor.h` | 7,494 |
| `src/widgets/effect-widgets.h` | 11,647 |
| `src/widgets/eq-overviews.h` | 14,098 |
| `src/widgets/graphic-eq-editor.h` | 4,142 |
| `src/widgets/irs-combo-box.h` | 1,426 |
| `src/window/window-correction.inc` | 6,929 |
| `src/window/window-dynamics.inc` | 19,626 |
| `src/window/window-effect-actions.inc` | 12,383 |
| `src/window/window-graphic-eq.inc` | 9,729 |
| `src/window/window-layout.inc` | 16,260 |
| `src/window/window-presets.inc` | 17,417 |
| `src/window/window-shell.inc` | 21,261 |
| `src/sync/gui-backend-bridge.inc` | 32,907 |
| `src/sync/gui-control-channel.h` | 706 |
| `src/sync/gui-settings-bridge.inc` | 7,629 |
| `src/gui-single-instance.h` | 3,309 |
| `src/viper-named-presets.h` | 4,844 |
| `src/viper-preset-dialog.h` | 14,849 |
| `src/viper-theme.h` | 12,046 |

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

## Post-merge compatibility checks (not GUI refactor blockers)

The GUI modularization work was accepted and merged in PR #1 on
2026-11-03. The 1.1.1 release-candidate workflow passed the full
developer regression suite, the Arch native build and both Debian-family
package builds. The following are **optional compatibility follow-ups**,
not unfinished extraction steps and not grounds to retain the completed
GUI modularization plan:

- **DDC unsupported-rate live case:** test an unsupported sample-rate
  or missing-rate VDC profile in the running PipeWire application, and
  confirm processing is safely rejected without changing audio unexpectedly.
  Parser-level invalid/missing-rate tests already pass; a valid 44.1/48 kHz
  profile was audibly tested with on/off/on switching.
- **KDE tray/window behavior:** test the single-instance GUI activation,
  minimize-to-tray and restore sequence in a KDE session. Cinnamon/X11
  tests passed, including activation when already hidden.
- **Legacy named-preset migration:** test importing/loading settings
  written by an older preset format on a disposable configuration, while
  preserving user files. Current create/load/edit/reset and related
  contract tests passed; historical migration was not separately
  exercised in live acceptance.

Track any outcomes here or in a targeted compatibility issue. Do not
reopen the completed GUI extraction plan merely for these items.

## Cleanup decisions

Historical layout notes, one-off milestone release notes and repeated
implementation status documents have been consolidated into the maintained
guides. Original versions remain in Git history. No commented-out bulk DSP
implementation was found in the current `src/` scan; obsolete setting-key
handling and legacy preset migration are **active compatibility code** and
should not be removed without migration tests. One unused teal SVG resource
was deleted; the PNG variants and blue SVG are used by Qt/Arch packaging.

The repository's local PKGBUILD is a **development** recipe, separate from
the already published AUR `viper4linux-re-git` package. Keep AUR metadata
and release assets distinct; documentation cleanup should not redesign
either recipe. Remaining *stable-release* work includes maintainer review,
tag/release publication decisions, redistribution/license review and real
KDE tray testing. GitHub Actions has already verified the release-candidate
source archive and native package builds.

The project's history was tracked from a later development snapshot rather
than from its earliest non-Git prototypes. Previous experiments and notes can
be inspected through the repository's earlier commits and removed Markdown
files, rather than being shipped as current documentation.
