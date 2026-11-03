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

### Checkpoint 4C — Graphic EQ detailed editor construction (BLOCKED: reported regression)

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
Do not proceed to Checkpoint 5 until diagnosed and retested.


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

## Checkpoint 5 — Effect panels and layout construction

Move construction and hookup of the six tab panels into reasonably scoped components (potentially grouped by tab) without changing signal connections, visibility, disabled/default states, effect labels or settings values. Avoid a single giant 'panels' file.

Rebuild and test: all 24 effect groups, independent toggling versus collapse, all slider ranges/defaults, scrolling, tab order, IRS and DDC controls.

## Checkpoint 6 — Settings/backend synchronization

Isolate synchronization and transport adapter responsibilities from presentation. This is the riskiest GUI stage: preserve the existing shared-memory control contract and version; do not change DSP/LADSPA internals or settings precision. Preserve the GUI's ability to reflect CLI edits without overwriting them, backend independence, source restoration and state update timing.

Rebuild and test: GUI→DSP, CLI→GUI, CLI→DSP, GUI quit/reopen, backend restart after updates, saved precision overrides, independent per-output memory, device reconnections, session state.

## Checkpoint 7 — Main-window integration and cleanup

Reduce `viper-gui.cpp` to orchestration of the main window and composed components. Remove only demonstrably unused includes/code. Review component boundaries, documentation, and CMake targets; avoid sweeping renames or broad formatting churn that obscures semantic changes.

Rebuild and test all stages again.

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
