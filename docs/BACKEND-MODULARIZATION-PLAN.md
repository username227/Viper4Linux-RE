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

The backend is substantially smaller than the former monolithic GUI. Favor a
small number of cohesive files rather than dividing every effect or helper
into a separate module. The **working expectation is approximately four source
files**, but this is not a quota or a fixed design. Keep code together when
its state, lifetime or execution order makes separation counterproductive.
The inventory and each extraction determine the final boundaries.

1. **Baseline and responsibility inventory.** Record the starting source
   revision, file sizes, native Arch compilation result and full
   `bash tests/check-developer.sh` result. Map the startup settings,
   shared-control lifecycle, processor state, worker activity, native effect
   configuration, audio callback and LADSPA exports. Do not predetermine
   extraction boundaries or modify native DSP code at this checkpoint.
2. **Startup settings and controls.** Extract the cohesive startup-settings
   parsing where it improves readability. Keep the short shared-control
   ownership code in the main file unless inspection reveals a compelling
   reason to move it. Preserve initialization order, reference counting,
   settings compatibility and shared-memory ABI; test launcher, settings,
   IPC and sample-rate behavior.
3. **Processor organization.** Extract or regroup processor implementation
   only along natural boundaries. Worker-driven IRS/DDC loading and native
   effect configuration share state and synchronization; do not split them
   merely to create more files. Preserve all 24 effect groups, worker
   handoffs, non-real-time coefficient preparation and effect ordering.
   Compile, review the mechanical diff and run targeted regressions after
   each meaningful extraction.
4. **Audio callback and full regression.** If it is clearer as a cohesive
   implementation section, extract the complete real-time callback without
   changing its processing order or safety guarantees. Keep processor state
   and lifecycle where they are easiest to audit. Run the full developer
   suite, clean Arch source/package build, artifact checks and LADSPA
   symbol/ABI verification; inspect sample-rate, IRS/DDC and shared-memory
   behavior. Record final file sizes and explain any departures from the
   initial structure.
5. **Maintainer live acceptance.** On Arch/Cinnamon with real PipeWire
   audio, verify physical outputs, Bluetooth reconnects, GUI↔CLI↔DSP
   synchronization, persistent settings, remembered outputs, IRS/VDC
   changes, EQ/DEQ, bypass, tray/launcher and restart behavior. Only after
   explicit maintainer approval consider merging into `main`.

A checkpoint can contain several small, individually verified mechanical
changes. A successful build alone does not prove sound equivalence. Stop
at any failing test and resolve it before proceeding. The five checkpoints
are acceptance stages, **not a requirement to create five modules**.

## Current implementation landmarks

The current source contains natural responsibilities: startup preferences,
shared control mapping, the `Processor` lifecycle and state, asynchronous
IRS/DDC and native filter updates, effect configuration, the
`Processor::run(unsigned long frames)` real-time callback, and LADSPA
entry points/descriptors.

These are **inspection landmarks, not promised file boundaries**. No line
ranges or exact extraction points are prescribed. In particular, keep the
short LADSPA glue in the main file unless there is a substantive reason to
move it; keep tightly coupled worker and processor code together when that
makes synchronization easier to reason about. Revisit the arrangement after
each checkpoint rather than forcing a preselected file count.

## History and release notes

The preceding GUI refactor was merged as PR #1 on 2026-11-03; the feature
inventory, tests and GUI source-size snapshot are retained in
`docs/DEVELOPMENT.md`. Keep the new backend refactor changelog separate
until it is merged or accepted for a release. The earlier GUI planning
document is preserved by its original Git commits, not duplicated as
another active plan.

**Next action:** checkpoint 1 inventory and baseline testing. No backend
refactoring work is yet accepted.
