# Installing ViPER4Linux-RE

ViPER4Linux-RE brings native ViPERDSP audio effects to Linux through **PipeWire**, with a **Qt6 desktop interface**. This guide is for people who want to install, run, update, or remove the application. For build and packaging internals, see the [developer documentation](DEVELOPMENT.md).

> **Development status:** ViPER4Linux-RE is an actively developed proof of concept. **Arch Linux with Cinnamon/X11** is the primary environment tested by the maintainer. Native packages are also built for Ubuntu and Debian distribution bases, but broader desktop and hardware compatibility has not yet been established.

## Before you install

- **PipeWire is required.** You need a working PipeWire audio session; a PulseAudio-only setup is not currently supported. The installer does not convert your sound system to PipeWire.
- **Desktop application:** The GUI requires Qt6 and a graphical desktop session. Native packages declare their required runtime libraries; you do not normally need to install Qt6 development packages yourself.
- **Package architecture:** The published native Arch and Debian-family packages are **x86_64 / amd64** builds. Other architectures have not been validated.
- **Choose one variant:** Do not install the stable and development Arch packages together. They provide the same application files and declare conflicts.
- **Existing DSP routing:** Running another system-wide audio processor such as JamesDSP may cause routing conflicts. Simultaneous operation has not been verified.

## Choose an installation method

| Distribution | Recommended option | Other option |
| --- | --- | --- |
| **Arch Linux** | [Stable AUR package](https://aur.archlinux.org/packages/viper4linux-re) | [Development AUR package](https://aur.archlinux.org/packages/viper4linux-re-git) or download the prebuilt Arch package from GitHub from the [Releases](https://github.com/username227/Viper4Linux-RE/releases) page |
| **Ubuntu 24.04 / Linux Mint 22.x** | Download the native Ubuntu/Mint `.deb` from the [releases](https://github.com/username227/Viper4Linux-RE/releases) page | Source build for experienced users |
| **Debian 13 / LMDE 7** | Download the native Debian/LMDE `.deb` from the [releases](https://github.com/username227/Viper4Linux-RE/releases) page | Source build for experienced users |
| **Other PipeWire-based distributions** | No validated native package yet | Build from source; compatibility is not guaranteed |

Download the published native packages and checksums from the **[latest GitHub release](https://github.com/username227/Viper4Linux-RE/releases/latest)**. Use the package for your actual distribution base: the two `.deb` builds are **not interchangeable**. Linux Mint and LMDE desktop behavior still needs independent testing, even though packages are built and installation-checked on their corresponding Ubuntu and Debian bases.

## Arch Linux

### Option A: AUR (recommended)

Two AUR packages are available:

- **[`viper4linux-re`](https://aur.archlinux.org/packages/viper4linux-re)** follows tagged stable releases and builds from the published unified-source archive.
- **[`viper4linux-re-git`](https://aur.archlinux.org/packages/viper4linux-re-git)** follows the development repository. It may contain fixes and features not yet released, but can change more frequently.

If you already use an AUR helper, install **one** of these packages. For example, with `paru`:

```bash
paru -S viper4linux-re
```

Or, to choose the development version instead:

```bash
paru -S viper4linux-re-git
```

An AUR helper is optional. To build the stable package manually, install the basic Arch build tools, inspect the package recipe, and run `makepkg` **as your regular user**:

```bash
sudo pacman -S --needed base-devel git
git clone https://aur.archlinux.org/viper4linux-re.git
cd viper4linux-re
less PKGBUILD
makepkg -si
```

For the development package, substitute `viper4linux-re-git` in the AUR clone URL and directory name. Always review AUR build recipes before running them. AUR helpers are community tools, not official Arch package-management commands; consult the [ArchWiki AUR guide](https://wiki.archlinux.org/title/Arch_User_Repository) if you need more detail.

### Option B: Prebuilt Arch package

The [GitHub release](https://github.com/username227/Viper4Linux-RE/releases/latest) also provides a versioned **`x86_64.pkg.tar.zst`** package for a system installation without building locally.

1. Download the Arch package and, optionally, `SHA256SUMS` from the **same release**.
2. Open a terminal in the download directory.
3. Install the actual downloaded package with pacman:

```bash
sudo pacman -U name-of-downloaded-package.pkg.tar.zst
```

Replace the example filename with the release asset's actual name. This is a locally installed package: **`pacman -Syu` does not discover future GitHub releases or upgrade it automatically**. To upgrade, download the newer package and install it with `pacman -U`, or switch to the stable AUR recipe for AUR-helper-managed updates.

## Ubuntu 24.04 and Linux Mint 22.x

1. Open the [latest GitHub release](https://github.com/username227/Viper4Linux-RE/releases/latest).
2. Download the `.deb` asset whose name includes **`ubuntu24.04mint22`** and **`amd64`**. Do not choose the Debian 13/LMDE 7 build.
3. Open the file in your graphical package installer, **or** install it from a terminal in the download directory:

```bash
sudo apt install ./name-of-downloaded-package.deb
```

Replace the example filename with the actual file name. Using `apt install ./...` lets APT resolve the package's declared dependencies. The package assumes you already have a functioning PipeWire audio session.

**Compatibility scope:** The Ubuntu 24.04 package is built and installation-checked on Ubuntu 24.04. Linux Mint 22.x is an intended target, but its particular desktop, routing, and tray behavior still needs independent testing.

## Debian 13 and LMDE 7

1. Open the [latest GitHub release](https://github.com/username227/Viper4Linux-RE/releases/latest).
2. Download the `.deb` asset whose name includes **`debian13lmde7`** and **`amd64`**. Do not choose the Ubuntu 24.04/Mint 22.x build.
3. Open the file in your graphical package installer, **or** install it from a terminal in the download directory:

```bash
sudo apt install ./name-of-downloaded-package.deb
```

Replace the example filename with the actual file name. The Debian 13 package is built and installation-checked on Debian 13. **LMDE 7** is an intended target but has not yet received equivalent desktop-level testing.

## Verify a release download (optional)

Each release includes a **`SHA256SUMS`** file covering the unified-source archive and all three native package files. To verify a package you downloaded, keep it and `SHA256SUMS` in the same directory and run:

```bash
sha256sum --check --ignore-missing SHA256SUMS
```

The `--ignore-missing` option avoids errors for the other release assets you did not download. Check that your selected file is listed with `OK`. A matching checksum verifies file integrity against the release manifest; it is not an independent signature or security audit.

## Build from source

Direct source builds are primarily a **developer / advanced-user** option, not a substitute for an officially tested package on every distribution. Source builds do **not** install a system package by default.

On Arch Linux, install the prerequisites used in the project's tested source-build instructions:

```bash
sudo pacman -S --needed base-devel cmake git pkgconf ladspa pipewire qt6-base python
```

Clone the repository with its **pinned ViPERDSP submodule**, build, and run the non-installing source check:

```bash
git clone --recurse-submodules https://github.com/username227/Viper4Linux-RE.git
cd Viper4Linux-RE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target viper_source_check
```

**Important:** A Git clone normally creates a directory matching the repository name's capitalization; if you specify a different destination or your file system differs, use that actual directory in the `cd` command.

When you deliberately want to launch this source build, use:

```bash
./viper4linux
```

The source launcher can **replace a running packaged ViPER backend** if their build identities differ. Do not launch it during audio playback you need to preserve. It does **not** restart the entire PipeWire service. You can inspect the planned action without changing the running backend:

```bash
./viper4linux --preflight
```

For an existing source checkout, initialize or restore the recorded DSP revision before building:

```bash
git submodule update --init --checkout third_party/ViPERDSP
```

**Other distributions:** Equivalent C/C++ build tools, CMake, Qt6 development libraries (Widgets and Network), X11 development headers, Python, PipeWire, and LADSPA support are required. Package names vary by distribution; this guide does not claim a tested one-line source build for them. Refer to the [developer guide](DEVELOPMENT.md) and [Arch packaging guide](ARCH-PACKAGING.md). Do **not** use `sudo cmake --install` as a shortcut: the project deliberately restricts direct host installation in favor of controlled native packages.

GitHub's automatically generated **Source code** archives do *not* include the pinned DSP submodule. If you want a release source archive rather than a Git checkout, download the release's separately attached **`unified-source.tar.xz`** asset.

## First launch and basic checks

After a native package installation, launch **ViPER4Linux-RE** from your desktop application menu or run:

```bash
viper4linux
```

The launcher starts or reuses the application's audio backend and Audio Output monitor as needed. It does **not** restart your system PipeWire service. Closing or hiding the GUI does not necessarily stop processing: when you actually quit the GUI, its configured exit behavior determines whether the ViPER backend is kept running or stopped.

For CLI diagnostics:

```bash
viper4linux-ctl status
viper4linux-ctl --help
```

If the status reports **Ready to process audio**, the backend process exists but the audio-control channel is not yet ready. If it persists, consult the [user guide](USER-GUIDE.md) and backend logs (usually `~/.local/state/viper4linux/backend.log`). An advancing audio callback counter indicates processing activity, not necessarily audible sound. Audio Output memory is opt-in and disabled by default.

## Updating

- **Arch stable AUR:** Use your AUR helper's normal package-update procedure, or update the AUR Git checkout and rebuild with `makepkg -si`.
- **Arch development AUR:** Rebuild against current upstream commits using your helper's VCS/development update procedure or an updated AUR Git checkout. A development package may have new upstream commits even when its AUR recipe has not changed.
- **Prebuilt Arch package:** Download a newer GitHub release package and install it with `sudo pacman -U ./actual-filename.pkg.tar.zst`.
- **Ubuntu/Mint/Debian/LMDE `.deb`:** Download the newer matching distribution package and run `sudo apt install ./actual-filename.deb`. Manual GitHub downloads do not set up an automatic APT update repository.
- **Direct source checkout:** Update the Git branch intentionally, then update its recorded ViPERDSP submodule and rebuild. For a clean checkout tracking `main`, the usual steps are:

  ```bash
  git pull --ff-only
  git submodule update --init --checkout third_party/ViPERDSP
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  cmake --build build --target viper_source_check
  ```

**Before switching installation methods**, check for an existing package or old launcher shortcut. Use `type -a viper4linux` to see which executable your shell would run. Do not work around conflicting package files with `pacman --overwrite`.

## Uninstalling and keeping your settings

If the GUI is running, quit it and choose **Stop backend** first when offered. Avoid removing the package while its processing backend is still active.

For an installed Arch package, remove whichever variant you installed:

```bash
sudo pacman -R viper4linux-re
# or, if you installed the development package:
sudo pacman -R viper4linux-re-git
```

For a Debian-family native package:

```bash
sudo apt remove viper4linux-re
```

For a **direct source build** that was never installed as a system package, there is no pacman/APT package to remove. Stop the source-launched backend and remove the source checkout only when you no longer need it; separately review any optional source shortcuts you explicitly created.

**Personal settings and libraries are intentionally separate from application files.** Removing a package does not automatically delete saved presets, imported IRS/VDC files, or per-output snapshots in `~/.local/share/viper4linux/`, nor should you delete them just to upgrade. Consider backing them up before changing installation methods. See [presets and libraries](PRESETS-AND-LIBRARIES.md) and the [user guide](USER-GUIDE.md).

## Help and limitations

- [User guide](USER-GUIDE.md): launching, tray behavior, audio routing, CLI and troubleshooting.
- [Effects reference](EFFECTS.md): effect controls and supported sample rates.
- [Arch packaging guide](ARCH-PACKAGING.md) / [Debian packaging guide](DEB-PACKAGING.md): package-building details for maintainers.
- [Issue tracker](https://github.com/username227/Viper4Linux-RE/issues): report a reproducible installation or compatibility problem, with your distribution, desktop environment, PipeWire version, package source, and relevant error messages.
- [Third-party notices](../THIRD-PARTY-NOTICES.md): upstream ViPERDSP has separate licensing and redistribution terms from the project's original application code.