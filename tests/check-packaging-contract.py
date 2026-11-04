#!/usr/bin/env python3
"""Offline dependency and packaging contracts for source and stable Arch builds.

The published -git PKGBUILD lives in the AUR, not in this source tree.
This test must also work inside a unified release archive (without .gitmodules).
"""
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
arch = root / 'packaging/arch'
stable_path = arch / 'PKGBUILD.stable.in'
stable = stable_path.read_text()
prepare = (root / 'cmake/PrepareViPERDSP.cmake').read_text()
source_setup = (root / 'cmake/SourceCheckout.cmake').read_text()
cmake = (root / 'CMakeLists.txt').read_text()
readme = (root / 'README.md').read_text()
license_text = (root / 'LICENSE').read_text()
copyright_notice = (root / 'COPYRIGHT').read_text()
third_party_notices = (root / 'THIRD-PARTY-NOTICES.md').read_text()
launcher = (root / 'src/viper_launcher.py').read_text()
backend = (arch / 'start-backend').read_text()


def require(value, reason):
    assert value, reason


def bash_array(recipe, name):
    match = re.search(rf'(?m)^{re.escape(name)}=\(([^)]*)\)', recipe)
    require(match is not None, f'{name} missing from stable PKGBUILD template')
    return set(shlex.split(match.group(1), comments=True))


# No second, stale -git PKGBUILD: the separately maintained AUR checkout is
# authoritative for its VCS source, versioning and package metadata.
require(not (arch / 'PKGBUILD').exists(), 'Obsolete development PKGBUILD reintroduced')
subprocess.run(['bash', '-n', str(stable_path)], check=True)
require(not (arch / 'install-package.sh').exists(),
        'Obsolete AUR CMake compatibility installer was reintroduced')

require('pkgname=viper4linux-re\n' in stable, 'Wrong stable package name')
require("license=('GPL-3.0-only' 'LicenseRef-ViPERDSP')" in stable,
        'Arch package must declare GPL-3.0-only plus third-party notice')
require("options=('!debug')" in stable, 'Stable package must not produce debug-symbol package')
require(license_text.startswith('GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n'),
        'Missing GNU GPL v3 text')
require('Gerald Frank (username227)' in copyright_notice and
        'GPL-3.0-only' in copyright_notice, 'Project author and license missing')
require('Gerald Frank' in readme and 'GPL-3.0-only' in readme,
        'User-visible author/license attribution missing')
require('GPL-3.0-only' in third_party_notices and 'ViPERDSP' in third_party_notices,
        'Third-party licensing boundary missing')
runtime = {'glibc', 'gcc-libs', 'qt6-base', 'pipewire', 'ladspa', 'python'}
build = {'cmake', 'pkgconf'}  # gcc/make/binutils belong to Arch base-devel
require(runtime <= bash_array(stable, 'depends'), 'Stable runtime dependencies incomplete')
require(build <= bash_array(stable, 'makedepends'), 'Stable build dependencies incomplete')
require("conflicts=('viper4linux-re-git' 'viper4linux' 'viper4linux-git')" in stable,
        'Stable package conflicts changed')
require(not re.search(r'^\s*replaces=', stable, re.M), 'Unwanted replaces=')
require('jamesdsp' not in stable.lower().split('conflicts=(', 1)[-1].split(')', 1)[0],
        'JamesDSP must not be marked as a package conflict')

# CMake exclusively owns the build and source-checkout requirements.
require(not (root / 'install.sh').exists(),
        'Obsolete source installer was reintroduced')
require('find_package(Qt6 6.4 COMPONENTS Widgets Network REQUIRED)' in cmake,
        'Qt6 Widgets/Network CMake dependency changed')
require('Qt6::Widgets Qt6::Network' in cmake, 'Qt6 GUI link dependencies changed')
require('find_package(Threads REQUIRED)' in cmake, 'Thread dependency changed')
require('#include <ladspa.h>' in (root / 'src/viper-ladspa.cpp').read_text(),
        'LADSPA interface changed; review dependency')
for required in ('find_package(Python3 REQUIRED COMPONENTS Interpreter)',
                 'rev-parse HEAD:third_party/ViPERDSP',
                 'ViPERDSP submodule revision mismatch',
                 'VIPERDSP_PREPARED_SOURCE_DIR',
                 'file(SHA256'):
    require(required in prepare, f'CMake pinned DSP preparation missing: {required}')
for required in ('VIPER_SOURCE_PREPARE_ROUTING',
                 'VIPER_SOURCE_ROUTING_TEMPLATE',
                 'VIPER_ENABLE_SOURCE_LINK_TARGET',
                 'viper_source_check', 'viper_source_links',
                 'ladspa_descriptor', 'viper_dsp_ladspa.so'):
    require(required in source_setup, f'CMake developer setup missing: {required}')

pacman = re.search(r'(?m)^sudo pacman -S --needed ([^\n]+)$', readme)
require(pacman, 'Arch source-install dependency command missing from README')
source_packages = set(shlex.split(pacman.group(1)))
require({'base-devel', 'cmake', 'git', 'pkgconf', 'ladspa', 'pipewire', 'qt6-base', 'python'} <= source_packages,
        'README source-install dependency list incomplete')

# Both stable and VCS recipes install through CMake-owned rules; the VCS
# recipe itself lives in the separately maintained AUR Git checkout.
for required in ('package() {', 'check() {', 'sha256sums=(',
                 'releases/download/v${pkgver}', 'bash tests/check-package.sh',
                 'DESTDIR="$pkgdir" cmake --install "$srcdir/viper4linux-build"',
                 '-DVIPER_ENABLE_SYSTEM_INSTALL=ON', '-DVIPER_SYSTEM_INSTALL_LAYOUT=arch',
                 '-DVIPER_SYSTEM_PACKAGE_NAME="$pkgname"'):
    require(required in stable, f'Stable PKGBUILD CMake contract missing: {required}')
for forbidden in ('VIPER_ARCH_STAGING_BACKEND', 'install-package.sh',
                  'ViPERDSP-patched', 'fix_lufs_speed.py'):
    require(forbidden not in stable, f'Legacy stable packaging path remains: {forbidden}')
require('install -Dm' not in stable, 'Stable recipe must delegate staging to CMake')
require('bash tests/check-mock.sh' not in stable, 'Full developer tests must not run during makepkg')
for patch in ('fix_lufs_speed.py', 'preallocate_stereo_imager.py',
              'preallocate_multiband_compressor.py'):
    require(patch in (root / 'cmake/PrepareViPERDSP.cmake').read_text(),
            f'CMake-owned DSP patch missing: {patch}')
require('git submodule update --init --checkout' in
        (root / 'docs/DEVELOPMENT.md').read_text(),
        'Documented pinned submodule initialization missing')
modules = (root / '.gitmodules')
if modules.is_file():
    data = modules.read_text()
    require('path = third_party/ViPERDSP' in data and
            'https://github.com/likelikeslike/ViPERDSP.git' in data,
            'Pinned ViPERDSP submodule declaration changed')

smoke = (root / 'tests/check-package.sh').read_text()
developer = (root / 'tests/check-developer.sh').read_text()
for required in ('python3 tests/check-launcher.py', 'python3 tests/check-arch-install.py',
                 'python3 tests/check-cmake-source-checkout.py',
                 "python3 -m unittest discover -s tests -p 'test_output_*.py'",
                 'python3 tests/check-packaging-contract.py'):
    require(required in smoke, f'Package smoke test missing: {required}')
require('bash tests/check-mock.sh' in developer and
        'python3 tests/check-native-state-contract.py' in developer,
        'Developer regression coverage changed')
# The repository deliberately does not duplicate the independently maintained
# AUR -git PKGBUILD. The maintainer confirmed a clean -git makepkg -Cf build
# using the same options and direct DESTDIR staging before adapter retirement.
system = (root / 'cmake/InstallSystem.cmake').read_text()
arch_rules = system.split('if(VIPER_SYSTEM_INSTALL_LAYOUT STREQUAL "arch")', 1)[1].split('else()', 1)[0]
require('install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"' not in arch_rules,
        'Arch must not package a duplicate standard GPL license')
require('ViPERDSP-NOTICE.md' in arch_rules, 'Missing custom upstream license notice')
require('Copyright (C) 2026 Gerald Frank (username227)' in system,
        'Debian metadata must credit original author')
for required in ('src/viper_launcher.py', 'src/viper_output_monitor.py',
                 'file(GLOB _viper_runtime_python', 'Missing package source:',
                 'viper4linux-output-state', 'viper4linux-output',
                 'ViPERDSP-NOTICE.md', 'v4l-cli.1', 'VIPER_SYSTEM_PACKAGE_NAME'):
    require(required in system, f'CMake install contract missing: {required}')
require('viper-output-state' in cmake, 'Native output-state binary missing from CMake')
for path in ('tests/check-package.sh', 'tests/check-developer.sh',
             'cmake/SourceCheckout.cmake', 'tests/check-cmake-source-checkout.py', 'LICENSE', 'COPYRIGHT',
             'THIRD-PARTY-NOTICES.md', 'packaging/arch/viper4linux.desktop'):
    require((root / path).is_file(), f'Missing project file: {path}')
require('SYSTEM_MODE' in launcher and "Path('/usr/bin/viper4linux-gui')" in launcher,
        'System launcher mode changed')
require('SYSTEM_BACKEND' in launcher and 'SYSTEM_ROUTING' in launcher,
        'Installed backend configuration changed')
for required in ('VIPER_BACKEND_FINGERPRINT', 'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1',
                 'PIPEWIRE_CONFIG_DIR="$config"', '/usr/share/pipewire/filter-chain.conf',
                 '/usr/share/viper4linux/routing-config/filter-chain.conf.d'):
    require(required in backend, f'System backend contract missing: {required}')

print('PASS: native packaging depends on CMake-owned DSP preparation, source build, install and launcher contracts')
