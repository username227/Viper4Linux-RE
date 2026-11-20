# Launchpad PPA preparation (review-only; no automatic uploads)

Launchpad accepts **Debian source packages**, not our prebuilt release `.deb`
files. This packaging layer uses the existing CMake install rules and the exact
published unified source archive, including its pinned ViPERDSP sources. The
Arch stable package and existing Ubuntu/Debian release build scripts are
unchanged.

Noble +ppa2 has been published; additional Resolute and Stonking packages
are **unpublished candidates**. The current project notice identifies
separate upstream ViPERDSP terms. Do not assume Canonical has approved the
redistribution arrangement merely because the source package builds.

## Initial supported series

- `noble`: Ubuntu 24.04 / Linux Mint 22.x.
- `resolute`: Ubuntu 26.04 LTS (new `+ppa1` candidate).
- `stonking`: Ubuntu 26.10 (new `+ppa1` candidate).
- Ubuntu 25.10 reached end of life in July 2026 and is deliberately omitted.
- Debian 13 and LMDE 7 use the existing native `.deb` release path, not Ubuntu PPAs.
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

The `prepare-launchpad-v1.2.0.yml` branch workflow also builds Resolute and
Stonking in separate official Ubuntu containers, from the exact verified v1.2.0
unified source archive. Each build runs the full CTest suite and checks the
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
