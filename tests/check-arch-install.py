#!/usr/bin/env python3
"""Offline direct-CMake Arch staging checks; never install on the host."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INSTALL = ROOT / 'cmake/InstallSystem.cmake'


def configure(tree: Path, build: Path, pkgname='viper4linux-re-git'):
    return subprocess.run(
        ['cmake', '-S', str(tree), '-B', str(build),
         '-DCMAKE_INSTALL_PREFIX=/usr',
         '-DVIPER_ENABLE_SYSTEM_INSTALL=ON',
         '-DVIPER_SYSTEM_INSTALL_LAYOUT=arch',
         '-DVIPER_SYSTEM_PACKAGE_NAME=' + pkgname],
        text=True, capture_output=True,
    )


def stage(build: Path, dest: Path):
    return subprocess.run(
        ['cmake', '--install', str(build)],
        env={**os.environ, 'DESTDIR': str(dest)},
        text=True, capture_output=True,
    )


with tempfile.TemporaryDirectory(prefix='viper arch cmake packaging ') as tmp:
    root = Path(tmp)
    tree, build, dest = (root / part for part in
                         ('source tree', 'built outputs', 'staged git package'))
    tree.mkdir()
    build.mkdir()
    (tree / 'docs').mkdir()
    (tree / 'docs/guide.md').write_text('example\n')
    paths = [
        'src/viper_launcher.py', 'src/viper_output_graph.py',
        'src/viper_output_store.py', 'src/viper_output_memory.py',
        'src/viper_output_monitor.py', 'src/viper_output_info.py',
        'src/viper_future_feature.py',  # Must be installed without editing a recipe.
        'packaging/arch/start-backend', 'packaging/arch/viper4linux',
        'packaging/arch/viper4linux-output',
        'packaging/arch/viper4linux.desktop',
        'routing-config/filter-chain.conf.d/99-viper-smart.conf',
        'resources/viper4linux-blue.png', 'resources/viper4linux-blue.svg',
        'man/viper4linux.1', 'man/viper4linux-ctl.1',
        'LICENSE', 'THIRD-PARTY-NOTICES.md', 'README.md',
    ]
    for name in paths:
        file = tree / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(f'marker for {name}\n')
    for name in ['viper-gui', 'viper-ctl', 'viper-output-state', 'viper_dsp_ladspa.so']:
        (build / name).write_bytes(b'example binary')

    # Imported mock binaries exercise the actual production CMake installation
    # rules, not a reimplementation. Never compile or touch live audio here.
    (tree / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.20)\n'
        'project(ArchStageFixture LANGUAGES C CXX)\n'
        'foreach(name IN ITEMS viper-gui viper-ctl viper-output-state)\n'
        '  add_executable(${name} IMPORTED GLOBAL)\n'
        '  set_target_properties(${name} PROPERTIES IMPORTED_LOCATION '
        '"${CMAKE_BINARY_DIR}/${name}")\n'
        'endforeach()\n'
        'add_library(viper_dsp_ladspa MODULE IMPORTED GLOBAL)\n'
        'set_target_properties(viper_dsp_ladspa PROPERTIES IMPORTED_LOCATION '
        '"${CMAKE_BINARY_DIR}/viper_dsp_ladspa.so")\n'
        f'include("{INSTALL}")\n'
    )
    configured = configure(tree, build)
    assert configured.returncode == 0, configured.stdout + configured.stderr
    dest.mkdir()
    result = stage(build, dest)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'Installing:' in result.stdout or 'Up-to-date:' in result.stdout
    for name in ['viper_launcher.py', 'viper_output_graph.py', 'viper_output_store.py',
                 'viper_output_memory.py', 'viper_output_monitor.py',
                 'viper_output_info.py', 'viper_future_feature.py']:
        output = dest / 'usr/lib/viper4linux' / name
        assert output.read_bytes() == (tree / 'src' / name).read_bytes(), name
        assert output.stat().st_mode & 0o777 == 0o644
    for name in ['viper4linux', 'viper4linux-ctl', 'viper4linux-gui',
                 'viper4linux-output', 'viper4linux-output-state']:
        assert (dest / 'usr/bin' / name).stat().st_mode & 0o777 == 0o755
    assert (dest / 'usr/lib/viper4linux/start-backend').stat().st_mode & 0o777 == 0o755
    assert os.readlink(dest / 'usr/bin/v4l-cli') == 'viper4linux-ctl'
    assert os.readlink(dest / 'usr/share/man/man1/v4l-cli.1') == 'viper4linux-ctl.1'
    assert (dest / 'usr/lib/ladspa/viper_dsp_ladspa.so').stat().st_mode & 0o777 == 0o755
    assert (dest / 'usr/share/doc/viper4linux/docs/guide.md').read_text() == 'example\n'
    assert not (dest / 'usr/share/licenses/viper4linux-re-git/LICENSE').exists()
    assert (dest / 'usr/share/licenses/viper4linux-re-git/ViPERDSP-NOTICE.md').is_file()

    # A second, independently configured CMake build stages the stable package.
    # The package-name option must not leak through a cached CMake configuration.
    stable_build = root / 'stable build'
    stable = root / 'stable stage'
    for name in ['viper-gui', 'viper-ctl', 'viper-output-state', 'viper_dsp_ladspa.so']:
        stable_build.mkdir(exist_ok=True)
        (stable_build / name).write_bytes(b'example binary')
    configured = configure(tree, stable_build, 'viper4linux-re')
    assert configured.returncode == 0, configured.stdout + configured.stderr
    stable.mkdir()
    result = stage(stable_build, stable)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not (stable / 'usr/share/licenses/viper4linux-re/LICENSE').exists()
    assert (stable / 'usr/share/licenses/viper4linux-re/ViPERDSP-NOTICE.md').is_file()
    assert not (stable / 'usr/share/licenses/viper4linux-re-git').exists()

    # Missing inputs fail at configure time, before anything can be staged.
    (tree / 'src/viper_output_monitor.py').unlink()
    broken_build = root / 'broken build'
    absent = root / 'broken stage'
    configured = configure(tree, broken_build)
    assert configured.returncode != 0, configured.stdout + configured.stderr
    assert 'Missing package source' in configured.stdout + configured.stderr
    assert not absent.exists()

    # A missing or symlinked DESTDIR must be rejected without writing outside
    # the disposable fixture. Unsafe host-root probes belong ONLY to Docker CI.
    missing = root / 'missing stage'
    result = stage(build, missing)
    assert result.returncode != 0 and 'DESTDIR' in result.stderr, result.stderr
    assert not missing.exists()
    alias = root / 'symlinked stage'
    alias.symlink_to(dest, target_is_directory=True)
    result = stage(build, alias)
    assert result.returncode != 0 and 'Symlinked DESTDIR' in result.stderr, result.stderr

print('PASS: direct CMake staging retains custom notices without redundant GPL copies, dynamic Python, '
      'modes, aliases, missing inputs and fail-closed private DESTDIR')
