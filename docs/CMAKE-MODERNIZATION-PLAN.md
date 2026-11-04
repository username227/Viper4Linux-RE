# CMake / CTest modernization plan

**Status:** In progress on implementation branch; not merged.  
**Implementation branch:** `refactor/cmake-modernization`  
**Priority:** Complete before the native LADSPA backend modularization.  
**Scope:** Build preparation, optional developer tests, installation, and packaging integration. **No intentional DSP, GUI, IPC, or routing behavior changes.**

## Why this work comes first

Make the repository approachable to Linux developers using familiar CMake and CTest commands. CMake should become the authoritative definition of how ViPER4Linux-RE is configured, built, tested, and installed. Packaging should consume the same installation rules. Retire `install.sh` **only after** its safety checks, private ViPERDSP patching, and source-checkout functionality are replaced and verified.

The backend refactor is prepared separately in `refactor/backend-modularization`; its checkpoints remain open while this work is developed. The screenshot/README gallery is independent.

## Current baseline (verified from `main` when this plan was written)

- `CMakeLists.txt` (minimum CMake 3.20; C++17) already compiles `viper_dsp_ladspa`, `viper-gui`, `viper-ctl`, and `viper-output-state`. It consumes `VIPERDSP_SOURCE_DIR` and version information from `cmake/Version.cmake`. It has no application install rules or CTest registration yet.
- `install.sh` checks tools/dependencies, runs isolated regressions, verifies the recorded ViPERDSP submodule commit, patches a **private** DSP copy, configures/builds with CMake, validates the LADSPA export, prepares the local routing configuration, and manages local launcher symlinks.
- `tests/check-mock.sh` builds isolated DSP/CLI test executables with direct `g++` calls; `tests/check-developer.sh` runs the larger developer suite; `tests/check-package.sh` runs short, offline package smoke checks.
- `packaging/arch/install-package.sh` is the current system package file-layout reference. The stable Arch PKGBUILD stages through that script; the VCS PKGBUILD is maintained separately in the AUR. Release CI also builds Ubuntu 24.04/Mint 22.x and Debian 13/LMDE 7 native packages.
- `scripts/start.sh`, the source launcher, and the packaged launchers currently rely on different source-tree versus system paths. These must be checked before installation rules are considered complete.
- The pinned ViPERDSP source and three downstream patch scripts are critical build inputs. A unified source archive must build offline without relying on Git metadata or unexpected network access.

These are **observations, not completed migrations**. Inventory every relevant script and release job at Checkpoint 1 rather than assuming this list is exhaustive.

## Intended public build interface

The default is a normal production build; development tests are explicitly opt-in:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Developers may enable regression targets with:

```sh
cmake -S . -B build-dev -DCMAKE_BUILD_TYPE=Debug \
  -DVIPER_BUILD_DEVELOPER_TESTS=ON
cmake --build build-dev --parallel
ctest --test-dir build-dev --output-on-failure
```

Proposed CMake cache option:

```cmake
option(VIPER_BUILD_DEVELOPER_TESTS
       "Build and register the developer regression suite" OFF)
```

The examples describe the **target workflow**, not commands guaranteed to work before the migration. Preserve useful short, mandatory package checks as a separate CI/packager gate if appropriate. Enabling developer tests makes targets available; it must not silently run expensive tests on every ordinary build. CI and explicit developer commands enforce the required test-before-release order.

Production installation should ultimately be driven by `cmake --install` with `GNUInstallDirs` conventions and package staging via `DESTDIR`, for example:

```sh
DESTDIR="$pkgdir" cmake --install build --prefix /usr
```

Do not run that illustrative command against the current tree yet; there are no application install rules. User-prefix installs are a goal only if all launchers and runtime paths can actually support them. Never overwrite live user configuration, preferences, IRS/VDC assets, or an unrelated executable during migration.

## Five acceptance checkpoints

### 1. Full build/install inventory and baseline

- Record the current `main` commit, submodule commit, build versions, compiler requirements, and a clean out-of-tree native build.
- Catalogue each responsibility of `install.sh`, CMake, `tests/`, `packaging/arch/`, Debian build scripts, launchers, and release Actions.
- Capture baseline staged Arch/deb manifests, symlink targets, file permissions, LADSPA ID/export, version metadata, launcher paths, and source versus installed behavior.
- Establish the current failure cases: missing dependency, missing or wrong DSP submodule, malformed/unapplicable patch, empty build tree, invalid install stage, and absence of audio hardware.
- Run the existing package/developer suites before changing any behavior. Distinguish remotely runnable mock tests from local Arch/PipeWire/audio acceptance.

**Pass:** reproducible baseline and responsibilities/compatibility matrix documented. No claim of an all-green native build without execution evidence.

### 2. CMake configuration and private DSP preparation

- Move deterministic dependency/configuration checks under CMake using normal `find_package`, `find_path`, `find_program`, or equivalent mechanisms; preserve clear diagnostic messages.
- Keep the submodule pinned and unmodified. Build from a private patched copy and fail safely on mismatched or unexpected patch inputs.
- Account explicitly for both Git checkouts and offline unified source archives. No implicit update to the upstream commit and no hidden network fetch.
- Make preparation idempotent and rebuildable after source/patch changes; avoid repeated patch application, stale generated output, and mutation of source files.
- Retain the real-time correctness intent of the three downstream fixes; test a clean build and an intentionally failing preparation case.

**Pass:** CMake builds the same production binaries from both source forms; wrong/missing upstream inputs fail clearly and safely.

### 3. Optional developer tests through CTest

- Implement `VIPER_BUILD_DEVELOPER_TESTS` defaulting to `OFF`; test binaries and heavy checks must not be required for the ordinary production path.
- Register meaningful individually named CTest cases, retaining isolation of IPC identifiers, temporary settings/configuration homes, and test artifacts.
- Initially wrap existing proven shell/Python tests where useful; then replace direct `g++` calls in the mock suite with CMake test targets where it materially simplifies maintenance.
- Preserve the distinction between isolated native/mock tests, quick package contracts, CI checks, and human audible/hardware acceptance. Do not pretend CTest proves Bluetooth or sound quality.
- Verify `ctest --output-on-failure`, selective test execution, propagation of nonzero exit codes, and that disabling tests omits developer-only build requirements.
- Ensure release CI and developer instructions use one documented test entry point, while genuinely platform-specific tests remain appropriately conditional.

**Pass:** default build has no developer suite, opt-in tests run and report correctly, and existing regression coverage is not silently lost.

### 4. CMake installation rules and packaging parity

- Add installation rules for all four production artifacts and required runtime assets: Python modules, launchers, CLI aliases, LADSPA module, PipeWire filter-chain configuration, desktop file, icons, manuals, license/notices, and documentation.
- Preserve the current Arch staged manifest from `packaging/arch/install-package.sh`, including executable permissions, symlinks, dynamic Python runtime module discovery, and the currently used system paths.
- Use `GNUInstallDirs` and explicit runtime path discovery rather than relying on the source checkout's working directory; check both system package and intended local-prefix behavior.
- Convert stable Arch and Debian packaging to stage via CMake install **only after** exact-tree comparisons pass. Coordinate the separately maintained AUR VCS PKGBUILD rather than assuming it updates automatically.
- Preserve clean upgrades, command entry points, user-owned settings, service/monitor lifecycle, and avoidance of accidental host installation. Validate staging destinations defensively.
- Do not broaden scope to CPack or unrelated packaging redesign unless a concrete benefit and tests justify it.

**Pass:** staged native Arch and Debian packages match approved runtime behavior and file layout; no unowned or unexpectedly moved files.

### 5. Release, source-checkout, and hardware acceptance; installer retirement

- Validate clean Git checkout with recursive submodules, offline unified release archive, normal build with tests OFF, opt-in developer build with tests ON, and repeated incremental build.
- Exercise CI: stable Arch package, Ubuntu/Mint deb, Debian/LMDE deb, published AUR VCS recipe coordination, version and release-metadata consistency.
- Validate local launch/relaunch, GUI single instance/tray, CLI controls, smart routing, output memory, persistent configuration, IRS/DDC, EQ/DEQ, physical speakers and EDIFIER Bluetooth output; check backend reuse/replacement and reconnects.
- Verify invalid configuration and missing inputs fail with actionable errors. Confirm CMake installation is safe under `DESTDIR` and user-level installs do not overwrite unrelated commands.
- Only after the maintainer accepts parity: update README, developer/packaging/release docs, source-launcher help text, and workflow references; remove `install.sh` and obsolete packaging helpers/tests **when no longer referenced**.
- Keep the backend refactor separate until this build modernization has passed and the maintainer approves its merge.

**Pass:** maintainer-approved test evidence, reproducible packages, no regressions, no surviving `install.sh` dependencies, and explicit approval before merge.

## Guardrails and workflow

- Implementation changes belong on `refactor/cmake-modernization`; this plan is also visible on `main` for prospective contributors.
- Prefer small, reviewable commits by responsibility. No arbitrary module quota, speculative feature additions, broad DSP cleanup, or changes to effect processing order.
- Keep `install.sh` working throughout the transition and use it as a known-good reference until the final checkpoint.
- CMake is the build/install authority; a small, clearly justified development or packaging wrapper may remain if it performs operational tasks not suited to CMake. **The goal is to retire the present installer, not prohibit every shell script.**
- Do not represent planned commands, CI configurations, or package parity as already implemented or tested.
- Merge only after a documented comparison and the maintainer's explicit acceptance.

## Progress

| Checkpoint | Status |
| --- | --- |
| 1. Inventory and baseline | **In progress:** source-level ownership inventory recorded in [CMake baseline](CMAKE-BASELINE.md); native builds and package manifests not yet captured |
| 2. CMake configuration and DSP preparation | **In progress:** first clean Arch CMake-managed private DSP build passed; incremental rebuild, invalid-input failure, and offline archive tests remain pending |
| 3. Optional CTest developer suite | **Initial scaffolding only:** `VIPER_BUILD_DEVELOPER_TESTS=OFF` default; `ON` registers existing full developer script under CTest, pending native checkout tests |
| 4. CMake installation and packaging parity | Not started |
| 5. Full acceptance and installer retirement | Not started |

### Branch work log

- Inventory reviewed `CMakeLists.txt`, `install.sh`, three fail-closed patch scripts, mock/package/developer tests, source and packaged launchers, stable Arch staging, Debian assembly, and release workflows. See [CMAKE-BASELINE.md](CMAKE-BASELINE.md).
- Added the optional CTest entry point without changing native production targets or default build requirements. The first CTest registration wraps `tests/check-developer.sh`; converting test executables into native CMake targets is later work, **not** yet complete.
- Independently checked the new CMake gating and test syntax in an isolated minimal CMake project: tests OFF discovered 0; tests ON discovered 1 and ran a **stub** successfully. This is a syntax/gating check **only**, not a native Viper4Linux regression pass.
- **Maintainer's Arch CTest verification (2026-11-04):** Branch `refactor/cmake-modernization` at pinned ViPERDSP commit `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`; `cmake -S . -B build-ctest-inventory -DVIPER_BUILD_DEVELOPER_TESTS=ON` succeeded with GCC 16.2.1 and Python 3.14.9. `ctest --test-dir build-ctest-inventory --output-on-failure`: **1/1 passed** (`viper_developer_regressions`), elapsed 23.28 seconds. The suite runs under the legacy wrapper; finer-grained CTest registration is not yet implemented.
- **Native baseline passed (maintainer, 2026-11-04):** the three verified DSP patches were applied manually to a private copy; clean Release CMake build with developer tests OFF compiled all four application targets and the real ViPERDSP static library; `ladspa_descriptor` was confirmed with `nm -D`. **This is the pre-migration baseline, not yet a test of CMake-owned patch preparation.**
- Added `cmake/PrepareViPERDSP.cmake` and wired normal CMake configuration through a private, fingerprinted and validated source copy. Patch scripts remain standalone Python utilities (not embedded Python source). **Maintainer's clean Arch run of `build-cmake-prepared` passed** without manual patch steps: all four native outputs built, `ladspa_descriptor` exported, and the tracked submodule remained clean. Safe reconfigure/rebuild, archives, and intentional failure cases are **not yet verified**.
- Staged package parity and live PipeWire acceptance have not been run in this branch yet.
