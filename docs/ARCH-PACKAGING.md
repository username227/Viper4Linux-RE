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
entry, icons, man pages, and maintained docs. Personal settings, presets,
and IRS/VDC files remain in user directories and should survive package
replacement. If an old source checkout installed a launcher symlink,
use `type -a viper4linux` to ensure it does not shadow `/usr/bin/viper4linux`.

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
