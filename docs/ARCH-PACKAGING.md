# Arch Linux packaging and installation

There are three supported paths; they do **not** share a PKGBUILD:

- **Direct source build:** `./install.sh` checks for required commands,
  headers and Qt6 components, initializes the pinned ViPERDSP submodule,
  patches a private copy and builds with CMake. It does not invoke `makepkg`
  or read any PKGBUILD. Install the prerequisites listed in the README.
- **Development VCS package:** `viper4linux-re-git` is published and its
  authoritative PKGBUILD is maintained separately in the AUR. Build/update
  it using the AUR Git checkout or an AUR helper. The obsolete duplicate
  `packaging/arch/PKGBUILD` was removed from this source repository.
- **Stable release package:** GitHub Actions generates a PKGBUILD from
  `packaging/arch/PKGBUILD.stable.in` and a pinned, unified source archive.
  It runs an actual Arch package build; generation does not publish to AUR.

On `refactor/cmake-modernization`, the **stable** Arch package uses CMake's
private DSP preparation and guarded `DESTDIR` staging by default. Set
`VIPER_ARCH_STAGING_BACKEND=legacy` only to compare with the old
`packaging/arch/install-package.sh` reference. CMake automatically discovers
runtime Python modules from `src/viper_*.py`.

The separately maintained `viper4linux-re-git` AUR recipe has not been changed
by this migration and may still use the legacy staging helper. It needs an
independent update in the maintainer's AUR checkout after final acceptance;
neither this branch nor the GitHub Actions jobs publish anything to the AUR.

## Dependency and package regression checks

`tests/check-packaging-contract.py` checks the stable template's runtime
and build dependencies, direct installer probes (including Qt6 Widgets and
Network), CMake requirements, the README's source-install package list,
pinned ViPERDSP handling, downstream patches, shared staging and backend
launcher contracts. It is invoked by `tests/check-package.sh` from both
normal package `check()` and the full developer suite. It is intentionally
compatible with a unified release archive, which has no `.gitmodules`.
`tests/check-arch-install.py` independently stages a mock stable and VCS
package and validates its file manifest and failure behavior.

The packaged `check()` suite is intentionally short and offline. For
native DSP and GUI regression coverage, run
`bash tests/check-developer.sh`. Arch release preparation additionally
builds the actual package and checks the LADSPA export. The separate
published AUR VCS recipe remains maintainer-owned; these repository tests
do not claim to validate its exact `pkgver()` or AUR metadata.

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

Our application code is MIT-licensed. ViPERDSP has different upstream terms,
which the MIT license does not supersede; see `THIRD-PARTY-NOTICES.md`.
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
