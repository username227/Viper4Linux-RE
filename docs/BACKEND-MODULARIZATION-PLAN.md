# Native DSP Backend Modularization Plan

**Project:** Viper4Linux-RE  
**Working branch:** `refactor/backend-modularization`  
**Starting point:** merged `main`, commit `17bc4fd5993dea2c6c44e3eaf9859dbb988dd8da` (2026-11-03)  
**Status:** planning only. No native DSP source changes have been made on this branch.  
**Merge gate:** maintainer's explicit approval after successful regression and live-audio acceptance.

This is the **single active modularization plan** for the next refactor. The
completed GUI modularization plan was retired from the working tree; its full
text remains in Git history. `docs/DEVELOPMENT.md` is the permanent developer
guide and unreleased change log, not a second refactor plan. Keep this plan
on the working branch during development; retire it from the branch's
mergeable tree when the work is finished if the maintainer does not want
completed planning documents carried into `main`.

## Objective and scope

The largest remaining single application implementation is
`src/viper-ladspa.cpp` (**84,547 bytes on the starting `main` revision**).
It is the LADSPA plugin loaded by PipeWire's filter-chain, not the entire
backend stack. It bridges interleaved stereo ViPERDSP to PipeWire's audio
buffers, maintains shared-memory controls, configures native effects, and
coordinates non-real-time work such as IRS and DDC loading.

Improve auditability and maintainability by separating distinct **source
responsibilities**, without changing DSP sound, controls, settings format,
plugin identity, audio routing, or process lifetimes. Extraction is not a
feature initiative.

The GUI has already been modularized. This branch must **not** rework the
GUI, CLI, output monitor, snapshot process, packaging policy, or pinned
ViPERDSP upstream source as a side effect.

## Non-negotiable real-time and compatibility rules

- Preserve the exact LADSPA exported descriptor, port order, plugin ID,
  instantiation/cleanup behavior, and public ABI.
- Preserve the `viper-control.h` shared-memory layout and all
  request/applied serial, readiness/status, and settings semantics.
- Preserve native ViPERDSP algorithms, effect parameter mapping, bypass,
  EQ/DEQ behavior, limiter, AGC/LUFS, dry/wet logic and audible levels.
- Keep the audio callback's execution model and observable processing order
  unchanged. **Never introduce blocking mutexes, filesystem activity,
  logging, thread creation, unbounded allocations or GUI work into it.**
- Preserve current handling for unsupported sample rates, IR/DDC parsing,
  loading failures, and non-real-time coefficient updates.
- Do not restructure the PipeWire output monitor, launcher, smart filter
  routing or per-output memory. These are separate backend components.
- Keep ViPERDSP as the pinned submodule. Preserve the downstream patches'
  current build-copy strategy.

A mechanical `.h` or implementation-only `.inc` extraction inside the
existing translation unit is preferable initially to introducing new
compiled targets or altering object lifetimes. Avoid speculative
architectural changes until tests justify them.

## Working checkpoints

1. **Baseline and responsibility map.** Record the starting `main` SHA,
   relevant byte/line sizes, the native Arch compilation result and full
   `bash tests/check-developer.sh` result. Identify control mapping,
   startup settings, background workers, effect configuration,
   audio callback, and LADSPA exports by exact source ranges. No code
   change at this checkpoint.
2. **Startup preferences and control-channel lifecycle.** Consider extracting
   parsing and shared-memory page ownership only if the existing lifetime,
   synchronization and initialization order can remain identical. Run
   launcher, settings, IPC and sample-rate tests.
3. **Background file and coefficient work.** Consider separating the
   Convolver IRS, sample-rate conversion and ViPER-DDC update machinery.
   Preserve worker-thread handoffs and real-time safety. Test enabled /
   disabled and invalid or unsupported file/rate cases.
4. **Non-real-time native effect configuration.** Group native apply/update
   routines by responsibility without changing call order or state. Test all
   24 effect groups, including dynamic EQ, Graphic EQ, FET, multiband,
   stereo imager, Crossfeed and Tube/AnalogX.
5. **Processor organization and audio callback.** Review the remaining
   `Processor` and exported LADSPA glue. Split further *only* if the change
   is genuinely clearer and no additional synchronization or runtime
   behavior is introduced. Keep the hot-path implementation cohesive.
6. **Full regression and packaging.** Run the complete developer suite,
   clean Arch source/package build and artifact checks on this exact head.
   Examine binaries, symbol exports, shared-memory ABI and sample-rate
   behavior. Record measured file sizes and any intentional deviations.
7. **Maintainer live acceptance.** On Arch/Cinnamon with real PipeWire audio:
   listen across physical outputs, switch/reconnect Bluetooth, verify
   GUI↔CLI↔DSP synchronization, persistent settings, remembered outputs,
   IRS/VDC changes, EQ/DEQ, bypass, tray/launcher and restart behavior.
   Only after explicit maintainer approval consider a PR into `main`.

After every extracted component, inspect the diff for mechanical movement,
compile, run the targeted automated tests, and ask for focused live checks
where the change touches actual audio. A successful build alone is not proof
of sound equivalence. Do not proceed past a failing checkpoint.

## Current implementation landmarks

These refer to the **pre-refactor starting file**, not promised file
boundaries:

- Startup preferences and saved IR/DDC paths near the beginning.
- Shared control mapping and `Processor` construction further down.
- Native apply/configure routines in the middle of `Processor`.
- Asynchronous Convolver/DDC state and workers around those routines.
- `Processor::run(unsigned long frames)` is the native audio callback;
  it must remain real-time safe.
- LADSPA create/connect/run/cleanup and `ladspa_descriptor()` at the end.

Inspect the current source and tests before choosing exact extraction
boundaries. Avoid hardcoding line numbers, which change during the split.

## History and release notes

The preceding GUI refactor was merged as PR #1 on 2026-11-03; the feature
inventory, tests and GUI source-size snapshot are retained in
`docs/DEVELOPMENT.md`. Keep the new backend refactor changelog separate
until it is merged or accepted for a release. The earlier GUI planning
document is preserved by its original Git commits, not duplicated as
another active plan.

**Next action:** checkpoint 1 inventory and baseline testing. No backend
refactoring work is yet accepted.
