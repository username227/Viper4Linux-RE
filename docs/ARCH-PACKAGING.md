# Arch Linux local development packaging

The source-with-Git tar.gz contains the full application repository; ViPERDSP
is retrieved at the pinned upstream commit by the development PKGBUILD.

**Build from any separate directory** — do not run makepkg in the source
checkout itself (including under its packaging/arch subdirectory):

```bash
# Extract the source archive on the Desktop; it contains viper4linux-imager/
mkdir -p ~/Desktop/viper4linux-package
cp ~/Downloads/PKGBUILD ~/Desktop/viper4linux-package/
cd ~/Desktop/viper4linux-package
makepkg -si
```

By default, the PKGBUILD reads a separate **local Git checkout** at
`~/Desktop/viper4linux-imager` regardless of where makepkg is launched.
Override with `VIPER4LINUX_REPO=/absolute/path makepkg -si` if necessary.
The revision count and twelve-digit hash are calculated from that checkout
using `git rev-list --count HEAD` and `git rev-parse --short=12 HEAD`.

There are no `conflicts`/`provides`/`replaces` entries during local testing.
Package installation does not modify /etc/pipewire or overwrite the user's
PipeWire configuration. The launcher handles backend fingerprint changes.

## Publication plan (not implemented yet)

- `viper4linux`: stable, versioned source tar.gz release, no Git needed for
  the build. ViPERDSP must be available as tarball source or within a properly
  licensed source bundle (not fetched via Git during the package build).
- `viper4linux-git`: current source from the published Git remote, with
  automatic commit-derived `pkgver()`, not the local Desktop checkout.
- Stable and Git packages must conflict with one another. Review actual
  package names and file conflicts for other ViPER implementations; consider
  JamesDSP compatibility separately rather than conflicting without evidence.
- A fix or tested workaround for building under paths containing spaces is
  still outstanding; use a simple directory name during local testing.
- Confirm redistribution licenses before publishing.

After installing, run `type -a viper4linux` to check that an obsolete
`~/.local/bin/viper4linux` symlink is not shadowing `/usr/bin/viper4linux`.
