# Arch Linux packaging and installation

There are three supported paths; they do **not** share a PKGBUILD:

- **Direct source build:** initialize the pinned ViPERDSP submodule,
  then `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`,
  `cmake --build build`, and
  `cmake --build build --target viper_source_check`.
  CMake prepares the private DSP dependency, generates the build-local routing
  configuration and verifies the native LADSPA export. This does not invoke
  `makepkg`, install a system package or launch audio. Optional shell aliases
  require a separate explicit `viper_source_links` CMake target. See the
  README and `docs/DEVELOPMENT.md`.
- **Development VCS package:** `viper4linux-re-git` is published and its
  authoritative PKGBUILD is maintained separately in the AUR. Build/update
  it using the AUR Git checkout or an AUR helper. The obsolete duplicate
  `packaging/arch/PKGBUILD` was removed from this source repository.
- **Stable release package:** GitHub Actions generates a PKGBUILD from
  `packaging/arch/PKGBUILD.stable.in` and a pinned, unified source archive.
  It runs an actual Arch package build; generation does not publish to AUR.

On `main`, **CMake is the only application installation engine.** The stable Arch PKGBUILD uses `cmake --install`
with a guarded `DESTDIR`; it does not maintain a second file-copy list or
manually patch ViPERDSP. The AUR compatibility installer has been retired.

The maintainer updated the independently maintained `viper4linux-re-git`
PKGBUILD locally to let CMake validate the pinned ViPERDSP checkout, prepare
and patch a private copy, compile and stage all package files directly.
A complete `makepkg -Cf` test on Arch (November 4, 2026) passed:
the private source was prepared afresh, all three patches applied, the package
smoke suite and 22 output-memory/identity tests passed, and the main and
debug pacman package archives were generated. The modernization branch
has since merged into `main`; the AUR checkout should track `main` and
regenerate `.SRCINFO` before publication. No AUR release or system
installation was performed by that test.

## Dependency and package regression checks

`tests/check-packaging-contract.py` checks the stable template's runtime
and build dependencies, CMake dependency discovery (including Qt6 Widgets and
Network), the README's source-build package list, pinned ViPERDSP handling,
downstream patches, sole-source CMake staging and backend
launcher contracts. It is invoked by both the standalone `tests/check-package.sh` script
and the CTest `package` group used in stable package `check()` and the
full developer suite. It is intentionally
compatible with a unified release archive, which has no `.gitmodules`.
`tests/check-arch-install.py` independently stages a mock stable and VCS
package directly through CMake, validating paths, permissions, dynamic
Python files, failure behavior and stable/VCS custom notice directories. A separate
real-build staging test compares the complete manifests of two independently
staged CMake installations.

The stable PKGBUILD builds the complete offline CTest suite under CMake's
standard `BUILD_TESTING=ON` default. Its `check()` runs
`ctest --output-on-failure --no-tests=error`, reporting every independently
registered test and failing when any check fails (or none are found).
The former 12-test fast-only package gate is no longer the default.
All native mocks, DSP checks, CLI and GUI contracts operate without live
PipeWire or the user's installed audio backend. The `git` build dependency
supports offline regression checks that create temporary repositories.
Use `-DBUILD_TESTING=OFF` only to intentionally omit test targets entirely;
`makepkg --nocheck` skips running `check()` but still compiles test targets.
The older `tests/check-package.sh` remains available to existing callers.

Arch release preparation relies on the full CTest run already performed by
`makepkg`; it does not rerun those tests in a second build pass. Both native
Debian-family builds likewise run the complete CTest suite before staging.
The existing source-staging, LADSPA export and disposable-container installation
checks remain separate. The separate published AUR VCS recipe remains
maintainer-owned and **does not automatically adopt this new CTest gate**;
its PKGBUILD and `.SRCINFO` must be reviewed separately. These repository
tests do not claim to validate that recipe's exact `pkgver()` or metadata.

For an AUR update, regenerate `.SRCINFO` in that package's Git checkout
using `makepkg --printsrcinfo > .SRCINFO` and push both changed files.
Do not use `pacman --overwrite` to bypass package file conflicts.

The per-output feature is initially **disabled**, and enabling it in the GUI
Preferences does not reset the current sound. It remembers distinct physical
hardware (including two same-model Bluetooth headsets), restores the exact
DSP state when returning to an output, and continues working after Quit GUI
through the launcher-supervised monitor. The native state helper validates
complete snapshots and filesystem-dependent IRS/VDC paths before applying
anything. Use `viper4linux-output --enable-memory` and
`viper4linux-output --disable-memory` for command-line control. The
`viper4linux-output --assign-preset FILE --target OUTPUT_KEY` command asks
the active monitor to assign an existing full-effect preset. Inactive-output
assignment does not modify currently playing audio.

After updating an AUR checkout, regenerate `.SRCINFO` with
`makepkg --printsrcinfo > .SRCINFO` and push both files. Stable AUR
publication happens separately with the next release.

Personal settings, presets, and IRS/VDC files remain in user directories and
should survive package replacement. If an old source checkout installed a
launcher symlink, use `type -a viper4linux` to ensure it does not shadow
`/usr/bin/viper4linux`.

## Package variants

| Package | Source | Versioning |
|---|---|---|
| `viper4linux-re` | Published versioned source tarball | Fixed release number (`1.1.0-1`, etc.) |
| `viper4linux-re-git` | Published Git remote | Commit-derived VCS `pkgver()` |

The stable package should not need a Git checkout to build; its sources,
including the pinned DSP dependency, are assembled into a separate
unified-source release archive. The `-git` package follows the latest published
repository commit rather than a Desktop path. Before publication, review
reciprocal conflicts, upstream licenses, proper sources/hashes, dependencies,
and reproducible builds. The VCS PKGBUILD is maintained in its separate AUR Git repository and is
not duplicated here.

Original application code is copyrighted to Gerald Frank (`username227`)
and is licensed under GPL-3.0-only; see `COPYRIGHT` and `LICENSE`.
Arch supplies the standard GPL-3 text in its `licenses` package, so no
duplicate GPL `LICENSE` is installed under the package's `/usr/share/licenses/`
directory. The custom `ViPERDSP-NOTICE.md` stays in
`/usr/share/licenses/viper4linux-re[-git]/` because upstream DSP
terms remain separate (`LicenseRef-ViPERDSP`). See `THIRD-PARTY-NOTICES.md`.
This GPL grant does not override upstream redistribution restrictions.
Publishing a unified source archive is a separate, deliberate distribution
step, not part of GitHub's automatic source archives.

## Prepare a stable release (GitHub Actions)

The single manually dispatched [Prepare stable release](../.github/workflows/prepare-stable-release.yml)
workflow checks out the selected commit and its **recorded ViPERDSP submodule**,
then builds the unified source, the native Arch package, an Ubuntu 24.04/Mint
22.x `.deb`, and a Debian 13/LMDE 7 `.deb`. It does not create tags, publish
releases or binary packages, or update the AUR.

1. Open **Actions → Prepare stable release → Run workflow**. Select the release
   source revision and enter the target version (without `v`). This single
   version input is written into `RELEASE-VERSION` inside the unified archive;
   the rendered PKGBUILD and release checks use that same input. No CMake
   version edit is needed. In a Git checkout, CMake identifies development
   builds using the nearest reachable `vMAJOR.MINOR.PATCH` tag and Git SHA;
   archive builds use `RELEASE-VERSION` and `RELEASE-REVISION`.
2. The Arch preparation job generates the unified source, SHA-256, stable
   `PKGBUILD` and `.SRCINFO`; it builds and tests the package from the generated
   tarball, not the Git checkout.
3. The Ubuntu/Mint and Debian/LMDE jobs independently compile and install-test
   native `.deb` packages using the exact same unified source archive.
4. After **all three builds** pass, an assembly job verifies provenance,
   source/archive checksums and all three native package hashes. Download the
   `viper4linux-re-VERSION-complete-release-candidate` ZIP containing the
   unified archive, main Arch package, both `.deb` files, a single
   `SHA256SUMS` covering all four artifacts, `PKGBUILD`, `.SRCINFO` and
   `SOURCE-COMMIT`. The optional Arch debug-symbol
   package is not included.
5. Confirm `vVERSION` points to **exactly** `SOURCE-COMMIT`, then create the
   GitHub release and attach the unified tarball, three native binaries, and
   the single `SHA256SUMS` file. GitHub's automatic source archives do not include
   the recorded DSP submodule.
6. Publish/update AUR recipes **manually** in your separate AUR Git checkout
   and regenerate `.SRCINFO` before pushing.

The workflow refuses to prepare a version whose existing tag points to a
**different** source commit. Rerun after any code changes; archive checksums
will change. Artifacts expire after their configured retention period.

### Run the release preparation locally

With the submodule initialized and clean, the same generation logic works
without GitHub Actions:

```bash
mkdir -p release-candidate
bash scripts/prepare-stable-release.sh 1.1.1 "$PWD/release-candidate"
```

The resulting source archive contains the pinned, unpatched upstream DSP.
The stable PKGBUILD now lets CMake verify the upstream revision and apply
all three DSP compatibility patches to a **private** build copy. The source
archive and its upstream dependency remain unmodified. The full Arch packaging test requires an Arch environment with pacman,
Qt6, CMake and the other declared dependencies.
