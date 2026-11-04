# CMake modernization: baseline and migration inventory

**Branch:** `refactor/cmake-modernization`  
**Reference revision:** `a84a433fc6b0f39cdf718e7ae127badba10df424` (branch point on `main`).  
**Status:** Static inventory and maintainer's Arch CTest and patched-DSP native build baselines passed (2026-11-04). Stage-manifest parity and live audio tests remain open.  
**Purpose:** Preserve the existing behavior and safety properties while relocating ownership to CMake/CTest.

This combines source-level inventory and maintainer-supplied command output. Passing the native build does not establish package staging parity or live PipeWire/audio behavior.

## Maintainer-executed baseline evidence (2026-11-04)

**CTest integration:** On Arch Linux, the checkout reports the pinned ViPERDSP submodule at `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`. With GCC 16.2.1 and Python 3.14.9, `cmake -S . -B build-ctest-inventory -DVIPER_BUILD_DEVELOPER_TESTS=ON` configured successfully. `ctest --test-dir build-ctest-inventory --output-on-failure` returned **1/1 passed** in 23.28 seconds, exercising the original isolated developer suite through the transitional CTest wrapper.

**Four-group CTest verification:** After the CTest dispatcher was split into separate package, native mock, application-contract and release-contract groups, `ctest -N` discovered four cases and all **4/4 passed** on Arch in 22.67 seconds (0.78 s, 19.15 s, 1.84 s, 0.89 s, respectively). Developer tests remain opt-in; the OFF path needs its separate no-tests confirmation.

**Legacy prepared native baseline:** The maintainer separately prepared an external ViPERDSP copy using all three existing Python patch scripts, configured `build-native-baseline` with `VIPER_BUILD_DEVELOPER_TESTS=OFF`, and compiled all production targets. All patches reported successful application, the build reached 100% without visible errors/warnings, and `nm -D` reported an exported `T ladspa_descriptor`.

**New CMake-owned preparation baseline:** After `cmake/PrepareViPERDSP.cmake` was introduced, a completely separate `build-cmake-prepared` was configured with **no manual patches** and developer tests OFF. CMake reported a private patched source directory under that build tree, applied each of the three verified fixes, and built ViPERDSP plus all four production targets to 100% without visible errors/warnings. `nm -D build-cmake-prepared/viper_dsp_ladspa.so` produced `0000000000017580 T ladspa_descriptor`. `git -C third_party/ViPERDSP status --short` produced no output, confirming no tracked submodule modifications.

**Incremental-build verification:** Following a branch update, CMake reported `Reusing verified private ViPERDSP source` and all three fixes already present. ViPERDSP and LADSPA were reused without recompilation; the GUI rebuilt because the Git-derived About revision changed.

**Unified-source archive verification:** A test archive under `/tmp` configured without Git metadata, with CMake's Git lookup disabled. Private DSP preparation applied all three fixes. The built `viper_dsp_ladspa.so` exported `0000000000017580 T ladspa_descriptor`. The unused Git-discovery CLI variable is expected because no `find_package(Git)` ran. Git remained installed on the host; this is a metadata/discovery independence test, not proof of an absent Git executable.

**Still unverified:** Intentional patch-failure handling, staged Arch/Debian package parity, and live PipeWire/audio acceptance.

## Existing responsibilities

| Component | What it currently owns | Intended destination |
| --- | --- | --- |
| `CMakeLists.txt` | C/C++17 targets `viper_dsp_ladspa`, `viper-gui`, `viper-ctl`, `viper-output-state`; Qt6, Threads, ViPERDSP target; generated About version | Keep CMake as build authority; add verified DSP prep, optional CTest and install rules |
| `cmake/Version.cmake` | Git-tag or release-archive version/revision | Retain; test both archive and Git checkout |
| `install.sh` | Arch-like dependency probes; isolated tests before and after; pinned submodule checkout and exact commit check; private three-script patching; native CMake build and LADSPA export check; source routing config; local command symlinks | Move portable checks, patch preparation, tests and installs into CMake/CTest; preserve safe source-checkout commands before retirement |
| `patches/fix_lufs_speed.py` | Verified replacement for LUFS speed clamp; rejects unknown upstream constructor | Reuse fail-closed against private copy; do not weaken validation |
| `patches/preallocate_stereo_imager.py` | Preallocates 4096 interleaved floats for 2048-frame RT chunk; refuses unknown source | Reuse fail-closed against private copy |
| `patches/preallocate_multiband_compressor.py` | Preallocates five band buffers before audio callback; refuses unknown source | Reuse fail-closed against private copy |
| `tests/check-package.sh` | Offline package contracts, launcher, simulated Arch manifest, Python per-output tests | Expose as fast CI/package smoke gate, preferably through CTest |
| `tests/check-mock.sh` | Direct `g++` builds of isolated LADSPA/Qt control mocks and C++ file readers, Python contracts, alternate sample rates, temporary homes | Initially CTest wrapper, eventually named CMake test binaries where appropriate |
| `tests/check-developer.sh` | Full suite including package and mock tests, GUI/state/settings/precision/preset and release contracts | Optional developer CTest entry point; preserve coverage |
| `packaging/arch/PKGBUILD.stable.in` | Private DSP source prep in `prepare()`, CMake in `build()`, fast checks + LADSPA export in `check()`, staged install via shell | Consume CMake prep/CTest and `cmake --install` after verified parity |
| Published AUR `viper4linux-re-git` recipe | Separate maintainer-owned Git repository; package not duplicated in this source tree | Coordinate separately after stable packaging parity |
| `packaging/arch/install-package.sh` | Arch staging path/permission/symlink manifest and checks | Translate into CMake install rules; retain as reference and test oracle during transition |
| `scripts/build-native-deb.sh` | Distro-specific dependencies; DSP prep, build, mock tests, manual Debian staging, shlibs/deb package and install smoke checks | Keep necessary distro packaging; replace duplicated app file installation with CMake staging |
| `scripts/test-stable-arch.sh` and `prepare-stable-release.yml` | Actual Arch makepkg, full developer suite, release archive, Ubuntu/Mint and Debian/LMDE jobs | Reuse new entry points without dropping coverage |
| Source `viper4linux` + `scripts/start.sh` | Source-relative launcher, `build/` paths, local routing-config, PipeWire client lock, backend fingerprint | Preserve usable source-checkout launches; do not assume install prefix |
| Packaged `packaging/arch/viper4linux`, `start-backend`, `viper4linux-output` | Absolute `/usr` paths, system LADSPA and runtime config, native Python module helpers | Preserve under system prefix; design prefix-aware behavior separately |

## Findings that require explicit resolution

### Direct CMake currently does not guarantee patched DSP

The default `VIPERDSP_SOURCE_DIR` in `CMakeLists.txt` is `third_party/ViPERDSP`. The three real-time compatibility changes are applied **only by prep paths** such as `install.sh` and the package scripts, which pass a private patched directory as an override.

Therefore, **do not present plain `cmake -S . -B build` as a safe equivalent to the installer yet**. The Checkpoint 2 work must make private patched dependency preparation the normal, guarded CMake path and reject unsafe or mismatched sources. Protect the tracked submodule, offline unified releases, and incremental rebuilds.

### Source launcher and installed launcher use different layouts

`scripts/start.sh` expects the source build in `build/` and a source-relative routing configuration. Installed launchers expect `/usr/bin`, `/usr/lib/ladspa`, `/usr/lib/viper4linux`, and `/usr/share/viper4linux`. CMake installation rules need to avoid shipping a broken wrapper or pretending arbitrary `--prefix` works without path changes. The old source installer also creates local `~/.local/bin` symlinks with collision safeguards.

### Arch and Debian artifacts are intentionally not identical

The Arch install script stages executable wrappers, `v4l-cli` and manual-page aliases, Python modules, icons, config, and docs under `/usr/share/doc/viper4linux`, plus licenses in `/usr/share/licenses/$pkgname`. The Debian script stages related runtime files, but docs under `/usr/share/doc/viper4linux-re`, a Debian `copyright`, and compressed `.1.gz` manuals.

The baseline should compare **per-distribution manifests and behavior**, not require byte-for-byte identical paths where packaging policy differs. CMake can own common app installation; distro-specific finalization may remain in package tooling.

### Avoid treating regression scripts as a full native quality test

`tests/check-mock.sh` explicitly uses a simplified DSP mock and isolated IPC. `tests/check-developer.sh` adds numerous contracts and state tests but not an audible test of the real ViPERDSP library in PipeWire. The CMake migration must preserve tests without overstating what they prove.

### Prepare for an additional safety distinction

An opt-in CTest option is harmless to the normal production build **only if it does not implicitly activate developer-only dependencies or native mock work**. The first step may register the existing suite in a gated CTest wrapper while the more extensive conversion of test compilation to CMake targets awaits Checkpoint 3.

## Baseline evidence checklist (completed items noted above)

1. Confirm `git rev-parse HEAD` and pinned `git rev-parse HEAD:third_party/ViPERDSP`; record CMake/compiler/Qt6/Arch versions.
2. Run current `bash tests/check-package.sh`, `bash tests/check-developer.sh`, and a clean isolated native production build using the *existing patched-source preparation path*. Preserve stderr, warnings, and exact command lines.
3. Capture `nm -D .../viper_dsp_ladspa.so` LADSPA export, module/plugin identity, and native program filenames.
4. Generate and capture staged Arch package file list and permissions. For Debian, retain both supported target artifacts and their documentation, man page, and dependency metadata.
5. Test missing submodule, incorrect upstream patch context, repeated build, attempted host-stage destination, and test OFF versus ON; do not deliberately damage the working system or audio profile.
6. Compare functional runtime behavior on Cinnamon/PipeWire with speakers, EDIFIER Bluetooth, IRS/DDC, output-memory restoration, GUI single instance and tray, backend reuse, and CLI. This evidence belongs at final acceptance, not a claim that static inventory completed it.

## Initial, non-destructive contributor verification

The gated CTest entry point is now available. A maintainer with Arch build dependencies can run:

```sh
git switch refactor/cmake-modernization
git submodule update --init --recursive
cmake -S . -B build-ctest-inventory -DVIPER_BUILD_DEVELOPER_TESTS=ON
ctest --test-dir build-ctest-inventory -N
ctest --test-dir build-ctest-inventory --output-on-failure
```

This exercises CTest discovery and the existing isolated test script. It **does not** compile the real production backend or replace current native build acceptance. The new default CMake path now prepares a private patched copy; runtime acceptance and installation are still separate tests.

## Remaining to close Checkpoint 1

- Native CTest and clean native build baselines are recorded above; capture package staging diff and live Arch/PipeWire audio evidence.
- Inventory any remaining release/AUR-specific behavior revealed by local package runs.
- Approve source-vs-installed launch behavior and exact intended prefix support before installation-rule migration.
