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
installer = (arch / 'install-package.sh').read_text()
source_install = (root / 'install.sh').read_text()
cmake = (root / 'CMakeLists.txt').read_text()
readme = (root / 'README.md').read_text()
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
for script in (stable_path, arch / 'install-package.sh', root / 'install.sh'):
    subprocess.run(['bash', '-n', str(script)], check=True)

require('pkgname=viper4linux-re\n' in stable, 'Wrong stable package name')
runtime = {'glibc', 'gcc-libs', 'qt6-base', 'pipewire', 'ladspa', 'python'}
build = {'cmake', 'pkgconf'}  # gcc/make/binutils belong to Arch base-devel
require(runtime <= bash_array(stable, 'depends'), 'Stable runtime dependencies incomplete')
require(build <= bash_array(stable, 'makedepends'), 'Stable build dependencies incomplete')
require("conflicts=('viper4linux-re-git' 'viper4linux' 'viper4linux-git')" in stable,
        'Stable package conflicts changed')
require(not re.search(r'^\s*replaces=', stable, re.M), 'Unwanted replaces=')
require('jamesdsp' not in stable.lower().split('conflicts=(', 1)[-1].split(')', 1)[0],
        'JamesDSP must not be marked as a package conflict')

# Contract between declared dependencies, actual compilation, and the direct
# source installer. A successful makepkg is not a substitute for this check.
require('find_package(Qt6 6.4 COMPONENTS Widgets Network REQUIRED)' in cmake,
        'Qt6 Widgets/Network CMake dependency changed; review package requirements')
require('Qt6::Widgets Qt6::Network' in cmake, 'Qt6 GUI link dependencies changed')
require('find_package(Threads REQUIRED)' in cmake, 'Thread dependency changed')
require('#include <ladspa.h>' in (root / 'src/viper-ladspa.cpp').read_text(),
        'LADSPA interface changed; review dependency')
probe = re.search(r'(?m)^for tool in ([^;\n]+); do$', source_install)
require(probe, 'Direct installer tool preflight loop missing')
checked_tools = set(probe.group(1).split())
for tool in ('cmake', 'g++', 'gcc', 'pkg-config', 'pipewire', 'python3', 'nm'):
    require(tool in checked_tools, f'Direct installer no longer checks required tool {tool}')
for item in ('/usr/include/ladspa.h', 'pkg-config --exists Qt6Widgets',
             'pkg-config --exists Qt6Network', '/usr/share/pipewire/filter-chain.conf'):
    require(item in source_install, f'Direct installer dependency probe missing: {item}')

pacman = re.search(r'(?m)^sudo pacman -S --needed ([^\n]+)$', readme)
require(pacman, 'Arch source-install dependency command missing from README')
source_packages = set(shlex.split(pacman.group(1)))
require({'base-devel', 'cmake', 'git', 'pkgconf', 'ladspa', 'pipewire', 'qt6-base', 'python'} <= source_packages,
        'README source-install dependency list incomplete')

# Stable and VCS builds share this staging script; do not reintroduce a
# per-PKGBUILD installation file list. The VCS version's recipe stays in AUR.
for required in ('package() {', 'check() {', 'sha256sums=(',
                 'releases/download/v${pkgver}', 'packaging/arch/install-package.sh',
                 'ViPERDSP-patched', '-DVIPERDSP_SOURCE_DIR=', 'bash tests/check-package.sh'):
    require(required in stable, f'Stable PKGBUILD contract missing: {required}')
require('install -Dm' not in stable, 'Stable recipe must delegate staging to installer')
require(stable.count('bash "$tree/packaging/arch/install-package.sh" "$tree" "$built" "$pkgdir" "$pkgname"') == 1,
        'Stable recipe must invoke shared staging exactly once')
require('bash tests/check-mock.sh' not in stable, 'Full developer tests must not run during makepkg')
for patch in ('fix_lufs_speed.py', 'preallocate_stereo_imager.py',
              'preallocate_multiband_compressor.py'):
    require(patch in stable and patch in source_install, f'Native DSP patch missing: {patch}')
require('git submodule update --init --checkout' in source_install,
        'Source installer no longer checks pinned DSP')
modules = (root / '.gitmodules')
if modules.is_file():
    data = modules.read_text()
    require('path = third_party/ViPERDSP' in data and
            'https://github.com/likelikeslike/ViPERDSP.git' in data,
            'Pinned ViPERDSP submodule declaration changed')

smoke = (root / 'tests/check-package.sh').read_text()
developer = (root / 'tests/check-developer.sh').read_text()
for required in ('python3 tests/check-launcher.py', 'python3 tests/check-arch-install.py',
                 "python3 -m unittest discover -s tests -p 'test_output_*.py'",
                 'python3 tests/check-packaging-contract.py'):
    require(required in smoke, f'Package smoke test missing: {required}')
require('bash tests/check-mock.sh' in developer and
        'python3 tests/check-native-state-contract.py' in developer,
        'Developer regression coverage changed')
for required in ('ln -s -- viper4linux-ctl "$pkgdir/usr/bin/v4l-cli"',
                 'ln -s -- viper4linux-ctl.1 "$pkgdir/usr/share/man/man1/v4l-cli.1"',
                 'usr/share/licenses/$pkgname/LICENSE', 'usr/lib/ladspa/viper_dsp_ladspa.so',
                 'viper4linux-output-state', 'viper4linux-output', 'src/viper_*.py',
                 'viper_output_monitor.py', 'Missing package source:'):
    require(required in installer, f'Installation contract missing: {required}')
require('viper-output-state' in cmake, 'Native output-state binary missing from CMake')
for path in ('tests/check-package.sh', 'tests/check-developer.sh', 'LICENSE',
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

print('PASS: stable Arch dependencies, direct installer probes, DSP source, staging and launcher contracts')
