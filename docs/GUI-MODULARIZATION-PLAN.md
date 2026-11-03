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

## Checkpoint 1 — Custom Qt widgets

Extract the self-contained widget classes from `src/viper-gui.cpp` (such as `SafeSlider`, `EffectGlyph`, `EffectSwitch` and other reusable visual widgets) into named widget source/header components. Preserve behavior, signals, event propagation, accessible names, painting, colors, and size policies exactly. Prefer proper headers/translation units over gratuitous header-only implementations; avoid changing Qt meta-object requirements.

Rebuild and test: slider mouse wheel only when focused, switch hit targets/toggling, all icon styles, effect panel collapse/expand and high-DPI/scaled rendering.

## Checkpoint 2 — Appearance and theme handling

Isolate light/dark/classic themes, palette and widget styling, icon/theme selection and visual state updates. Keep user configuration keys and theme redraw behavior unchanged.

Rebuild and test: selected themes, system theme changes, tray/icon rendering, resize and restart persistence.

## Checkpoint 3 — Native named-preset management

Isolate native `QSettings` named preset reading/writing, validation, migration, Save/Load/Rename/Delete, reset and audio-output assignment. Preserve original on-disk INI format, UUID filenames, path safety and atomic writing, loading semantics and every confirmation prompt.

Rebuild and test: complete preset round-trip, legacy migration, overwrite/rename/delete, Apply to Audio Output, Reset to Defaults, source selection, and independent output memory.

**Keep the separate Android-export experiment on its own branch** (`feature/viper4android-preset-export`). It is not part of the refactor; reconcile after the GUI restructuring is accepted. Avoid mixing that feature into a code-movement commit.

## Checkpoint 4 — Graphic EQ and Dynamic EQ UI

Extract EQ widget composition, graphical overview, popup/editor controls, selected-band state, preset curves and layout handling. Preserve 10/15/25/31-band layouts, exact gain values, disabled-band behavior, Dynamic EQ active-band counts, band selection and precision compatibility.

Rebuild and test: full EQ edit/save/reopen/load round-trips, mouse interactions, resizing and external CLI changes.

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
