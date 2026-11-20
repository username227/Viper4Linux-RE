# Native Debian-family packages

Our normal distributable remains the unified-source archive. The single
[Prepare stable release](../.github/workflows/prepare-stable-release.yml)
workflow builds the Arch package and one standalone Debian 13/LMDE 7 `.deb`
from **the same pinned, unified source archive** before publication. It also
prepares and tests three unsigned, release-specific Ubuntu PPA source packages.
The 1.0.0 Debian-family backfill was completed separately; the one-off backfill
workflow is no longer used for future releases.

| Native binary build | Intended distributions | Validation scope |
| --- | --- | --- |
| Ubuntu 24.04, 26.04, 26.10 (PPA) | Supported Ubuntu releases; Mint 22.x uses the Noble package base | Each Ubuntu series is compiled and installation-tested independently; Mint-specific behavior needs confirmation |
| Debian 13 | Debian 13 and **LMDE 7** | Builds and installation checks run in Debian Trixie; LMDE-specific desktop behavior still needs testing |

The packages depend on installed system Qt6, PipeWire and Python libraries;
there is **no Flatpak, AppImage, Electron or bundled Qt runtime**. The package
is built independently for each distribution base to respect differences such
as Ubuntu Noble's `libqt6widgets6t64` and Debian Trixie's
`libqt6widgets6`. `dpkg-shlibdeps` determines the actual linked library
requirements; the build also requires `pipewire`, `python3` and Qt6's platform
plugins at runtime.

## CMake-owned installation

The standalone Debian builder `scripts/build-native-deb.sh` no longer
has independent DSP patching or application file-staging rules. It configures
CMake with `VIPER_ENABLE_SYSTEM_INSTALL=ON` and
`VIPER_SYSTEM_INSTALL_LAYOUT=debian`, builds the application, and stages
every runtime file with guarded `DESTDIR=... cmake --install`.

The script remains for Debian `.deb` creation because it needs distribution-specific
Qt/C++ dependency discovery via `dpkg-shlibdeps`, `DEBIAN/control` metadata,
`dpkg-deb`, and optional installation smoke tests in disposable distro
containers. These operations are not application installation rules.
The source archive remains pinned, and the existing stable release workflow
continues to generate the Debian standalone package and three Ubuntu PPA candidates.

Debian packages continue to include the GPL text under
`/usr/share/doc/viper4linux-re/LICENSE` and the package-specific `copyright`
file credits Gerald Frank while keeping ViPERDSP's separate notices. Arch
omits only the redundant common GPL license file, not the upstream notice.

The installed application still uses absolute `/usr` paths. CMake refuses
other prefixes and unsafe `DESTDIR` values. Do not call `cmake --install`
on a live host without an approved packaging environment.

## Prepare release assets

For each new version, manually dispatch **Actions → Prepare stable release**
and enter the version without the `v` prefix (for example, `1.2.0`).

1. The `prepare` job exports the recorded submodule into the unified source,
   generates the checksum and stable PKGBUILD, then compiles and tests the Arch
   package against **the generated archive**, not a Git checkout.
2. The Debian 13/LMDE 7 job builds, runs CTest, and installation-tests its
   native standalone `.deb` inside Debian Trixie.
3. Three Ubuntu PPA jobs use the same source in Noble, Resolute and Stonking,
   independently building, running CTest and installation-testing each binary.
   The `git` build dependency is explicit in the Debian source metadata.
4. The `assemble` job requires Arch, Debian, and **all three Ubuntu PPA jobs**
   to pass. It verifies the commit and three GitHub release artifacts, with
   a single `SHA256SUMS`, and emits a complete release candidate.
   It also collects a separate `ppa-VERSION-all-series-unsigned-REVIEW`
   bundle of the three PPA source packages and locally tested binaries.
5. Review the artifacts and ensure the version tag points to the recorded
   `SOURCE-COMMIT`. Manually attach the unified source, Arch and Debian
   standalone package, plus the checksums, to the GitHub release.
   After review, separately sign and upload each Ubuntu source package to
   Launchpad. The published-PPA installation smoke test runs after publication.
6. Update stable and VCS AUR recipes in the **separate local AUR checkouts**,
   regenerating `.SRCINFO` before pushing.

No workflow creates a tag, publishes a release, pushes an AUR package, or
builds Flatpak. The automatically generated GitHub source archive is not a
substitute for the uploaded unified source archive. The complete release candidate contains one consolidated `SHA256SUMS`
covering the unified source archive, Arch package and standalone Debian `.deb`;
verify it with `sha256sum --check SHA256SUMS` from the extracted directory.

For a desktop install, users can open the matching `.deb` in their graphical
package manager or use `sudo apt install ./filename.deb` to resolve runtime
dependencies. They must have an operational **PipeWire** audio session;
PulseAudio-only setups are not supported. The GUI can be started with
`viper4linux` and the native CLI with `v4l-cli` (also available as
`viper4linux-ctl`). The application launcher itself is native; Python
remains needed by the Audio Output monitor and related support modules.

This package deliberately uses the same `viper4linux-re` package name and
filesystem paths as the native Arch release. The published 1.0.0 packaging history remains preserved; current releases
use the maintained set of downstream DSP patches without adding DSP changes
solely for Debian packaging. The historical two `.deb` packages successfully built in GitHub Actions for 1.0.0;
future releases keep only the Debian/LMDE standalone package;
Mint-specific GUI, routing and tray behavior still requires direct testing.

## Extending support

Ubuntu 24.04, 26.04 and 26.10 receive separate native PPA builds.
Linux Mint 21.x is not covered by the Noble/Mint 22.x package base. A Mint 22.x user or LMDE 7 user
should ideally confirm GUI launch, X11/Wayland tray behavior, default-output
routing and Bluetooth reconnects before we declare them independently tested.
