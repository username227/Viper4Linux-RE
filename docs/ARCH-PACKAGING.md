# Arch Linux development package (`viper4linux-git`)

The release archive is now a **tar.gz containing the complete Git repository** and
`packaging/arch/PKGBUILD`, rather than a source ZIP. Until the repository is
published, the PKGBUILD uses `git+file://` to read the included Git history.
The pinned ViPERDSP source is fetched separately from GitHub; no generated DSP
binaries are included in the release.

From the extracted Viper4Linux project:

```bash
cd packaging/arch
makepkg -si
```

The package installs these system-owned files:

- `/usr/bin/viper4linux` — launcher, using the installed backend and GUI
- `/usr/bin/viper4linux-gui` — Qt6 application
- `/usr/bin/viper4linux-ctl` — native control CLI
- `/usr/lib/ladspa/viper_dsp_ladspa.so` — compiled ViPERDSP adapter
- `/usr/lib/viper4linux/` — launcher logic and per-user backend startup helper
- `/usr/share/viper4linux/routing-config/` — Viper4Linux's PipeWire filter
- `/usr/share/applications/viper4linux.desktop` and icons — Cinnamon menu

Neither `/etc/pipewire` nor the user's PipeWire configuration is changed. At
launch time a private copy of the distro's `filter-chain.conf` is written in
`$XDG_RUNTIME_DIR/viper4linux-filter-config/` and combined with our filter
module. Persistent preferences remain under `~/.config/ViPERforLinux` and IRS
files under `~/.local/share/viper4linux/IRS`.

**Important for testing:** An old source install may have created
`~/.local/bin/viper4linux`. On Arch, `~/.local/bin` often precedes `/usr/bin`
in `PATH`. Run `type -a viper4linux` to verify the new binary is selected; if
an old `~/.local/bin/viper4linux` is a symlink to an extracted source checkout,
remove *only that obsolete symlink* after verifying it, e.g.
`ls -l ~/.local/bin/viper4linux` and then `unlink ~/.local/bin/viper4linux`.
This avoids accidentally launching the previous source build.

The source-derived pacman version is `0.1.0.r<revision-count>.g<short-hash>`.
The revision count comes from `git rev-list --count HEAD`, not an invented tag.
The About dialog uses the simpler `0.1.0-<commit-hash>` format. When commits
advance, rebuilding with makepkg produces a new `pkgver`.

The package intentionally has no `conflicts`, `provides`, or `replaces` entries
during local testing. Before publication, review Noah's Viper4Linux package
names and determine whether any actual package/path/service collision exists
with JamesDSP. No forced removal of JamesDSP is assumed.

This is development packaging. The project's redistribution license has not
been finalized and **this package has not been compiled by makepkg in a native
Arch environment**; real Qt6 and PipeWire integration must be tested there.
