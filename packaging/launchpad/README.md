# Launchpad PPA preparation (review-only; no automatic uploads)

Launchpad accepts **Debian source packages**, not our prebuilt release `.deb`
files. This packaging layer uses the existing CMake install rules and the exact
published unified source archive, including its pinned ViPERDSP sources. The
Arch stable package and existing Ubuntu/Debian release build scripts are
unchanged.

Noble +ppa2 was published; Resolute and Stonking v1.2.0 builds completed
successfully on Launchpad. Verify each publication status independently. The current project notice identifies
separate upstream ViPERDSP terms. Do not assume Canonical has approved the
redistribution arrangement merely because the source package builds.

## Initial supported series

- `noble`: Ubuntu 24.04 / Linux Mint 22.x.
- `resolute`: Ubuntu 26.04 LTS (separate native PPA binary).
- `stonking`: Ubuntu 26.10 (separate native PPA binary).
- Ubuntu 25.10 reached end of life in July 2026 and is deliberately omitted.
- Debian 13 and LMDE 7 retain the standalone native `.deb` release path.
- Future releases no longer create a duplicate standalone Ubuntu/Mint `.deb`;
  older published `.deb` assets remain available.
- Only `amd64` is enabled to match the architectures already validated.

## Create a source package without uploading it

Run in an **Ubuntu packaging environment** with `dpkg-dev`, `debhelper`,
`build-essential`, `xz-utils` and `fakeroot` installed. From a checkout of this
branch, download the published, checksum-verified unified source tarball of the
target release, then:

```bash
bash scripts/prepare-launchpad-source.sh 1.2.0 noble \
  /path/to/viper4linux-re-1.2.0-unified-source.tar.xz \
  /path/to/launchpad-output
```

No installation or upload occurs. `dpkg-buildpackage -S -sa -us -uc -d`
prepares an **unsigned source package**. For local binary-build validation
in a clean Ubuntu environment, build the generated source package with
`dpkg-source -x` and `dpkg-buildpackage -b -us -uc`, with the `debian/control`
build dependencies available; run the offline CTest suite and inspect the binary
package. Do not publish merely because source packaging succeeded.

## Release-preparation integration (unsigned candidates only)

The branch's `.github/workflows/prepare-stable-release.yml` now includes a
`launchpad-source` matrix job for **Noble, Resolute and Stonking**. When the
maintainer manually runs **Prepare stable release** with a candidate version,
the job downloads the exact checksum-verified unified source archive produced
by that workflow's Arch/source preparation job. It does not require a published
GitHub release or refetch a possibly older tag. Each Ubuntu-series container
prepares `+ppa1` source, builds its native binary with the complete offline
CTest suite, installs and removes that binary, and saves **unsigned** source
upload files and test `.deb` for review.

These `ppa-VERSION-SERIES-unsigned-REVIEW` artifacts deliberately do not
match the normal release-asset collection pattern. The existing release's
three checksum entries (unified source, Arch and Debian/LMDE)
are independently verified. Assembly waits for all three Ubuntu PPA builds,
and produces a separate `ppa-VERSION-all-series-unsigned-REVIEW` artifact. No workflow signs, sends files to Launchpad, tags a release,
or changes the PPA without a separate maintainer action. A correction to an
already-uploaded Debian version requires a new `+ppaN` revision; the
current automated preparation deliberately starts new versions at `+ppa1`.

### Testing the **published** PPA is a separate post-upload gate

The separate `.github/workflows/test-published-ppa-install.yml` fetches
the **actual Launchpad binary**, not the source candidate. It checks APT
origin/version, dependency resolution, installed files, LADSPA exports,
read-only commands and clean removal on the selected Ubuntu series.
It supports manual workflow dispatch parameters: `series` (`noble`,
`resolute`, `stonking`), upstream `version` and `ppa_revision`.
It must run **after** the matching Launchpad binary is published;
automatically running it during release preparation would test an old
published binary or fail before publication. The branch-push Noble
test defaults to the already-published v1.2.0 `+ppa2`.

The v1.2.0 branch validation has completed a native source rebuild,
**50/50 CTests** on both Ubuntu 26.04 and 26.10, APT installation,
library checks and clean removal. This is not equivalent to a successful
Launchpad build or a published-package smoke test for those series.

## Upload, only after review and a successful build test

1. Create a Launchpad account, register a suitable OpenPGP key and create a
   public PPA named `viper4linux-re` from the Launchpad interface.
2. Sign the reviewed `*_source.changes` and associated `.dsc` with `debsign`.
3. Upload the signed `*_source.changes` with
   `dput ppa:<your-launchpad-id>/viper4linux-re <file>_source.changes`.
4. Watch the Launchpad build logs and verify installed binaries on the target
   Ubuntu version before documenting user installation.

Those steps require the maintainer's own Launchpad account/GPG key. Nothing
here creates a PPA or publishes packages automatically.

## Version ordering matters

The existing Ubuntu 24.04/Mint 22 GitHub release .deb uses a version such as
`1.2.0-1~ubuntu24.04mint22`. The first Noble PPA package uses
`1.2.0-1~ubuntu24.04mint22+ppa1` so APT considers it an upgrade, not a
downgrade. Use `VIPER_PPA_REVISION=2` for a corrected PPA upload of the same
upstream version; Launchpad will not accept reusing an existing version.

The one-time `prepare-launchpad-v1.2.0.yml` workflow validated the initial
release-specific packaging. Future release preparation uses the generalized
three-series matrix in `prepare-stable-release.yml`, from its newly prepared,
checksum-verified unified source archive. Each build runs the full CTest suite and checks the
candidate package's dependencies, installed paths, CLI, and clean removal.
Unsigned source-upload files and test binaries are retained for maintainer
review. No signing or Launchpad upload occurs. The production Launchpad builds
and a post-publication install smoke test for each series remain required.

The first Noble `+ppa1` source upload was accepted, but its clean Launchpad
binary build failed because CTest invokes `git` and `git` was absent from
`Build-Depends`. Revision `+ppa2` declares `git` explicitly. Our GitHub runner
already had `git`, which masked that missing build dependency during CI.
Keep all tests enabled and verify the Launchpad chroot result after upload.

A single PPA can host multiple Ubuntu suites. Each series requires a separate
source upload (or a compatible package copy) and its own build validation.
