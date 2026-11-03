# GUI Modularization and Repository Cleanup Plan

**Project:** ViPER4Linux-RE  
**Working branch:** `refactor/gui-modularization`  
**Base:** `main` at the time the branch was created (2026-11-02).  
**Status:** In progress; **do not merge into `main` until the entire GUI separation is complete, tested, and explicitly approved by the maintainer.**

This document is the durable handoff for work across conversations and for contributors inspecting the project. It records the agreed process, architectural intentions, testing gates and constraints. It is not a promise that any unimplemented stage has been completed.

## Why

`src/viper-gui.cpp` is approximately 285 KB and contains window creation, custom widgets, themes, effect controls, EQ editors, presets, settings persistence and DSP/CLI synchronization. These are valid features but too many responsibilities for one translation unit and one GUI class. The goal is improved readability, testability and safe future maintenance **without changing behavior**.

A smaller second area is `src/viper-ctl.cpp` (~30 KB, already partially separated into `viper-cli-*.h`); the real-time LADSPA bridge (`src/viper-ladspa.cpp`, ~85 KB, ~1,493 lines) is considerably more sensitive and **is not part of this GUI refactoring effort**.

## Non-negotiable rules

1. **All refactoring work stays on `refactor/gui-modularization`** until *every* planned stage is finished and accepted. No partial merges, cherry-picks or releases to `main`.
2. **One small extraction per checkpoint.** Rebuild the program after each change, run relevant automated tests, and investigate regressions before making the next extraction. Commit verified checkpoints separately on this branch to preserve reversibility.
3. **No intentional feature changes, defaults changes, parameter rescaling, renamed user settings, DSP changes, or incidental bug fixes.** Record discovered defects as separate follow-ups instead of folding them into code movement.
4. Preserve all six tabs, effect-switch/expand behavior, EQ editor interactions, themes, tray behaviors, named presets, IRS/VDC selection, GUI↔backend/CLI synchronization, precise CLI values, output-device memory, and audio routing.
5. Preserve existing `QSettings` keys, native preset formats, shared-memory ABI and backend binary behavior.
6. Changes that require native audio/IPC modification are outside this scope and need their own review and more stringent testing.
7. The normal AUR `-git` package tracks `main`; **do not redirect AUR or publish stable releases from this branch**.
8. Development/full regression tests should be run on demand at checkpoints, not silently added to the normal `makepkg`/AUR build path.
9. Never treat a source-level/static test as equivalent to a successful Qt compilation, actual DSP processing, or on-device GUI acceptance test.

## Checkpoint 0 — Root shell-script organization (first change)

**Keep at root:** `install.sh` (documented source-build entry point) and `viper4linux` (public application launcher).

**Move under existing `scripts/`:** `start.sh`, `stop.sh`, `status.sh`, `gui.sh`, `verify.sh`. Preserve executable modes. Scripts must derive the repository root from their own location rather than the calling working directory. Update references in the scripts, installer, and user guide; leave installation/package entry points unchanged.

Verification: `bash -n` on all moved and modified scripts; execute `./scripts/status.sh`, `./scripts/gui.sh`, and the verification script where a source build is available; rebuild to confirm the launcher and packaging assumptions. Note that `gui.sh` explicitly opens the GUI without starting the backend. `start.sh` runs a **foreground** filter-chain client; don't launch a second backend while testing. `stop.sh` must stop only this project's client.

Acceptance: root directory is simpler; documentation is accurate; no broken internal paths, file modes or package behavior.

### Checkpoint 0 verification record — 2026-11-03

**Result: ACCEPTED; proceed to Checkpoint 1.** The maintainer confirmed a successful native build of the refactor branch and provided output from `./scripts/status.sh` followed by `./scripts/verify.sh` on Arch Linux/Cinnamon.

- Relocated scripts resolved their repository-relative paths even though the checkout path contained spaces.
- The status script detected the running ViPER backend and printed native DSP control state.
- The verification script found the expected CMake/source and compiled binaries, confirmed the LADSPA plugin reference, and ran the mock regressions successfully.
- The mock results passed native effect dispatch, resampling/IR/DDC, EQ, compressors, CLI precision, native state capture, and settings tests.
- Native processing ran at 48 kHz with an advancing audio callback counter (836034 to 837119).
- PipeWire link inspection showed Rhythmbox feeding the ViPER filter and the filter feeding the ALSA speaker sink.
- Deliberate invalid-command and missing-IRS diagnostics appeared in the passing negative-test cases; they were not unexpected failures.

**Scope of acceptance:** relocating shell utilities and updating their paths. This does **not** certify the future GUI modularization stages, and the manual GUI checklist was not performed at this checkpoint. No work is approved for merge into `main`.

## Checkpoint 1 — Custom Qt widgets

Extract the self-contained widget classes from `src/viper-gui.cpp` (such as `SafeSlider`, `EffectGlyph`, `EffectSwitch` and other reusable visual widgets) into named widget source/header components. Preserve behavior, signals, event propagation, accessible names, painting, colors, and size policies exactly. Prefer proper headers/translation units over gratuitous header-only implementations; avoid changing Qt meta-object requirements.

Rebuild and test: slider mouse wheel only when focused, switch hit targets/toggling, all icon styles, effect panel collapse/expand and high-DPI/scaled rendering.

### Checkpoint 1 implementation — GUI smoke test accepted

The following classes were extracted **without changing their existing
methods, signals, drawing logic or event handlers**:

- `SafeSlider`, `EffectGlyph`, `EffectSwitch` → `src/widgets/effect-widgets.h`
- `IrsComboBox` → `src/widgets/irs-combo-box.h`

The `EqOverview` and `DynamicEqOverview` classes deliberately remain in
`viper-gui.cpp` until the dedicated EQ refactoring stage (Checkpoint 4);
their drawing and state interactions should be extracted together.

These are self-contained inline Qt widget classes, so no new CMake sources or
Qt meta-object processing are needed; `viper-gui.cpp` now includes the headers.
The DSP, IPC, native preset format, and build configuration are unchanged.

**Verification status — maintainer acceptance, 2026-11-03:** The maintainer
opened the refactored GUI and confirmed correct rendering, effect activation
switches, functioning dropdown menus and movable sliders. The extracted
widget implementations were source-compared against the originals before
committing, and no DSP/IPC logic was changed. This is **accepted as the
Checkpoint 1 GUI smoke test**; the complete developer suite and live audio
verification are deferred to the final integration gate.

**Not specifically confirmed in this smoke test:** mouse wheel focus behavior
and IRS-list stability during popup refresh. Preserve these as regression
checks when related controls are next modified. No changes may merge to `main`
until the entire refactor is complete and expressly approved.

## Checkpoint 2 — Appearance and theme handling

Isolate light/dark/classic themes, palette and widget styling, icon/theme selection and visual state updates. Keep user configuration keys and theme redraw behavior unchanged.

Rebuild and test: selected themes, system theme changes, tray/icon rendering, resize and restart persistence.

### Checkpoint 2 implementation — accepted after native GUI testing

- Extracted the complete existing palette and QSS rendering code from
  `ViperWindow::applyTheme()` into `src/viper-theme.h`,
  `viper_theme::apply()`. The main window retains theme selection, persistence,
  Cinnamon GTK-theme detection, Qt color-scheme signals, and its muted status
  text state; no stored keys or effect code changed.
- The new renderer is an inline function because the extracted implementation
  is self-contained and CMake does not need new source targets. The extraction
  passes existing dialog/widget pointers explicitly instead of depending on
  private main-window fields. No audio-backend modifications.
- **Maintainer acceptance, 2026-11-03:** The user confirmed that the rebuilt GUI
  works, including visual controls and theme switching. The initially missing
  dark-mode result was due to selecting the wrong control in Cinnamon's theme
  settings; after choosing the appropriate setting, ViPER4Linux switched to
  dark mode and retained correct theme colors. **Checkpoint 2 appearance
  extraction is accepted.**
- Remaining edge cases (EQ editor palettes, changes during an open session,
  and restart persistence where not explicitly checked) stay on the final
  integration checklist. Acceptance of this checkpoint is not a claim that
  the full regression suite has been run. No merge to `main`.

### Separate future issue — multiple simultaneous GUI instances

The application currently permits opening multiple GUI processes that control
the same DSP backend and may access the same settings. Potential concerns are
stale control displays, competing writes and lost preferences. This behavior
has not been established as defective and is **out of scope for this mechanical
GUI refactoring**: do not introduce a singleton guard or synchronization change
within Checkpoint 2. Investigate later with two windows and coordinated tests;
decide whether to permit multi-window synchronization or enforce one GUI only
as an independent feature/bugfix, with explicit maintainer approval.

## Checkpoint 3 — Native named-preset management

Isolate native `QSettings` named preset reading/writing, validation, migration, Save/Load/Rename/Delete, reset and audio-output assignment. Preserve original on-disk INI format, UUID filenames, path safety and atomic writing, loading semantics and every confirmation prompt.

Rebuild and test: complete preset round-trip, legacy migration, overwrite/rename/delete, Apply to Audio Output, Reset to Defaults, source selection, and independent output memory.

**Keep the separate Android-export experiment on its own branch** (`feature/viper4android-preset-export`). It is not part of the refactor; reconcile after the GUI restructuring is accepted. Avoid mixing that feature into a code-movement commit.

### Checkpoint 3A — Named-preset storage extraction (accepted)

Moved the existing named-preset library directory, list/sort, name validation,
INI read/write, one-slot migration, and loading-to-staging operations into
`src/viper-named-presets.h` as inline functions in `viper_presets`.
The GUI methods delegate to the extracted functions using their existing
`QSettings` object. The dialog, confirmations, save-to-staging capture,
restore-to-controls, CLI assignment, UUID selection and reset-to-defaults
remain in `ViperWindow` unchanged. Existing keys, `format/version=1`, and
error text are preserved. This is a **partial checkpoint**: the user must
verify the native Qt compilation and preset round-trip before proceeding to
further UI separation. Maintainer test passed on 2026-11-03; do not merge to `main`.

Quick test: rebuild `viper-gui`; save a test preset, load it back, rename it,
confirm overwrite and delete behavior, restart, and verify previously saved
presets still appear. If a legacy one-slot preset is available, test migration
once; do not create or delete existing user presets merely for testing.

**Verification update (2026-11-03):** Maintainer reports that all previously tested save/load/rename/delete functions still work after the extraction. Checkpoint 3A accepted. Legacy migration was not separately exercised, and full integration checks remain outstanding.

### Checkpoint 3B — Preset dialog extraction (accepted GUI smoke test)

The dialog, buttons, confirmation prompts, output assignment flow, list refresh
and reset action are moved to `src/viper-preset-dialog.h`. Main-window
stateful operations are invoked through synchronous callbacks. UI text,
identifiers, storage formats and status messages are unchanged.

**Maintainer verification, 2026-11-03:** The maintainer reports that all
preset-manager functionality still appears to work following the extraction.
Checkpoint 3B is accepted at the GUI smoke-test level. This report does not
independently establish that every branch of the Audio Output assignment,
legacy migration, reset confirmation, and restart persistence has been
exercised. Keep those cases in the final integration checklist.

Native build command: `cmake --build build --target viper-gui -j4`.
No merge to `main` before completion and explicit approval of the full GUI
refactor.

## Checkpoint 4 — Graphic EQ and Dynamic EQ UI

Extract EQ widget composition, graphical overview, popup/editor controls, selected-band state, preset curves and layout handling. Preserve 10/15/25/31-band layouts, exact gain values, disabled-band behavior, Dynamic EQ active-band counts, band selection and precision compatibility.

Rebuild and test: full EQ edit/save/reopen/load round-trips, mouse interactions, resizing and external CLI changes.

### Checkpoint 4A — Graphic/Dynamic EQ overview widgets (accepted smoke test)

Moved the two existing self-contained overview widgets, `EqOverview` and
`DynamicEqOverview`, together with their shared `DeqBand` structure into
`src/widgets/eq-overviews.h`. The widget bodies are byte-for-byte identical
to their previous implementations. No changes were made to cubic curve
smoothing, log-frequency coordinates, Q/gain/threshold units, visual marker
clustering, mouse selection behavior, or value storage. No editor or backend
state transitions were changed.

**Maintainer verification, 2026-11-03:** Both EQ graphical overviews work;
numbered Dynamic EQ markers can be clicked to select bands, and resizing works.
This accepts the Checkpoint 4A GUI smoke test. Tests of coincident-frequency
markers, complete saved-state round trips, and the full developer regression
suite remain pending integration verification.

**Verified rebuild and version provenance, 2026-11-03:** After an explicit
`cmake -S . -B build` reconfiguration and a successful
`cmake --build build --target viper-gui -j4`, the maintainer confirmed that
`git rev-parse --short=12 HEAD` and `build/generated/viper-build-info.h`
both contain `fd7d0c6d3cc8` (version `1.1.0-fd7d0c6d3cc8`).
The maintainer subsequently retested and reports the GUI/EQ remains functional.
This strengthens acceptance of Checkpoint 4A. The About-dialog display after
this rebuild was not separately reported; don't claim it was inspected.

**Separate build-system follow-up:** The earlier About display
`1.1.0-3742fe888058` was consistent with version information generated
at CMake configuration time. A normal incremental build does not necessarily
refresh that header when Git HEAD changes. Consider a reproducible automatic
revision refresh after the refactor, independently from GUI code movement.

The detailed editor layout and state integration remain for Checkpoint 4B.
No merge to `main` before the full refactor is accepted.

### Checkpoint 4B — Dynamic EQ detailed editor construction (accepted GUI smoke test)

Moved the existing Dynamic EQ editor widget and layout construction into
`src/widgets/dynamic-eq-editor.h`, returning its widget pointers to the main
window. Existing controls, ranges, labels, tooltip text, Close action and
dialog dimensions are unchanged. All data binding, band-selection callbacks,
table changes, Spread frequencies processing, persistence and DSP interaction
remain in `ViperWindow`.

Test with `cmake --build build --target viper-gui -j4`: open Dynamic EQ,
change active band count, click graph markers and table rows, edit Q, gain,
threshold and frequency, exercise Spread frequencies, close/reopen the dialog,
and resize it. Preserve the numerical values to their existing precision.
**Maintainer verification, 2026-11-03:** GUI reported the exact 4B commit
`1.1.0-cdcccdbf4646`, and the maintainer reported the editor and overall GUI
appeared to function correctly. This is the 4B GUI smoke-test acceptance;
comprehensive precision and backend regression tests remain for final review.
No merge to `main` without full refactor approval.

### Checkpoint 4C — Graphic EQ detailed editor construction (accepted)

**Targeted fix implemented and maintainer-retested:**
Reapply the 10/15/25/31-column visibility invariant **after** the detailed
editor's initial `show()`, as well as when loading a band-count layout.
Previously `loadEqLayout()` updated visibility before the hidden nonmodal
dialog was shown; the screenshot shows 31 displayed faders while the mode is
10, with obsolete levels in inactive faders. Only active band faders feed
the main overview, so moving an out-of-range fader creates apparent
graph/editor disagreement. This is the most direct explanation for the
reported symptoms, but is not validated until retested. No DSP or preset
logic was changed.

**Regression reported 2026-11-03 (not accepted):** During the clean-build test,
manual Graphic EQ fader changes did not update the overview curve in the main
window. Selecting another preset updated the overview, but the previously
manually altered fader positions remained displayed in the detailed editor.
The maintainer reports this as a regression; preserve it as a blocking issue,
not a successful Checkpoint 4C test.

**Initial source review:** The extraction kept the existing
`QSlider::valueChanged` connections to `saveEqLayout()`,
`selectEqPreset("custom")`, `updateEqOverview()`, and `push()`;
preset application still calls `setValue()` on the same stored fader pointers,
then refreshes the overview. The observed divergence does not yet have a
proven source-code cause. Check for simultaneous GUI instances and verify
whether the clicked editor belongs to the same live main window, then
reproduce with a single process before attempting a behavioral fix.
This was subsequently addressed by enforcing active-column visibility and
retested successfully; see the acceptance note below.


Extracted the existing retained, nonmodal 31-fader Graphic EQ editor layout
and its child controls into `src/widgets/graphic-eq-editor.h`. The factory
returns the same Qt widget pointers to the main window. Its dimensions,
scroll behavior, object names, fader minimum/maximum/step values (hundredths
of a dB), 31-column layout, Flat and Close controls remain unchanged.
Signal connections for fader value edits, precision persistence, EQ preset
selection, backend sync, and the Flat action remain in the main window.

**Clean build, explicitly requested by maintainer after apparent incremental
build stalls:**

```bash
git pull --ff-only
cmake -S . -B build-clean -DCMAKE_BUILD_TYPE=Release
cmake --build build-clean --parallel 4
```

This uses a fresh out-of-tree build directory without deleting the existing
`build/`. Execute `./build-clean/viper-gui` to test the newly compiled
GUI and verify About shows the current HEAD revision. Run the actual project
verification scripts (`./scripts/verify.sh` may target `build/`, so verify
their behavior or run against the clean build explicitly). In the Graphic EQ,
test 10/15/25/31 bands, fader edits and precision labels, Flat action, preset
selection/save/delete, horizontal scrolling, resize, and Close/reopen. Recheck
Dynamic EQ interactions. If the compiler appears stalled, inspect live
processes and memory while the build is active. This checkpoint has **not
been natively built or tested by the assistant**; maintain verification gate.

**Follow-up findings and fixes, 2026-11-03 (maintainer-tested):**
The maintainer confirmed the original 31-fader visibility problem was much
improved, but reported (1) the overview's automatic vertical rescaling
misrepresented relative gains across presets, and (2) switching band counts
while the editor remained open did not immediately show or hide columns.
The overview now uses fixed vertical +/-12 dB with a permanent 0 dB centerline
and a fixed logarithmic 20 Hz–20 kHz horizontal mapping (these are rendering
changes only; DSP values are untouched). The band-count selector callback
now explicitly calls `syncGraphicEqEditorColumns()` after preset/layout
application, which previously refreshed graph values but not active-column
visibility for preset selections. Check 10/15/25/31 modes *with editor open*,
graph normalization using Flat/Acoustic/Bass Booster, real-time fader→graph
updates, and preset→fader updates. Check that 0 dB remains centered regardless
of gain extrema. No native build or visual test has been performed by assistant.

**Maintainer confirmation, 2026-11-03:** Both fixes at commit
`a3c2e61bb2e4` were verified: fixed, comparable Graphic EQ graph scales
and immediate band appearance/disappearance while the editor remains open.
The Graphic EQ faders, overview, and presets appeared synchronized. Dynamic EQ
was also rechecked and appeared functional. The reported 4C GUI regression
is resolved as far as the maintainer's visual/interactivity smoke test goes.

**Final focused acceptance, 2026-11-03:** Maintainer confirmed all
three remaining tests operational: Graphic EQ custom preset save/switch/reload,
settings persistence across GUI quit/reopen, and audible response to EQ fader
changes. Alongside the accepted 4A overview widget tests, 4B Dynamic EQ
editor tests, and 4C Graphic EQ fixes, **Checkpoint 4 is accepted**.

The full developer regression suite, CLI/backend precision and
output-reconnect tests remain later integration gates. Unmodified effects
were not specifically retested for Checkpoint 4.

## Checkpoint 5 — Effect panels and layout construction

Move construction and hookup of the six tab panels into reasonably scoped components (potentially grouped by tab) without changing signal connections, visibility, disabled/default states, effect labels or settings values. Avoid a single giant 'panels' file.

Rebuild and test: all 24 effect groups, independent toggling versus collapse, all slider ranges/defaults, scrolling, tab order, IRS and DDC controls.

### Checkpoint 5A — Crossfeed panel extraction (maintainer GUI smoke test passed)

Moved only Crossfeed (ViPER Cure) control construction to
`src/panels/crossfeed-panel.h`, returning the exact `QGroupBox`,
`EffectSwitch` and preset `QComboBox` to `ViperWindow`. The existing
`addEffectPanel()` card wrapper, enable switch, saved expansion state,
mode selection, settings keys, signal hookups and backend writes remain in
the main window. The UI's three modes, labels and tooltip are unchanged.
The rest of the Spatial tab and all other effects remain untouched.

**Maintainer-requested clean rebuild and stall diagnosis:** use a fresh
`build-stage5a/` directory, `cmake -S . -B build-stage5a -DCMAKE_BUILD_TYPE=Release`
followed by `cmake --build build-stage5a --parallel 4 --verbose`, with
output captured by `tee` and `pipefail`. A clean build verifies this
checkpoint but cannot alone establish why prior builds appeared to hang.
Check compiler activity and memory/swap **during** any slowdown, before
interrupting. Launch `./build-stage5a/viper-gui` and verify its About
revision. Test Crossfeed enable/disable, all three modes, collapse/expand
without affecting the enable state, persistence after reopening, and audible
effect if the audio setup permits. Verify that the other Spatial panels
and tab order have not changed. No native compilation has been run by
the assistant. Hold at 5A pending maintainer verification.

**Maintainer feedback, 2026-11-03:** The Crossfeed-only extraction was
reviewed in the GUI and reported to be in order. Treat this as a successful
initial GUI smoke test, not proof that the separate clean-build hang has been
diagnosed or that every acoustic mode/persistence test was explicitly reported.
The follow-on Checkpoint 5B can group the remaining Spatial components
(Headphone Surround+, Differential Surround, Field Surround, Reverberation,
Stereo Imager) for one tab-wide verification gate; preserve panel ordering,
switch/collapse separation, slider controls, and backend mappings.

### Checkpoint 5B — Spatial tab consolidation (awaiting maintainer test)

Consolidated Crossfeed, Headphone Surround+, Differential Surround, Field
Surround, Reverberation and Stereo Imager construction into the single
`src/panels/spatial-panels.h` file. The Crossfeed-only 5A file has been
removed. A typed factory returns the original switch, mode and slider
pointers to `ViperWindow` and delegates slider construction to the
existing `addSlider()` helper. All six card wrappers, expansion settings,
saved controls, signal connections, tooltips, ranges, and DSP publishing
remain unchanged. The main window retains responsibility for signal hookups.
The six-panel card ordering has been preserved.

**Clean-build verification:** use a *new* `build-stage5b/` directory
with `cmake -S . -B build-stage5b -DCMAKE_BUILD_TYPE=Release` and
`cmake --build build-stage5b --parallel 4 --verbose`, capturing the log.
Test all six Spatial cards, independent on/off vs expand/collapse,
mode selections, sliders, previous values, saved settings, and audible
processing. This is the first complete-tab checkpoint; other five
tab files have not yet been extracted. Do not proceed until the maintainer
reports a successful build and functional checks. No native Qt compilation
has been run by the assistant.

**Target organization:** exactly six files under `src/panels/`:
`output-panels.h`, `bass-panels.h`, `eq-tone-panels.h`,
`spatial-panels.h`, `dynamics-panels.h`, and
`correction-panels.h`. Do not create empty files for unfinished tabs.
Keep the ability to split an unusually complex tab later.

### Checkpoint 5C — Output tab extraction (maintainer accepted)

Extracted the widget and layout construction for all three Output effects
(Master Limiter, Playback Gain Control/AGC, and LUFS Targeting) to
`src/panels/output-panels.h`. The typed factory returns the existing control
pointers and the AGC+LUFS interaction warning label to `ViperWindow`.

The main window still owns the three `addEffectPanel()` card wrappers,
independent collapse/expand state, QSettings keys, default values, live labels,
signal connections, backend/CLI synchronization and DSP writes. The native
limiter's permanently checked, disabled switch is unchanged. The warning's
position in the Output tab and its visibility rule are unchanged. The
preexisting `addSlider()` helper is reused; no DSP, ABI, preset, packaging,
submodule or CMake source-target changes were made.

**Acceptance, 2026-11-03 — maintainer confirmation:** The maintainer
confirmed all seven Output-panel checks passed on a fresh native build:
three cards in order, permanently active/disabled Limiter indicator,
independent switches and expand/collapse, limiter sliders, LUFS speed options,
AGC+LUFS combined warning, and values persisting after GUI restart.
**Checkpoint 5C accepted** at the build/GUI smoke-test level; the final
full developer, DSP/CLI precision and hardware regression suites still apply.
The intermittent build hangs are unresolved and tracked separately.

After cloning the refactoring branch with submodules, build from a new
directory with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` and
`cmake --build build --parallel 4`. Check all three Output cards and tab
order, limiter's fixed-on indicator, enabled switches versus collapsing,
sliders' ranges and value labels, Slow/Normal/Fast selections, the AGC+LUFS
warning shown only when both are on, and values after closing/reopening the
GUI. Confirm that changing AGC and LUFS settings sends the expected native
controls; optional CLI edits should refresh without overwriting precise values.
Avoid deliberately raising loudness or output gain while listening at high
volume. If a build stalls, capture compiler process state and RAM/swap
activity before interrupting it.

**Current panel-file count:** 2/6 (`output-panels.h`, `spatial-panels.h`).
Bass, EQ & Tone, Dynamics and Sound Correction remain to be extracted.
No merge or release to `main`.

### Checkpoint 5D — Bass tab extraction (maintainer accepted)

Extracted all four Bass-card control constructions into
`src/panels/bass-panels.h`: ViPER Bass, ViPER Bass Mono,
Psychoacoustic Bass, and Dynamic System. This mechanical extraction
returns original combo, switch and slider pointers to `ViperWindow`
without altering values, defaults, options, tooltips, warnings or slider
ranges. The existing `addSlider()` helper is reused. In the main window,
all four effect-card wrappers keep their stable keys and order:
`bass`, `bass_mono`, `psycho_bass`, `dynamic_bass`.
QSettings keys, saved expansion state, live-label formatting, all
signal connections, backend/CLI synchronization and native DSP writes
remain in `ViperWindow`. No edits to ViPERDSP, LADSPA, shared memory
ABI, CMake, packaging or presets.

**Acceptance — 2026-11-03, maintainer confirmation: PASSED.**
The maintainer reports all seven Bass-panel acceptance checks passed, including
the four card order, independent switch/collapse behavior, Bass and Mono mode
selectors and sliders, Psychoacoustic Bass controls, Dynamic System sliders,
GUI settings persistence after restart, and audible effects at conservative
volume. The fresh native build log also reached `[100%] Built target viper-gui`.
**Checkpoint 5D accepted** at native build, GUI and reported-audio smoke-test
level. The final full developer/CLI regression gate remains pending.

**Intermittent “build hang” investigation resolved:** the test instructions
had run `./build-stage5d/viper-gui` in the foreground after the build; the Qt
application event loop kept the shell occupied until the GUI closed. The
maintainer confirmed that closing the GUI immediately restored the prompt.
The compiler was not stuck in that run. Launch future standalone GUI tests
with `./build-stage5e/viper-gui &`, keeping build success separately logged.

**Test 5D:** Clean-configure and build into `build-stage5d/`, inspect
About revision, and launch this exact revision. Confirm four Bass cards
in their previous order; switches remain independent of collapse/expand;
Natural/Pure Bass+/Subwoofer selectors for Bass and Bass Mono; strength
and frequency controls; mono caution text; Psychoacoustic Bass cutoff,
intensity, harmonic order, original bass control; Dynamic System X/Y
frequencies, side gains, strength and caution text; saved/reloaded values;
and audible processing at conservative levels. If another build hangs,
record process state and log output before interrupting it.

**Panel extraction progress:** 3/6 (Output, Bass, Spatial); still
outstanding EQ & Tone, Dynamics, Sound Correction. No merging or
releasing to `main` before full acceptance and explicit approval.

### Checkpoint 5E — EQ & Tone tab extraction (maintainer accepted)

All six EQ & Tone card constructors have been moved to
`src/panels/eq-tone-panels.h`: Graphic EQ, Dynamic EQ, ViPER Clarity,
Spectrum Extension, Tube Simulator and AnalogX. The panel module uses
three small construction factories, preserving original widget creation
order around the editor initialization and other tabs:
`createEqAndDynamicPanels`, `createClaritySpectrumPanels`, and
`createTubeAnalogXPanels`. The main window still performs all six
`addEffectPanel()` calls with their stable card keys, preserves
independent enable/expand behavior, and owns all Qt signal connections,
Graphic EQ preset save/delete actions, editor wiring, QSettings values,
live graph updates, CLI/backend publishing and precision.

The factories preserve the original Qt widget parents (EQ/Dynamic and
Tube/AnalogX under `tabs_`; Clarity and Spectrum under `central`),
labels, band selector properties, active-band limits, tooltip text,
presets/combo entries, slider ranges and effect ordering. The separate
`src/widgets/eq-overviews.h`, Dynamic/Graphic EQ editor builders and
DSP/native controls are unchanged. No CMake source-target, submodule,
shared-memory ABI or external format changes.

**Acceptance, 2026-11-03 — maintainer confirmation: PASSED.**
All eight EQ & Tone acceptance checks were explicitly confirmed: six cards
and enable/expand behavior; Graphic EQ bands, fixed axis, presets,
fader updates and visibility; Dynamic EQ controls and editor interactions;
Clarity/Spectrum; Tube/AnalogX; persistence, audible effects, and other tabs.
Checkpoint 5E is **accepted** at native-build and GUI/audio smoke-test
level. The final developer suite, CLI precision and device-reconnect
regression gate remains outstanding.

**Acceptance checklist:** Build current refactor branch from a clean
`build-stage5e/` directory and launch `./build-stage5e/viper-gui &`
(not foreground, which previously looked like a build hang). Inspect all
six EQ & Tone cards and independent enable/collapse behavior. Test Graphic EQ
10/15/25/31 bands, preset changes and user preset save/delete, manual
fader-to-fixed-axis-graph synchronization, active fader visibility with
editor open, quit/reopen precision persistence, and audible EQ response.
Test Dynamic EQ main active-band selector, editor and graph markers, selected
band and table synchronization, disabled bands, precision and editor close/
reopen. Test Clarity modes and strength; Spectrum frequency and strength;
Tube model/drive/mix; AnalogX modes and enable states. Confirm no unintended
changes to the other five tabs, preset round trips or audio processing.

**Panel extraction count:** 4/6 (Output, Bass, EQ & Tone, Spatial).
Dynamics and Sound Correction remain to extract. No merge/release to
`main` before the complete refactor is accepted.

### Checkpoint 5F — Dynamics tab extraction (maintainer accepted)

Extracted FET Compressor and Multiband Compressor widget construction to
`src/panels/dynamics-panels.h`, with separate `createFetPanel()` and
`createMultibandPanel()` factories. The full original FET slider set,
Multiband three/five processing-band choices, all five per-band editor pages,
per-band enable switches, frequency labels, and four crossover sliders
retain their labels, tooltips, ranges, tab names, Qt object properties,
and initial hidden/visible state. The Multiband factory fills the existing
main-window band/crossover arrays by reference, avoiding copies or changed
ownership of the saved widget rows.

The main window still adds the two independently collapsible effect cards
with their original keys (`fet`, `multiband`), wires the crossover
expand/collapse toggle to arrow and visibility changes, restores/persists
`ui/mb_selected_band`, handles dynamic 3/5-band views and frequency labels,
and retains all QSettings, effects state, GUI↔CLI/backend synchronization,
DSP publishing, and native parameter precision. No DSP, LADSPA, ViPERDSP,
shared-memory ABI, presets, CMake or packaging changes.

**Acceptance, 2026-11-03 — maintainer confirmation: PASSED.**
All seven Dynamics acceptance checks passed using a correctly identified
`1.1.0-b5b0ba605002` build: FET and Multiband cards, independent toggles
versus collapsing, six FET sliders, 3/5-band switching, five band editors
and per-band switches, crossover controls/labels/expander, selected-band
and parameter persistence, preset recall, and audible DSP behavior with
other tabs unaffected. **Checkpoint 5F accepted** at native-build,
GUI and audio smoke-test level; the final full developer, CLI precision
and output-reconnect regression gates still apply.

**Test 5F:** Use a fresh `build-stage5f` directory and build the
`viper-gui` target, then launch `./build-stage5f/viper-gui &` so the
Qt event loop doesn't appear to hang the terminal. Check FET and Multiband
card order; independent enable and collapse controls; six FET parameter
sliders and labels; 3/5-band selector and five per-band editors; per-band
enable/disable and precision; four crossover frequencies with the edit
expander/arrow and updated band-range labels; tab selection persistence
after GUI restart; saved preset recall and cautiously audible processing.
Check other tabs remain functional. Run final full developer tests before
considering `main`.

**Panel files extracted:** 5/6 (Output, Bass, EQ & Tone, Spatial, Dynamics);
Sound Correction remains. Next are Checkpoints 6 and 7, then complete
integration acceptance and explicit maintainer merge approval.

### Checkpoint 5G — Sound Correction tab extraction (accepted with deferred DDC profile testing)

All three Sound Correction widget constructors are now in
`src/panels/correction-panels.h`, divided into
`createDdcPanel()`, `createSpeakerCorrectionPanel()`, and
`createConvolverPanel()`. Control labels, selection/refresh/import
buttons, tooltips, layout policies and status widgets are unchanged.
The DDC combo remains an `IrsComboBox`, with a selectable complete
profile label and .vdc warning. Speaker Correction retains its explanatory
warning. Convolver retains its `IrsComboBox` selector, refresh/reload and
import buttons, read-only, selectable wrap-anywhere long-filename field,
cross-channel mix slider (0–100) and backend status line.

**Behavioral boundary:** `ViperWindow` still owns both
`beforeOpen` lazy-refresh callbacks, both `QFileSystemWatcher` objects
and directory-change callbacks, DDC and IRS import file validation and
copy/duplicate naming, guarded live selection and reload callbacks,
QSettings state and all native DSP/CLI publishing. The original card keys
`ddc`, `speaker_correction`, and `convolver` and their independent
enable-versus-expansion state are unchanged. No changes to the DSP engine,
LADSPA, shared-memory ABI, CMake, third-party submodule, or preset formats.

**Verification: PARTIAL; DDC profile tests outstanding (2026-11-03).**
The maintainer confirmed that the GUI reports `1.1.0-3a73c3b3aa27`,
matching this checkpoint's commit, and that the IRS full-filename field
does wrap correctly; the expanding-width dropdown popup is also working as
intended and must not be made to wrap or truncate unnecessarily. Convolver
and other Sound Correction behavior appear to work as intended in the
maintainer's manual testing. The maintainer has **no .vdc test files**,
so DDC profile import/selection/processing cannot be considered tested.
The other five tabs were visually/smoke checked and appear fine, but
have not been exhaustively retested field by field in this revision.

Static checks of the new module and unchanged callbacks passed.
A native build was identified by its GUI version, but the full developer
suite and comprehensive hardware regression have not yet run.
**Maintainer exception decision, 2026-11-03:** Defer .vdc
profile import, selection and processing to final regression. This
explicitly authorizes proceeding to Checkpoint 6 without falsely
marking DDC tested; the DDC exception remains a final acceptance
blocker until tested or explicitly dispositioned by the maintainer.

**Test 5G:** Fresh `build-stage5g/` build of `viper-gui`, launched
with `./build-stage5g/viper-gui &` to avoid blocking the terminal.
Verify all three cards in order, enable/expand separation, DDC .vdc
selection/import/refresh and visible profile name, Speaker Correction
toggle and advisory, and Convolver .irs/.wav imports, instant kernel
switching, Refresh/Reload, full very-long filename wrapping/selectability,
cross-channel mix and status/errors. With an open combo popup, a folder
change must not reset its scroll or rebuild it; deferred refresh should
appear after reopening. Test duplicate/unreadable files non-destructively;
ensure GUI quit/restart retains selection and settings. Verify audible
convolution conservatively and no regressions in other tabs. Sample-rate
mismatch behavior must remain safe, but fixing the existing audio engine
is outside this mechanical extraction.

**Six of six panel files now exist:** `output-panels.h`, `bass-panels.h`,
`eq-tone-panels.h`, `spatial-panels.h`, `dynamics-panels.h`,
`correction-panels.h`. The full refactor is **not finished**: Checkpoint 6
(settings/backend synchronization), Checkpoint 7 (main-window integration),
full regression/packaging verification and explicit maintainer approval
still precede any merge to `main`.

## Checkpoint 6 — Settings/backend synchronization (focused maintainer acceptance; broader regression deferred)

Mechanically moved six ViperWindow member implementations into
`src/sync/gui-settings-bridge.inc` and
`src/sync/gui-backend-bridge.inc`: `persistCurrentSettings()`,
`ensureBackend()`, `publishToBackend()`, `liveUiSnapshot()`,
`readBackend()` and `pollBackend()`. Both .inc files are included
**inside the original class scope** once. The implementation remains
bound to ViperWindow as a deliberate intermediate encapsulation:
no new QObject, threads, APIs or reordered signal handlers. Its
existing control loads/stores, request serials, signal blockers,
live slider guards, offline pending edits, audio activity status,
500ms polling and 300ms QSettings debounce remain in their original
method order. This does not yet claim a fully independent model layer.

New `src/sync/gui-control-channel.h` isolates two directly testable,
Qt-free policies: named IPC mapping reconnect and preservation of
precise live native values unless the actual GUI slider is edited or
a complete preset/reset replaces them. Integration changes only
delegate these existing decisions; the rest of the method bodies
are preserved. IPC `Controls` layout/version, CLI settings key names,
DSP/LADSPA implementation, ViPERDSP submodule and packaging unchanged.

Added `tests/check-gui-sync-contract.py`, compiling/running fake
reconnect and precision cases in a temporary C++17 fixture, hooked
into `tests/check-developer.sh`. Updated the precision source-contract
to follow the synchronization implementation files. **Note:** older
GUI source-contract tests now inspect the actual extracted panel,
widget, theme and synchronization implementations while preserving
their substantive control/behavior checks.

**Isolated control policy test: PASSED.** A standalone C++17
reconnection/precision test passed with exact Git-blob-verified
test and helper bytes. This is not a native Qt/PipeWire regression test.

**Status: focused maintainer acceptance (2026-11-03).** The maintainer
successfully built and opened the Checkpoint 6 GUI on Arch Linux and
reported the full developer suite passing. Native GUI→DSP slider edits
produced audible changes; CLI `bass-on` and `bass-off` correctly
updated the existing GUI without readback feedback; edits survived
closing/reopening the GUI. Precision preservation was tested by
`viper4linux-ctl bass-gain 5.37`: editing the unrelated Clarity
slider retained the exact native 5.37 value, and deliberately editing
the Bass slider replaced that native precision as intended.

**Backend restart:** An initial restart through `viper4linux` opened
a second GUI and reverted visible Bass values, so this observation is
**inconclusive**, not a verified synchronization failure or a passing
test. The maintainer retested using a backend-only launcher restart,
keeping the original Checkpoint 6 GUI as the sole GUI instance; the
offline Bass frequency edit and persisted settings appeared to survive
reconnection. Mark **focused offline-edit/reconnect PASS, with the
second-GUI interaction explicitly deferred to the broader regression**.
The assistant did not run native Arch tests locally; results here are
the maintainer's reports.

**Per-output persistence requirement:** The existing Audio Output
memory setting under Preferences is opt-in. When enabled, the
`OutputMemory` monitor stores full native snapshots separately for
stable physical-output identity keys in `~/.local/share/viper4linux/outputs`,
checkpoints edits while an identified sink remains active, and restores
that sink's own snapshot when revisited. This must persist across
GUI close, backend restart, physical sink switch, and device reconnect.
The single-sink backend restart is **not** proof that two independent
physical sink snapshots correctly restore. The final regression must
verify A→B→A→B, restart, output memory enabled/disabled, exact native
precision preservation, and no unwanted overwrite by a second GUI.
Also verify activity/idle/offline status and full effect panel coverage.
Do not change QSettings/global defaults as a substitute for per-output
snapshot restoration.

**Final Checkpoint 6 regression checklist (focused acceptance done):** Build `viper-gui` in a fresh `build-stage6/`,
then run `./build-stage6/viper-gui &`. Test GUI-to-DSP changes,
CLI-to-GUI readback without feedback loops, CLI native precision
preserved while unrelated GUI sliders change, deliberate override on
edited slider or preset/reset, pending offline edits applied after DSP
reconnect, readback UI updates persisted on GUI restart, per-output
memory and output-device reconnection, and activity/idle status.
Then run the complete developer regression suite and audit any
test-contract failures against the moved source rather than deleting
coverage. **Final DDC .vdc import/processing test still outstanding.**

After verification proceed to Checkpoint 7 integration/cleanup,
complete native developer/packaging and real-audio acceptance, and
explicit maintainer approval before merging to `main`.

## Checkpoint 7 — Main-window integration and cleanup (automated regression passed; live acceptance pending)

A second mechanical extraction moved seven **cohesive, ViperWindow-owned**
method regions into `src/window/`:
`window-presets.inc` (preset snapshot/reset/manager bridge),
`window-layout.inc` (window sizes, theme, panel orchestration, value labels),
`window-correction.inc` (VDC/IRS libraries and live selection),
`window-shell.inc` (tray, autostart, per-output preferences/status, About/Help),
`window-graphic-eq.inc` (Graphic EQ layouts/presets),
`window-dynamics.inc` (Dynamic EQ/FET/multiband editor state),
`window-effect-actions.inc` (full/panel preset apply/reset actions).

They are **implementation-only includes inside ViperWindow**, preserving
the class scope and existing constructor, signals, `sender()`,
signal blockers, QSettings storage, per-output memory callbacks,
backend bridge and DSP control contract. There are no additional
translation units or public component APIs. The original constructor,
GUI lifecycle/event hooks, `push()`, offline status and owned state
remain directly in `viper-gui.cpp`. No deliberate feature change.
The six existing effect-panel files remain as accepted. Source packaging
uses `git archive`, which includes these checked-in `.inc` modules;
CMake still builds a single `viper-gui.cpp` Qt translation unit with
private class-body includes. No extra CMake target is required.

**Source-integrity gate:** The extraction was checked for exact
lossless reconstruction of the original `viper-gui.cpp` by substituting
the seven original method chunks back for their single include sites.
`tests/check-window-integration-contract.py` now covers the
once-only window wiring and important moved methods. The developer source-contract helper indexes all seven new files.
Legacy named-preset, preset reset, preset-save choice, About and tray
contracts now follow the current module boundaries without removing
behavioral requirements. The new integration contract is run by
`check-developer.sh`. A native Qt build was performed by the
maintainer; the complete device/audio acceptance matrix remains pending.

**Automated suite (maintainer, 2026-11-03): PASS reported.** The
maintainer successfully worked through the stale source-contract
assertions caused by the module extraction and reported success after
rerunning `tests/check-developer.sh`. The assistant independently
reviewed the revised assertions against branch source but has not run
the native Arch suite itself. Source-contract repairs were test-only
and did not change DSP, GUI runtime or IPC code.

**Sample cross-section live regression (maintainer, 2026-11-03):
PASS reported.** The maintainer exercised a representative sampling
of controls across all six tabs and reported expected functionality,
with special attention to IRS file loading and Graphic EQ. This is
a successful sample-based runtime smoke test, **not** exhaustive
per-control coverage. Full Dynamic EQ, window/tray lifecycle, named
preset save/load/reset, per-output memory A→B→A→B and valid DDC
profile processing are not yet accepted as a result of this sample.

**GUI lifecycle runtime tests (maintainer, 2026-11-03): PASS reported.**
Five manual operations worked as expected: theme switching, window
resizing, minimizing to system tray, restoring from tray and closing/
reopening the GUI. These are live desktop observations, distinct from
the automated source-contract suite. Startup preference variations
were not separately reported by this result.

**Named-preset runtime acceptance (maintainer, 2026-11-03): PASS
reported.** The maintainer confirmed creating, loading, modifying
and resetting named effect presets still work following Checkpoint 7
modularization. These results validate the exercised operations, but
do not independently certify every preset-management edge case.

**Dynamic EQ graph clarification (maintainer report, 2026-11-03):
help copy added; native acceptance pending.** The maintainer confirmed
editor/overview synchronization and observed Band 4's apparent peak
decrease when reducing Band 8 to -4.50 dB. The screenshots showed Band
8 configured as **Low shelf** at 4000 Hz, Q 3.20. Such a filter
attenuates frequencies *below* 4000 Hz, so it legitimately changes
the combined curve around Band 4 at 250 Hz while Band 4's individual
configured gain remains +6.50 dB. Threshold, attack and release may
also influence the live audio but are not modeled by the preview.

**Maintainer follow-up: filter-shape response confirmed.** Holding the
4000 Hz / -4.50 dB / Q 3.20 Band 8 settings otherwise steady and
switching Low shelf to Peak produced a *significant expected change*
in the displayed graph. This strongly supports ordinary low-shelf
interaction as the explanation for the prior observation rather than
a synchronization defect. It does not validate the mathematical
accuracy of the approximate curve against native DSP measurements.

An earlier speculative change forced a fixed +/-12 dB graph axis and
clipped stacked responses; this was **reverted without altering the
DSP** because the reported behavior was filter interaction rather
than evidence of a scaling error. The existing illustrative combined
response/marker coordinates and auto-ranging scale were restored
exactly from the pre-change source, and the plot's mathematical
approximation remains unchanged. Instead, the popup editor now
explicitly distinguishes individual gain from combined response,
explains frequency/Q/shelf influence, and discloses that it is an
approximation, not measured or live audio. Static tests check the
previous curve/marker scale and the explanatory UI copy. Maintainer
native build and inspection are still needed before final acceptance.

**Dynamic EQ follow-up (maintainer, 2026-11-03): focused retest
appears satisfactory.** After restoring the original response-graph
renderer and adding explanatory popup text, the maintainer reports
Dynamic EQ still seems functional. This is a positive runtime
smoke-test observation, not a measurement validating the illustrative
curve against the actual native DSP transfer function.

**Audio Output per-device memory (maintainer, 2026-11-03): basic
A→B→A→B and restart persistence PASS reported.** The maintainer
switched repeatedly between EDIFIER NeoBuds Planar and the internal
output, observed previously changed effect settings restored on return,
then restarted the ViPER backend and confirmed a changed setting took
effect again after further output switches. The source launcher path
regression caused by the move of `start.sh` into `scripts/` was fixed
separately; starting through `./viper4linux` again brought up the
Audio Output monitor and identified the headphones. Direct
`build/viper-gui` launch intentionally does not manage the backend
or monitor. This positive live test does not yet cover the memory-off
mode, exact native precision, or two-GUI interference.

**Native ViPER-DDC live processing (maintainer, 2026-11-03):
audible-effect PASS reported.** A deliberately non-neutral, dual-rate
`.vdc` low-pass test profile was provided for 44.1/48 kHz. The
maintainer enabled it and reported that it muted the treble
considerably, which supports that the selected coefficients were
actually processed rather than merely displayed as loaded. This is a
subjective listening confirmation, not a measured frequency-response
validation. The maintainer explicitly confirmed the A/B sequence: DDC enabled
attenuated treble considerably; switching DDC off restored normal
treble; switching it on again restored the attenuation. Mark the
valid-profile native processing and live enable/disable test **PASS**.
Unsupported sample-rate behavior remains unverified.

**Audio Output memory edge cases (maintainer, 2026-11-03):
memory-off and precision preservation PASS reported.** The maintainer
confirmed the two remaining per-output memory behaviors, but deliberately
did not open a second GUI. Rather than allow two GUI instances to race
on settings, Checkpoint 7 now introduces one-GUI-per-user-session
ownership in the GUI executable itself: a later launch requests the
existing window to be shown/raised (including from the tray) and exits
without creating a second `ViperWindow`. The independent backend and
Audio Output monitor lifecycles are unchanged. Qt native compile and
live Cinnamon/X11 acceptance of the new guard remain pending.

**Live acceptance outstanding:** Rebuild and run the entire
`bash tests/check-developer.sh` suite as needed, six-tab effect/audio
functionality, preset save/load/delete/reset, theme switching,
tray/minimize/restore/startup preferences, Graphic/Dynamic EQ editors,
Convolver import/popup behavior, GUI↔CLI precision,
disconnected-backend edits/reconnect, remaining Audio Output memory
edge cases (memory-off, precision and two-GUI interference), and
package/archive content. Native DDC valid-profile processing and
enable/disable A/B passed; unsupported sample-rate behavior remains
unverified. Do not mark the whole refactor complete or
merge to `main` until maintainer acceptance.

## Per-stage verification gate

- Review each diff to ensure it is **mechanical extraction**, with zero deliberate feature changes.
- Build `viper-gui` from the branch using its normal CMake/Arch packaging path, with the pinned ViPERDSP submodule available; record exact command and result.
- Run relevant focused regression tests after each stage.
- Run `bash tests/check-developer.sh` before final acceptance (and earlier for affected high-risk functionality), independently of routine `makepkg` smoke checks.
- Test a running installation under Cinnamon/X11 with real PipeWire audio. Validate artwork is not relevant here; validate audio controls, presets, window resizing, mouse wheel focus, theme/tray, source and device changes.
- Record failure, corrective change, and result on the branch before continuing.
- If any stage cannot be compiled/tested, **mark it unverified and do not proceed as though it passed**.

## Final acceptance gate — only then consider `main`

Complete source review, clean native rebuild and full developer regression suite; manual GUI/real-audio acceptance, repeated open/close and backend-independent operations, and packaging checks. Confirm no ABI/behavior/setting changes and no unexpected modification to the DSP engine or submodule. Merge into `main` **only with explicit maintainer approval after the entire separation**.

## Constraints and related work

- The pinned ViPERDSP source is tracked as a Git submodule; downstream patches operate on a **private build copy**. Preserve this separation.
- Other current work: experimental one-way ViPER4Android JSON preset export lives in `feature/viper4android-preset-export`; maintain separately until its phone compatibility is verified.
- Public documentation may describe the plan, but the experimental branch is not a stable release or a promise of future support.
- Independent developer review (especially Qt design and real-time audio assumptions) remains welcome.

## Where to continue

In a new conversation, give the repository name, branch `refactor/gui-modularization`, and this file path `docs/GUI-MODULARIZATION-PLAN.md`. First inspect branch history and the last verified checkpoint; do not assume an in-progress stage is complete. Continue the *next unverified* stage without changing `main`.
