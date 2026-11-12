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

The upstream ViPERDSP processor is a Git submodule from
`https://github.com/likelikeslike/ViPERDSP.git`, now pinned on the validation
branch to commit `f515736383586c6cc62990a517b8d1b809b92d03` (merged
upstream Pure Bass+ stereo fix, PR #7). CMake verifies the recorded revision
in a Git checkout and creates a private patched build copy. Already published
release archives retain the DSP revision bundled when they were created.

## CMake-first source checkout (modernization branch)

A normal source build no longer requires an installer script to prepare DSP
patches or routing configuration:

```sh
git submodule update --init --checkout third_party/ViPERDSP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target viper_source_check
```

CMake verifies the recorded upstream ViPERDSP revision and applies all five
downstream patches to a **private build-tree copy**, without modifying the Git
submodule. Unless `VIPER_ENABLE_SYSTEM_INSTALL=ON`, the source-build option
`VIPER_SOURCE_PREPARE_ROUTING=ON` automatically copies the stock PipeWire
filter-chain template and project fragment into
`build/routing-config/` at configure time. The default template is
`/usr/share/pipewire/filter-chain.conf`, overridable by
`-DVIPER_SOURCE_ROUTING_TEMPLATE=/absolute/template`. A package build
does not need the developer's stock filter-chain file, because it sets
`VIPER_ENABLE_SYSTEM_INSTALL=ON` and defaults routing preparation to OFF.

The source launcher expects the conventional **`build/`** directory. Manual
source builds, including full developer CTest builds, use `-B build` so that
`./viper4linux` and `./viper4linux --preflight` work without build-directory
overrides, symlinks or additional environment variables. Package build trees
are independently configured and installed via system paths.
When a developer already has a custom
`routing-config/filter-chain.conf` in the checkout, the launcher and
backend keep using that file; otherwise they use CMake's generated files
under `build/routing-config/`. The `viper_source_check` build target
verifies the compiled binaries, module and effective routing configuration.
It does **not** start PipeWire, the backend or the GUI.

### Optional private command shortcuts

If you explicitly want `~/.local/bin/viper4linux` and `v4l-cli`
to point into the source checkout, enable an **additional developer option**:

```sh
cmake -S . -B build -DVIPER_ENABLE_SOURCE_LINK_TARGET=ON
cmake --build build --target viper_source_links
```

The shortcuts are never created by configure or the normal build.
`viper_source_links` runs only when explicitly requested. It verifies the
source build, requires a private home subdirectory, checks both names for
unrelated files/symlinks before creating either shortcut, and preserves
existing matching shortcuts. An alternative user-private directory can be
set using `-DVIPER_SOURCE_LINK_DIRECTORY=/absolute/path/under/your/home`.

**Do not invoke `viper_source_links` or the source launcher on a workstation
with a working installed ViPER session unless you intend to switch command
resolution or perform the separately planned live-audio test.**
Neither operation is part of native package building.

These behaviors are covered by
`python3 tests/check-cmake-source-checkout.py` and the launcher regression
suite. The separate `install.sh` and `scripts/setup-source-checkout.sh` source
setup scripts have been retired from this development branch. CMake and its
developer targets now own their functionality. This is not proof of live
PipeWire/audio acceptance, which must still be checked before merging.

## v1.1.1 development history — archived notes

The following notes were written before the v1.1.1 release tag and describe
changes already present in that release baseline. They are retained here for
engineering context, **not** as new post-release changes. For user-facing
changes merged since v1.1.1, see the [changelog](../CHANGELOG.md).
Source-code modularization itself is not a new DSP feature.

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
- **Explicit GUI exit and audio backend choice (new behavior):** when quitting
  the GUI from the close button (if configured to quit), application menu, or
  system tray, users can keep the ViPER audio backend running, stop only the
  ViPER backend, or cancel. A **Remember this choice** checkbox suppresses
  future prompts; Preferences lets users switch between **Ask every time**,
  **Keep audio backend running**, and **Stop audio backend**. Hiding or
  minimizing the GUI does not prompt or stop processing, and the system
  PipeWire service is never stopped. This behavior was confirmed by the
  maintainer using a package build of the feature branch before PR #4 merged.
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
| `patches/fix_harmonic_abs.py` | Uses floating-point `std::abs` for Psychoacoustic Bass harmonic coefficients. | Corrects Linux fractional-coefficient truncation and startup behavior. |
| `patches/fix_fet_abs.py` | Uses floating-point `std::abs` for the FET compressor sidechain. | Restores compression on fractional-level samples. |

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
LADSPA headers and pinned upstream ViPERDSP. For a direct source build,
initialize the submodule, run `cmake -S . -B build`,
`cmake --build build` and
`cmake --build build --target viper_source_check`. The last target checks
the native LADSPA symbol and source routing without launching audio.
Before a live source launch, run `./viper4linux --preflight` from the
checkout. This **read-only** command checks the source build and current
ViPER process marker and reports whether an actual launch would start a new
backend, reuse a matching backend, or stop/replace an existing backend.
It does not create process locks, start the GUI, change presets, or stop audio.
The result is a snapshot; the real launcher rechecks state under a lock.
When the output says **REPLACE**, a subsequent normal launch may briefly
interrupt the current ViPER audio session. Check that your existing output
memory and settings are preserved before proceeding.

Run `./viper4linux` only when intentionally testing the source backend.
For complete DSP mock and regression tests, use CTest as the supported
runner. The former `tests/check-developer.sh` aggregate wrapper was retired
after the integrated 46/46 test run. For a system package,
see [Arch packaging](ARCH-PACKAGING.md).

The standard CMake `include(CTest)` integration enables `BUILD_TESTING=ON`
by default, registering 12 fast, offline package checks without extra flags.
Routine stable Arch `makepkg check()` runs `ctest --output-on-failure
--no-tests=error` plus LADSPA export verification.
The native developer mock suite remains opt-in and is not compiled by normal
package builds. To disable all CTest registration, configure with
`-DBUILD_TESTING=OFF`. The separately maintained `-git` AUR recipe needs
an independent update before its regular builds use the same CTest interface.

For the complete native DSP, GUI, preset, snapshot and release-contract
regressions, **CTest is the standard developer test runner**. Configure the
normal `build/` directory with developer tests enabled, compile its native/mock
test binaries, then run CTest:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVIPER_BUILD_DEVELOPER_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./viper4linux --preflight
```

CTest registers the independent Python contract checks and native mock checks
with individual numbered results, elapsed time and a final summary instead of
hiding all checks inside four aggregate cases. Python `unittest` modules
(including `test_output_*.py`) have separate CTest entries; each module's
individual assertions remain visible through verbose output.

Useful options:

```bash
ctest --test-dir build -N                 # list registered checks without running
ctest --test-dir build -L package --output-on-failure
ctest --test-dir build -L mock --output-on-failure
ctest --test-dir build -R backend --output-on-failure
ctest --test-dir build -V                 # full output, including passing checks
```

The legacy all-in-one developer runner has been removed; the full suite now
runs through individually registered CTest cases. `tests/check-package.sh`
remains a fast shell-based package gate for existing callers, and
`tests/check-mock.sh` remains required by the individual CTest mock cases.
No actual test assertions were removed. CTest runs the same mock
harness with isolated temporary preferences and IPC, not the user's live
PipeWire or installed backend.
Release-contract checks are registered only when the corresponding GitHub
workflow file is present (it may be absent from unified source archives).

Release candidate CI runs the full developer CTest suite *in addition to*
normal Arch package checks, and native Debian builds run the full CTest suite
before package staging. A passing package smoke check does not certify a DSP
change; run the developer suite before merging native DSP or snapshot changes.
Rebuilding an AUR package should not force every user to run expensive,
developer-only checks.


A full live acceptance test must cover Qt rendering, real PipeWire audio,
Bluetooth reconnection, sample-rate changes, IRS/VDC loading, presets, tray
behavior, and the backend fingerprint on a GUI-only upgrade. Cinnamon/X11
tray and minimize-to-tray behavior have been tested; KDE still requires live
testing. The backend should not restart for GUI-only changes.

### Backend lifecycle diagnostics

The launcher appends to `$XDG_STATE_HOME/viper4linux/backend.log` (by default
`~/.local/state/viper4linux/backend.log`). New backend stdout/stderr messages
have local ISO-style timestamps including the UTC offset and the backend PID.
This applies to both source and installed startup scripts. Launcher events
identify start/ready, explicit restart or build replacement, and immediate
startup failure. A final `output stream closed` line reports when the backend
stdout/stderr pipe closes; by itself it does **not** establish the exit code,
signal, or root cause. Correlate it with the launcher entries and the system
journal before attributing a crash.

The Audio Output monitor now timestamps its diagnostics, including
initialization and whether its tracked backend PID disappears. The monitor
can notice backend disappearance while running, but cannot report the
terminated process's exit code. Log timestamps reflect when messages reach
the reader; they do not prove when a buffered upstream message was generated.
Run `python3 tests/test_backend_lifecycle_logging.py` for a non-audio smoke
test of both launcher logging streams and the monitor/launcher message format.

### Log retention

`src/viper_log_retention.py` implements user-selectable age retention for the
combined session log (default seven days, zero means keep indefinitely). The
application menu offers **Log Retention…** adjacent to **View ViPER Log**.
The setting is saved in `$XDG_CONFIG_HOME/viper4linux/log-retention.json`.
The backend log reader and Audio Output monitor write their differently labeled
messages into a single `backend.log` through short-lived file opens guarded by
the same stable `backend.log.lock` sidecar. Pruning uses atomic replacement
under the lock; it does not truncate or rename a descriptor still held for
writes. The monitor checks periodically even with no diagnostics. After the
former output monitor exits, the launcher imports the legacy
`output-monitor.log` once, marking the archived block for clarity. Only the
combined log receives new messages. A fallback pipe drain prevents a
failed log helper from closing the backend/monitor diagnostic pipe; it may lose
diagnostics in that failure case, but should not terminate audio or monitoring.
Normal hourly maintenance avoids copying logs that contain no expired entries.
Legacy entries without timestamps use the first later timestamp as a
conservative fallback until that date expires. Install both logger scripts and
restart the ViPER backend after upgrading from the earlier direct-file logger.


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
