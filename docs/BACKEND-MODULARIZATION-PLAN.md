# Native DSP Backend Modularization Plan

**Project:** Viper4Linux-RE  
**Working branch:** `refactor/backend-modularization`  
**Original branch point:** merged `main`, commit `17bc4fd5993dea2c6c44e3eaf9859dbb988dd8da` (2026-11-03)  
**Checkpoint 1 tested baseline:** `1aac063bf8351dbfd7670f34b7195bb498f888c1` (2026-11-04), after merging modernized `main` at `cab05cc8027a0c9f98392710b227dbbf0effe1f2`  
**Status:** Checkpoint 1 complete (source inventory and maintainer Arch compilation/regressions); no native DSP implementation changes yet.  
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

## Checkpoint 1 — completed source and Arch regression baseline (2026-11-04)

**Acceptance:** Checkpoint 1 completed. This establishes a clean *pre-refactor*
baseline, not live-audio acceptance or post-extraction sound equivalence.
No native DSP implementation changes have been made.

### Baseline identity and maintainer-executed verification

- Fresh clone at `~/Desktop/Viper4Linux-RE-backend`, on branch
  `refactor/backend-modularization`, head
  `1aac063bf8351dbfd7670f34b7195bb498f888c1`. The CMake modernization
  from `main` was merged before the clone. The main working tree and
  ViPERDSP submodule were clean before and after the tests.
- `src/viper-ladspa.cpp`: **1,493 lines / 84,545 characters**,
  Git blob `ede9a24c7c325cb13d75caeb6e34bdc1a04a39f1`; identical
  on `main` and the backend branch. Historical size figures from earlier
  revisions do not indicate any DSP modification here.
- Pinned upstream ViPERDSP: `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`.
  CMake prepared the private DSP source copy and applied all three verified
  downstream corrections: LUFS speed, Stereo Imager preallocation, and
  Multiband Compressor preallocation. The upstream submodule stayed clean.
- Maintainer Arch toolchain: GCC/G++ **16.2.1**, Python **3.14.9**.
  Release compilation completed at **100%** with no compiler warnings/errors
  shown in the provided build log; routing config was generated in
  `build/routing-config/`. The mock test binaries were compiled too.

The maintainer ran, in the fresh source checkout:

~~~bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVIPER_BUILD_DEVELOPER_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tests/check-developer.sh
cmake --build build --target viper_source_check
nm -D build/viper_dsp_ladspa.so | grep ' ladspa_descriptor$'
git status --short
git -C third_party/ViPERDSP status --short
~~~

Recorded results:

- **CTest 4/4 PASS**: `viper_package`, `viper_mock`,
  `viper_contracts` and `viper_release`.
- **Full standalone developer regression suite PASS**, including native
  mock DSP effects/IPC, GUI and CLI contracts, saved setting precision,
  EQ/DEQ, IR resampling, DDC parsing, native snapshots, release and package
  verification. All **22 physical Audio Output identity/memory tests PASS**.
- **`viper_source_check` PASS** (no audio or backend launched).
- Exported native LADSPA symbol verified:
  `0000000000017580 T ladspa_descriptor`. The absolute symbol address is
  build-dependent, not itself an ABI contract.
- Expected negative-test diagnostics for missing IR files and invalid CLI
  commands are not failures; isolated test callback counters do not
  characterize the currently installed playback session.

**Not yet verified:** post-extraction audible equivalence; live PipeWire,
Bluetooth output changes, sample-rate transitions, or installed backend
replacement in this fresh clone. Those require later maintainer acceptance.

### Baseline source responsibility map

Approximate line ranges refer to the **unchanged** 1,493-line source and are
inspection landmarks rather than promised extraction boundaries.

| Lines | Responsibility | Couplings |
| --- | --- | --- |
| 1–35 | Headers, `kChunk=2048`, plugin ID and seven ports | Constants shared with callback/descriptor |
| 37–325 | Configuration paths and `load_saved_settings()` | Qt-compatible INI, selected EQ layout, legacy settings then `[precision]` overrides |
| 327–348 | `control_mapping`, `control_mutex`, reference count, acquire/release | One shared mapping per process; initial load, failure fallback and final cleanup |
| 349–425 | `Processor` construction/destruction | Wet/dry engines, effect defaults, sample rate, saved IRS/DDC, worker lifetime |
| 427–563 | `ir_worker_loop()` | Request/applied serial tracking and off-thread Convolver, EQ, Tube, AnalogX, Crossfeed, compressors, Imager, DDC, Speaker and DEQ work |
| 564–934 | Worker-associated native effect configuration | Same wet processor and guarded mutex/transition state |
| 935–954 | `run_dry_while_loading()` | Nonblocking callback fallback through dry engine and preallocated buffer |
| 956–1142 | Other native effect configuration | Wet/dry processor state and last-value caches; some setters called from callback |
| 1143–1358 | `Processor::run(unsigned long frames)` | Real-time atomic reads, changed-value updates, chunked stereo processing and crossfade |
| 1360–1451 | Port reader and `Processor` state | Processor objects, worker, atomics, mutex, buffers, cached settings, crossfade |
| 1453–1492 | LADSPA hooks and descriptor | Exported ABI, plugin ID/label, port ordering and lifecycle |

### Dependency and synchronization findings

1. **Startup and IPC:** settings are read at first control-page acquisition,
   not from the audio callback. The loader reads legacy `[General]` values
   and the active EQ layout, then applies native `[precision]` overrides
   *last*. The mapping is reference-counted and must retain fallback
   behavior if another instance owns the control server.
2. **Lifetime:** the `Processor` initializes native wet and dry engines,
   sample-rate/effects, startup IR/DDC and controls before the worker is
   spawned. Destruction signals and joins the worker **before** releasing
   shared controls. Splitting files must not alter these lifetimes.
3. **Worker ↔ callback:** the worker processes independent request/applied
   serials and performs potentially expensive file parsing and native
   coefficient rebuilds off-thread. It owns guarded updates to the wet
   processor. The callback attempts `wet_mutex` using
   `std::try_to_lock`, **never waits**, and takes the dry-processing path
   on lock contention. Keep that fallback and the serial handshake intact.
4. **Transition policies are deliberately different:** Convolver kernel
   replacement uses a guarded dry transition. Invalid DDC replacements
   clear old coefficients and native DDC accepts only 44.1/48 kHz.
   EQ-layout, Tube and AnalogX have their own handoff behavior.
   **Dynamic EQ edits must not trigger a global transition-to-dry** and
   changes to disabled DEQ must not interrupt other enabled effects.
5. **RT math:** `run()` sanitizes nonfinite input and output, passes
   unity-level stereo audio to preallocated wet/dry buffers, processes up
   to 2,048 frames per chunk and performs the existing 20-ms crossfade.
   Preserve processing order, native limiter, bypass, allocation, sample
   sanitization and synchronization semantics. Do not add blocking,
   file I/O, logging, or thread creation to this callback.
6. **Interfaces:** preserve exported `ladspa_descriptor`,
   `kPluginId=2780030`, label `viperbass_stereo`, seven existing
   ports (four audio, three legacy Bass controls), and the
   `viper-control.h` layout/status/request serial contract.
   GUI, CLI, output monitor, snapshot helper and packaging changes
   are out of scope for this mechanical extraction.

### Checkpoint 2 guidance (candidate, not file quota)

The startup settings/path parser (`config_directory`,
`saved_ir_path`, `saved_ddc_path`, `load_saved_settings`) is the
first plausible cohesive mechanical extraction. It has clearer inputs
than the worker/processor complex and performs disk access only outside
the callback. Preserve its initialization ordering, accepted configuration
formats and precision precedence; run focused settings/IPC regressions
immediately after the move.

Keep the short shared-control ownership block and LADSPA descriptor in
the main translation unit unless a substantial independent responsibility
emerges. Worker dispatch, native effect setters and processor state are
tightly coupled; do not split by GUI tabs or force an arbitrary file count.
Using an implementation-only header or `.inc` inside the existing C++
translation unit remains an option if it preserves ownership and ABI.

**Checkpoint 1 gate passed.** Next: Checkpoint 2, one reviewed mechanical
change at a time, with targeted compilation and regression tests. Full
developer and live-audio acceptance remain later gates.

## Checkpoint 2 — first mechanical extraction verified (2026-11-04)

**Commit:** `f5d5e1b28a1bcce583c4c0b175d686e7f807fc8d`
(startup settings only; no native DSP implementation or CMake changes).

- Moved `config_directory()`, `saved_ir_path()`, `saved_ddc_path()`,
  and `load_saved_settings()` verbatim into
  `src/backend/startup-settings.inc`; included at their original position
  inside `src/viper-ladspa.cpp`'s existing single C++ translation unit.
  The source diff is reversible byte-for-byte by reinserting the moved
  implementation, so startup parser control flow and settings semantics
  have not been rewritten.
- Main LADSPA source decreased from 1,493 to 1,207 lines; the extracted
  include has 288 lines. The small IPC mapping/refcount block and LADSPA
  descriptor remain in the main file, by design.
- Updated `tests/check-precision-persistence-contract.py` to read
  the extracted implementation and assert the include is present, so the
  existing legacy/precision precedence assertions are retained rather than
  silently bypassed.

**Maintainer executed after a clean fast-forward pull:**

~~~bash
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tests/check-developer.sh
git status --short
~~~

**Results:** native and mock LADSPA targets recompiled and linked at
100%; CTest **4/4 PASS**; **full developer regression suite PASS**,
including precision persistence, mock DSP/IPC, IRS/DDC and sample-rate
checks; all **22 output identity/memory tests PASS**; working tree clean.
No compile error or warning appeared in the submitted build log.
No installed backend was replaced or started by these commands.
The diagnostics from deliberately invalid CLI inputs and missing IRS
fixtures are expected test behavior.

**Acceptance:** first Checkpoint 2 extraction passes automated tests.
Further restructuring remains subject to separately reviewed diffs and
tests. Real-device auditory equivalence remains a later maintainer gate.

## Checkpoint 3 — processor extraction regression (2026-11-04)

**Implementation commit:** `29556cde66736ead005947f9fc0b1c3dcad13f16`.

- Moved the entire `Processor` class verbatim into
  `src/backend/processor.inc` (1,104 physical lines). The parent
  `src/viper-ladspa.cpp` now has 105 physical lines and retains the shared
  control-page owner and all LADSPA hooks/descriptors. The 288-line
  `src/backend/startup-settings.inc` remains unchanged.
- Kept the same anonymous namespace and single C++ translation unit,
  using a local implementation include. Reinserting the class in place
  reproduces the old source **byte for byte**; worker lifetime, native
  effects, `std::try_to_lock` callback behavior, processing order, buffers,
  serial handoffs, and ABI were not rewritten.
- Updated the existing UI/native-effect/DEQ source contract tests to
  inspect the new processor file and assert its inclusion from the main
  LADSPA source. These checks continue to verify actual implementation
  text rather than merely passing because it moved.
- The commit inadvertently reset the executable bit on
  `tests/check-native-effect-routing.py` from 100755 to 100644. This is
  unrelated to DSP and is corrected in the documentation follow-up,
  preserving the original test script permission.

**Maintainer's fresh Arch checkpoint:**

~~~bash
git pull --ff-only
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVIPER_BUILD_DEVELOPER_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tests/check-developer.sh
nm -D build/viper_dsp_ladspa.so | grep ' ladspa_descriptor$'
grep VIPER4LINUX_DEV_VERSION build/generated/viper-build-info.h
git status --short
~~~

**Results:** source pull fast-forwarded; CMake verified all three DSP
patches in the existing private dependency copy; native LADSPA,
mock LADSPA and Qt GUI rebuilt to 100% without compiler warnings/errors
in the supplied output. **CTest 4/4 PASS**, **full developer regression
suite PASS**, including all **22 Audio Output tests**, IPC, mock DSP,
Convolver/IRS and DDC/rate coverage, settings precision, DEQ handoff,
effects, packaging and UI contracts. Git working tree remained clean.
Exported symbol was `T ladspa_descriptor`; generated source-build GUI
version was `1.1.0-29556cde6673` (matches the extraction commit).
The build/test run did not install or restart the real audio backend.

**Acceptance:** this processor-class move has passed automated tests.
Post-extraction live playback and audible equivalence have **not** yet
been verified by this log. Review the complete callback and the
worker/native-configuration ownership as potential next responsibilities;
split only when the resulting source is easier to audit, **not to reach
a particular file count**.

## Checkpoint 4 — callback extraction and automated regression (2026-11-04)

**Implementation commit:** `5c66fb8308838e0f1f135d52f185c93cad415143`.

The entire `Processor::run(unsigned long frames)` member function was
mechanically extracted from `src/backend/processor.inc` into
`src/backend/processor-run.inc`, **included at precisely the old
location inside the Processor class** and compiled in the same C++
translation unit. Reinsertion reconstructs the prior processor
implementation byte-for-byte. No audio-thread synchronization, native
DSP parameters, processing order, sample-rate policy, dry/wet mix,
LADSPA descriptor, CMake target or ViPERDSP source was changed.
Existing native routing and UI source-contract tests now include the
callback source explicitly, preserving their original assertions and
adding the callback include/try-lock checks.

The resulting **four source responsibilities** are:

| File | Physical lines (Git text split) | Owner |
| --- | ---: | --- |
| `src/viper-ladspa.cpp` | 105 | Shared IPC/control owner, LADSPA ABI and entrypoints |
| `src/backend/startup-settings.inc` | 289 | Startup settings, saved IR/DDC paths |
| `src/backend/processor.inc` | 890 | Native DSP state and lifecycle, worker, effect configuration |
| `src/backend/processor-run.inc` | 218 | Complete real-time audio callback |

These line counts include terminal newline splitting; Git may report
one fewer counted line for files without a final blank line.
**Four is the outcome of the inspected natural boundaries**, not a
quota. No further splitting of the worker/native effect setters is
currently justified merely to reduce file size.

**Maintainer ran from the fresh Arch checkout:**

~~~bash
git pull --ff-only
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVIPER_BUILD_DEVELOPER_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tests/check-developer.sh
nm -D build/viper_dsp_ladspa.so | grep ' ladspa_descriptor$'
git status --short
~~~

**Results:** fast-forwarded to the callback commit; CMake verified the
three private ViPERDSP corrections, rebuilt both LADSPA variants and
the GUI to 100% without compile errors or warnings in the supplied
log; **CTest 4/4 PASS**; **full developer suite PASS** including
all **22 Audio Output tests**, native DSP/IPC, effect routing,
Dynamic EQ, Convolver/IRS, DDC, sample rates, GUI/CLI contracts,
source and package/release contracts. Export remains
`0000000000017580 T ladspa_descriptor`; Git working tree clean.
Test diagnostics for invalid settings and missing IRS fixtures were
intentional. This source-build test did not launch or replace the
installed audio backend.

**Acceptance status:** the callback move has passed full developer
regressions and the clean independent Arch source build plus deterministic
CMake package-staging parity described below. This is not an executed
`makepkg`/pacman package build, and audio equivalence still requires the
Checkpoint 5 maintainer gate. No merge to `main` yet.

### Checkpoint 4 independent clean-build and staging acceptance (2026-11-04)

The maintainer fast-forwarded to `cd688e531bc388b68ee7e4314f1cbcd3840d4b02`,
then configured an **independent, new** `build-backend-final/` directory
using GCC/G++ 16.2.1, CMake Release,
`VIPER_BUILD_DEVELOPER_TESTS=ON`,
`CMAKE_INSTALL_PREFIX=/usr`,
`VIPER_ENABLE_SYSTEM_INSTALL=ON` and
`VIPER_SYSTEM_INSTALL_LAYOUT=arch`. CMake copied and patched the pinned
ViPERDSP dependency in a private build directory (verified LUFS speed,
Stereo Imager preallocation and Multiband Compressor preallocation), compiled
all production and mock targets to 100% without reported compiler
warnings/errors, and passed **CTest 4/4**.

The maintainer then ran the independent read-only-host staging parity
test, `bash tests/check-cmake-install-parity.sh build-backend-final`. It
installed the newly compiled artifacts into two isolated `/tmp`
`DESTDIR` trees and checked file hashes, permissions, symlink targets
and directory entries. **PASS: identical 35 files and 2 symlinks**;
all staged files remained under their private test roots.
No system install, daemon restart or audio output modification occurred.

The independently compiled plugin still exports
`0000000000017580 T ladspa_descriptor`. The `git status --short`
output was empty (clean working tree). The latest full standalone
developer suite had already passed at the preceding callback commit;
this *separate clean-build run* explicitly ran all four CTest groups.

**Checkpoint 4 gate:** complete for source compilation, automated
regressions, safe deterministic Arch CMake staging and symbol export.
An actual Arch `makepkg` artifact, if required for release, is a
separate package-production test; CMake parity is not that test.
Remaining: Checkpoint 5 real PipeWire live checks and maintainer
approval before merging. No additional splitting is indicated by
the successful four-file architecture.

### Checkpoint 5 — initial live-audio confirmation (2026-11-04)

The maintainer launched the GUI reporting **`1.1.0-5c66fb830883`**,
which identifies the last DSP source change (the mechanically extracted
real-time callback). The later commits only changed documentation.
The launcher **reused the existing source-checkout backend** instead
of interrupting ongoing playback, and the maintainer reported that
**effects were still processing**.

**Additional maintainer live tests (2026-11-04):**
IRS impulse-response changes continue to work, VDC profile changes
continue to work, and settings remain remembered. In this later
test the backend had **not been running** and started properly.
This checks the startup path separately from the earlier successful
matching-fingerprint backend reuse during uninterrupted playback.
These are the maintainer's observed outcomes; no new DSP changes
were made for these checks.

**Status:** live playback/effects, GUI/backend reuse, cold backend
startup, IRS/VDC changes and retained settings have positive maintainer
confirmation. Still outstanding for a complete Checkpoint 5 sign-off:
physical output switching/Bluetooth reconnect and per-device recall,
intentional backend replacement/restart as appropriate, a focused
multi-effect toggle/bypass pass and any required reboot and
sample-rate-specific checks. The source-checkout and installed system
package have different fingerprints; keep the installed package
untouched. Obtain explicit maintainer approval before merging to `main`.

## History and release notes

The preceding GUI refactor was merged as PR #1 on 2026-11-03; the feature
inventory, tests and GUI source-size snapshot are retained in
`docs/DEVELOPMENT.md`. Keep the new backend refactor changelog separate
until it is merged or accepted for a release. The earlier GUI planning
document is preserved by its original Git commits, not duplicated as
another active plan.

**Next action:** Checkpoint 5 live-audio acceptance on the source
checkout: confirm backend reuse and intentional restart behavior,
real playback across output switches/Bluetooth reconnect, GUI/CLI
settings and presets, IRS/DDC loading and 44.1/48/96-kHz processing
as supported. Record exceptions before the maintainer's explicit
merge decision. Do not change the installed system package as part
of this test, and do not add source files without architectural need.
