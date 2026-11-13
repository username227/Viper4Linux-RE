# Native Debian-family packages

Our normal distributable remains the unified-source archive. The single
[Prepare stable release](../.github/workflows/prepare-stable-release.yml)
workflow builds the Arch package and two independently compiled native `.deb`
packages from **the same pinned, unified source archive** before publication.
The 1.0.0 Debian-family backfill was completed separately; the one-off backfill
workflow is no longer used for future releases.

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

## CMake-owned installation

The native Debian and Ubuntu builder `scripts/build-native-deb.sh` no longer
has independent DSP patching or application file-staging rules. It configures
CMake with `VIPER_ENABLE_SYSTEM_INSTALL=ON` and
`VIPER_SYSTEM_INSTALL_LAYOUT=debian`, builds the application, and stages
every runtime file with guarded `DESTDIR=... cmake --install`.

The script remains because native `.deb` creation needs distribution-specific
Qt/C++ dependency discovery via `dpkg-shlibdeps`, `DEBIAN/control` metadata,
`dpkg-deb`, and optional installation smoke tests in disposable distro
containers. These operations are not application installation rules.
The source archive remains pinned, and the existing stable release workflow
continues to generate the distro-specific packages.

For historical parity regression, `tests/check-cmake-deb-parity.py`
can compare the CMake stage to a **frozen test-only baseline** located in
`tests/fixtures/legacy-deb-stage.txt`. That file is never called by the
production Debian builder. The comparison runs in temporary directories,
without `apt`, `dpkg`, or a live audio session.

Debian packages continue to include the GPL text under
`/usr/share/doc/viper4linux-re/LICENSE` and the package-specific `copyright`
file credits Gerald Frank while keeping ViPERDSP's separate notices. Arch
omits only the redundant common GPL license file, not the upstream notice.

The installed application still uses absolute `/usr` paths. CMake refuses
other prefixes and unsafe `DESTDIR` values. Do not call `cmake --install`
on a live host without an approved packaging environment.

## Prepare release assets

For each new version, manually dispatch **Actions → Prepare stable release**
and enter the version without the `v` prefix (for example, `1.1.0`).

1. The `prepare` job exports the recorded submodule into the unified source,
   generates the checksum and stable PKGBUILD, then compiles and tests the Arch
   package against **the generated archive**, not a Git checkout.
2. The two Debian-family jobs build from the **same downloaded artifact** in
   their distribution-specific containers and verify dependencies and installation.
3. The `assemble` job runs only after all three builds pass. It checks the
   `SOURCE-COMMIT` against the workflow's commit, verifies the unified source
   manifest and all three package checksums, and uploads one
   `viper4linux-re-VERSION-complete-release-candidate` artifact.
4. Download that ZIP, review its contents, and ensure the `vVERSION` tag points
   to the recorded `SOURCE-COMMIT`. Attach the unified source archive, the
   main Arch package, the two `.deb` files, and the single consolidated
   `SHA256SUMS` file to the GitHub release manually.
   The `-debug` Arch package is intentionally excluded.
5. The stable and VCS AUR packages are updated in your **separate local AUR
   checkouts**, not by the workflow. Regenerate `.SRCINFO` before pushing.

No workflow creates a tag, publishes a release, pushes an AUR package, or
builds Flatpak. The automatically generated GitHub source archive is not a
substitute for the uploaded unified source archive. The complete release candidate contains one consolidated `SHA256SUMS`
covering the unified source archive, Arch package, and both `.deb` packages;
verify it with `sha256sum --check SHA256SUMS` from the extracted directory.

For a desktop install, users can open the matching `.deb` in their graphical
package manager or use `sudo apt install ./filename.deb` to resolve runtime
dependencies. They must have an operational **PipeWire** audio session;
PulseAudio-only setups are not supported. The GUI can be started with
`viper4linux` and the CLI with `v4l-cli`.

This package deliberately uses the same `viper4linux-re` package name and
filesystem paths as the native Arch release. The published 1.0.0 source and
all three downstream DSP patches are preserved; no DSP changes are made to
create a Debian-family package. The two `.deb` packages successfully built in GitHub Actions for 1.0.0;
Mint-specific GUI, routing and tray behavior still requires direct testing.

## Extending support

Ubuntu 26.04 LTS and Linux Mint 21.x have different package bases and are not
covered by the initial two build targets. A Mint 22.x user or LMDE 7 user
should ideally confirm GUI launch, X11/Wayland tray behavior, default-output
routing and Bluetooth reconnects before we declare them independently tested.
