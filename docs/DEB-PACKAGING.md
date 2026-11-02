# Native Debian-family packages

Our normal distributable remains the unified-source archive. The workflows
build two independently compiled `.deb` packages from that same source archive. The main [Prepare stable release](../.github/workflows/prepare-stable-release.yml)
workflow creates the unified source, builds Arch, then builds the `.deb` packages
from **that exact source archive before publication**. The separate manual
[Prepare native Debian-family packages](../.github/workflows/prepare-deb-release.yml)
workflow is for an **already published, tagged release** such as 1.0.0.
For an existing release, it checks GitHub's recorded SHA-256 digest and
verifies the source manifest's application commit against the Git tag. A newer
commit on `main` cannot silently change the packaged DSP or application source.

| Native binary build | Intended distributions | Validation scope |
| --- | --- | --- |
| Ubuntu 24.04 LTS | Ubuntu 24.04 and **Linux Mint 22.x**, including Mint 22.3 | Builds and installation checks run in Ubuntu Noble; Mint-specific desktop behavior still needs testing |
| Debian 13 | Debian 13 and **LMDE 7** | Builds and installation checks run in Debian Trixie; LMDE-specific desktop behavior still needs testing |

The packages depend on installed system Qt6, PipeWire and Python libraries;
there is **no Flatpak, AppImage, Electron or bundled Qt runtime**. The package
is built independently for each distribution base to respect differences such
as Ubuntu Noble's `libqt6widgets6t64` and Debian Trixie's
`libqt6widgets6`. `dpkg-shlibdeps` determines the actual linked library
requirements; the build also requires `pipewire`, `python3` and Qt6's platform
plugins at runtime.

## Prepare release assets

For **future releases**, run **Prepare stable release** as before: the Arch
job produces the release-candidate archive and the two Debian-family jobs
build from that same source. All three jobs must succeed before tagging and
publishing. Download the source/Arch artifact and both `.deb` artifacts for
the manual GitHub release. No job publishes files automatically.

For **the already published 1.0.0 release**, use the separate action:

1. Open Actions → **Prepare native Debian-family packages** → Run workflow.
   Enter `1.0.0` (without `v`).
2. The jobs retrieve the *published* 1.0.0 unified-source tarball, validate its
   SHA-256 digest and manifest against the `v1.0.0` tag, and build inside
   Ubuntu Noble and Debian Trixie containers.
3. After both jobs succeed, download their artifacts. Each contains one
   native `.deb` and its `SHA256SUMS`.
4. **Manually** attach the validated `.deb` files to the *existing* GitHub
   release. Label Mint 22.x as targeted by the Ubuntu 24.04 build (and LMDE 7
   by the Debian 13 build). Neither workflow publishes anything.

For a desktop install, users can open the matching `.deb` in their graphical
package manager or use `sudo apt install ./filename.deb` to resolve runtime
dependencies. They must have an operational **PipeWire** audio session;
PulseAudio-only setups are not supported. The GUI can be started with
`viper4linux` and the CLI with `viper4linux-ctl`.

This package deliberately uses the same `viper4linux-re` package name and
filesystem paths as the native Arch release. The published 1.0.0 source and
all three downstream DSP patches are preserved; no DSP changes are made to
create a Debian-family package. Until these workflows succeed on GitHub,
there is **no claim of tested Debian/Mint binaries**.

## Extending support

Ubuntu 26.04 LTS and Linux Mint 21.x have different package bases and are not
covered by the initial two build targets. A Mint 22.x user or LMDE 7 user
should ideally confirm GUI launch, X11/Wayland tray behavior, default-output
routing and Bluetooth reconnects before we declare them independently tested.
