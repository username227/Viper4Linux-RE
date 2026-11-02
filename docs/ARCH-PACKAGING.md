# Arch Linux packaging (development and publication plan)

**Current state:** `packaging/arch/PKGBUILD` is the local-development recipe
for `viper4linux-re-git`, not yet a public AUR submission. The package builds
against a separate local Git checkout; do not run `makepkg` inside that
checkout or any of its subdirectories.

## Build the current development checkout

Extract the source-with-Git archive somewhere on the Desktop, keeping its
`.git` directory. Create a separate folder containing the PKGBUILD:

```bash
mkdir -p ~/Desktop/viper4linux-package
cp /path/to/PKGBUILD ~/Desktop/viper4linux-package/PKGBUILD
cd ~/Desktop/viper4linux-package
VIPER4LINUX_REPO="$HOME/Desktop/your-extracted-source-directory" makepkg -si
```

The `VIPER4LINUX_REPO` environment variable overrides the checkout path
currently hard-coded as a default in the test PKGBUILD. The `pkgver()`
function derives revision count and commit hash from its actual Git history.
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
| `viper4linux-re` | Published versioned source tarball | Fixed release number (`0.1.0-1`, etc.) |
| `viper4linux-re-git` | Published Git remote | Commit-derived VCS `pkgver()` |

The stable package should not need a Git checkout to build; its sources,
including the suitably licensed DSP dependency, must be obtainable through
release archives. The `-git` package should follow the latest published
repository commit rather than a Desktop path. Before publication, review
reciprocal conflicts, upstream licenses, proper sources/hashes, dependencies,
and reproducible builds. The repository PKGBUILD remains subject to the
user's own separate cleanup/review.
