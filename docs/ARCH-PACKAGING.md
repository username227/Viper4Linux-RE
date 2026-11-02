# Arch Linux packaging (development and publication plan)

**Current state:** `packaging/arch/PKGBUILD` is a development recipe for
`viper4linux-re-git`, not yet a public AUR submission. It retrieves the
application from https://github.com/username227/viper4linux-re.git and
ViPERDSP separately, then initializes the pinned submodule using the local
makepkg Git source cache. Do not run `makepkg` inside a source checkout.

## Build the development package

Once the GitHub repository has been populated, place the PKGBUILD in a
separate folder and build it:

```bash
mkdir -p ~/Desktop/viper4linux-package
cp /path/to/PKGBUILD ~/Desktop/viper4linux-package/PKGBUILD
cd ~/Desktop/viper4linux-package
makepkg -si
```

For a pre-push test against a local checkout with full Git history, override
`VIPER4LINUX_GIT_URL` to a `file://` URL; do not edit the upstream URL inside
the PKGBUILD. `pkgver()` derives the revision and hash from actual Git history.
Use a space-free temporary build folder for now: Arch's generated compiler
`-ffile-prefix-map` can break under the current CMake/Make setup if a build
path contains spaces.

The local test package declares conflicts with `viper4linux-re`, `viper4linux`
and `viper4linux-git`. This permits pacman to offer removal of the older
incompatible installation instead of bypassing file ownership checks.
JamesDSP remains a separate runtime-routing compatibility question; it is
not declared a package conflict. Do not use pacman's `--overwrite` merely
to bypass conflicts.

The package installs the launcher, GUI, controller, LADSPA module, desktop
entry, icons, man pages, maintained docs, and all `src/viper_*.py` Python
modules. Both Arch VCS and stable PKGBUILDs call
`packaging/arch/install-package.sh` in `package()`; the script inventories
runtime Python modules automatically, checks required files before staging,
then verifies the package contents. When new runtime Python modules are added,
there is no need to edit the AUR `package()` file list.

For the AUR VCS recipe, the package function is now:

```bash
package() {
    local tree="$srcdir/viper4linux"
    local built="$srcdir/viper4linux-build"
    bash "$tree/packaging/arch/install-package.sh" "$tree" "$built" "$pkgdir" "$pkgname"
}
```

After updating an AUR checkout, regenerate `.SRCINFO` with
`makepkg --printsrcinfo > .SRCINFO` and push both files. Stable AUR
publication happens separately with the next release.

Personal settings, presets, and IRS/VDC files remain in user directories and
should survive package replacement. If an old source checkout installed a
launcher symlink, use `type -a viper4linux` to ensure it does not shadow
`/usr/bin/viper4linux`.

## Intended public packages

| Package | Source | Versioning |
|---|---|---|
| `viper4linux-re` | Published versioned source tarball | Fixed release number (`1.0.0-1`, etc.) |
| `viper4linux-re-git` | Published Git remote | Commit-derived VCS `pkgver()` |

The stable package should not need a Git checkout to build; its sources,
including the pinned DSP dependency, are assembled into a separate
unified-source release archive. The `-git` package follows the latest published
repository commit rather than a Desktop path. Before publication, review
reciprocal conflicts, upstream licenses, proper sources/hashes, dependencies,
and reproducible builds. The repository PKGBUILD remains subject to the
user's own separate cleanup/review.

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
   source revision and enter the target version (without `v`). The version must
   match `V4L_BASE_VERSION` in CMake.
2. The Arch preparation job generates the unified source, SHA-256, stable
   `PKGBUILD` and `.SRCINFO`; it builds and tests the package from the generated
   tarball, not the Git checkout.
3. The Ubuntu/Mint and Debian/LMDE jobs independently compile and install-test
   native `.deb` packages using the exact same unified source archive.
4. After **all three builds** pass, an assembly job verifies provenance,
   source/archive checksums and all three native package hashes. Download the
   `viper4linux-re-VERSION-complete-release-candidate` ZIP containing the
   unified archive, main Arch package, both `.deb` files, checksums,
   `PKGBUILD`, `.SRCINFO` and `SOURCE-COMMIT`. The optional Arch debug-symbol
   package is not included.
5. Confirm `vVERSION` points to **exactly** `SOURCE-COMMIT`, then create the
   GitHub release and attach the unified tarball, three native binaries, and
   appropriate checksums. GitHub's automatic source archives do not include
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
bash scripts/prepare-stable-release.sh 1.0.0 "$PWD/release-candidate"
```

The resulting source archive contains the pinned, unpatched upstream DSP; the
stable PKGBUILD applies the three compatibility fixes only to a private build
copy. The full Arch packaging test requires an Arch environment with pacman,
Qt6, CMake and the other declared dependencies.
