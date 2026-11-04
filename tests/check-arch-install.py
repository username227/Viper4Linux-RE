#!/usr/bin/env python3
"""Offline packaging-layout regression checks; no real system install."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'packaging/arch/install-package.sh'
INSTALL = ROOT / 'cmake/InstallSystem.cmake'


def invoke(tree: Path, build: Path, dest: Path, pkgname='viper4linux-re-git'):
    return subprocess.run(['bash', str(SCRIPT), str(tree), str(build), str(dest), pkgname],
                          text=True, capture_output=True)


with tempfile.TemporaryDirectory(prefix='viper arch packaging ') as tmp:
    root = Path(tmp)
    tree, build, dest = (root / part for part in ('source tree', 'built outputs', 'staged package'))
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

    # The AUR compatibility adapter requires a pre-existing CMake build.
    # A fixture with imported targets exercises the *real* install rules,
    # without compiling, starting audio or writing outside temp directories.
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
        'if(VIPER_ENABLE_SYSTEM_INSTALL)\n'
        f'  include("{INSTALL}")\n'
        'endif()\n'
    )
    configured = subprocess.run(['cmake', '-S', str(tree), '-B', str(build)],
                                text=True, capture_output=True)
    assert configured.returncode == 0, configured.stdout + configured.stderr
    result = invoke(tree, build, dest)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'Installing:' in result.stdout or 'Up-to-date:' in result.stdout, result.stdout
    for name in ['viper_launcher.py', 'viper_output_graph.py', 'viper_output_store.py',
                 'viper_output_memory.py', 'viper_output_monitor.py',
                 'viper_output_info.py', 'viper_future_feature.py']:
        output = dest / 'usr/lib/viper4linux' / name
        assert output.read_bytes() == (tree / 'src' / name).read_bytes(), name
        assert output.stat().st_mode & 0o777 == 0o644
    assert (dest / 'usr/bin/viper4linux-output-state').is_file()
    assert (dest / 'usr/bin/viper4linux-output').is_file()
    assert os.readlink(dest / 'usr/bin/v4l-cli') == 'viper4linux-ctl'
    assert os.readlink(dest / 'usr/share/man/man1/v4l-cli.1') == 'viper4linux-ctl.1'
    assert (dest / 'usr/lib/ladspa/viper_dsp_ladspa.so').stat().st_mode & 0o777 == 0o755
    assert (dest / 'usr/share/doc/viper4linux/docs/guide.md').read_text() == 'example\n'
    assert (dest / 'usr/share/licenses/viper4linux-re-git/LICENSE').is_file()

    # Stable and VCS package names use identical CMake staging apart from license path.
    stable = root / 'stable stage'
    result = invoke(tree, build, stable, 'viper4linux-re')
    assert result.returncode == 0, result.stderr
    assert (stable / 'usr/share/licenses/viper4linux-re/LICENSE').is_file()

    # Missing a required module must fail before staging anything at all.
    (tree / 'src/viper_output_monitor.py').unlink()
    absent = root / 'broken stage'
    result = invoke(tree, build, absent)
    assert result.returncode != 0 and 'Missing package source' in result.stderr, result.stderr
    assert not absent.exists()

    # Reject accidental package-to-root installs.
    result = invoke(tree, build, Path('/'))
    assert result.returncode != 0 and 'Destination must be' in result.stderr

print('PASS: AUR compatibility adapter delegates to CMake with dynamic Python, correct package modes and fail-closed staging')
