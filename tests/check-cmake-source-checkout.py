#!/usr/bin/env python3
"""Isolated CMake developer setup tests: no package install, audio or user home."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'cmake/SourceCheckout.cmake'


def run(*argv, env=None, success=True):
    result = subprocess.run(argv, env=env, capture_output=True, text=True)
    if success:
        assert result.returncode == 0, (argv, result.stdout, result.stderr)
    else:
        assert result.returncode != 0, (argv, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='viper cmake source checkout ') as tmp:
    base = Path(tmp)
    source = base / 'source checkout'
    (source / 'routing-config/filter-chain.conf.d').mkdir(parents=True)
    (source / 'routing-config/filter-chain.conf.d/99-viper-smart.conf').write_text('source smart fragment\n')
    launcher = source / 'viper4linux'
    launcher.write_text('#!/bin/sh\nexit 0\n')
    launcher.chmod(0o755)
    (source / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.20)\n'
        'project(DevFixture NONE)\n'
        'option(VIPER_ENABLE_SYSTEM_INSTALL "" OFF)\n'
        'option(VIPER_SOURCE_VERIFY_LADSPA_EXPORT "" OFF)\n'
        'foreach(name IN ITEMS viper-gui viper-ctl viper-output-state viper_dsp_ladspa)\n'
        '  add_custom_target(${name})\n'
        'endforeach()\n'
        f'include("{MODULE}")\n')
    template = base / 'stock filter chain.conf'
    template.write_text('stock filter-chain\n')
    home = base / 'private home'
    home.mkdir()
    bin_dir = home / '.local/bin'
    env = {**os.environ, 'HOME': str(home), 'XDG_BIN_HOME': str(bin_dir)}
    build = base / 'build output'
    common = ['cmake', '-S', str(source), '-B', str(build),
              '-DVIPER_SOURCE_ROUTING_TEMPLATE=' + str(template)]
    run(*common, env=env)
    assert (build / 'routing-config/filter-chain.conf').read_text() == template.read_text()
    fragment = build / 'routing-config/filter-chain.conf.d/99-viper-smart.conf'
    assert fragment.read_text() == 'source smart fragment\n'
    assert not (source / 'routing-config/filter-chain.conf').exists(), 'CMake wrote into source tree'
    assert not bin_dir.exists(), 'Normal configure must not create command aliases'
    failed = run('cmake', '--build', str(build), '--target', 'viper_source_check',
                 env=env, success=False)
    assert 'Missing source build output' in failed.stdout + failed.stderr

    for name in ('viper-gui', 'viper-ctl', 'viper-output-state', 'viper_dsp_ladspa.so'):
        output = build / name
        output.write_text(f'binary fixture {name}\n')
        output.chmod(0o755)
    run('cmake', '--build', str(build), '--target', 'viper_source_check', env=env)
    assert not bin_dir.exists(), 'Read-only check target created private links'

    # An existing customized source-tree configuration must take priority.
    custom = source / 'routing-config/filter-chain.conf'
    custom.write_text('developer custom config\n')
    template.write_text('stock template updated\n')
    run(*common, env=env)
    assert custom.read_text() == 'developer custom config\n'
    assert (build / 'routing-config/filter-chain.conf').read_text() == template.read_text()
    run('cmake', '--build', str(build), '--target', 'viper_source_check', env=env)

    # Command-alias target is not present until explicitly enabled.
    run('cmake', '--build', str(build), '--target', 'viper_source_links',
        env=env, success=False)
    run(*common, '-DVIPER_ENABLE_SOURCE_LINK_TARGET=ON', env=env)
    run('cmake', '--build', str(build), '--target', 'viper_source_links', env=env)
    assert os.readlink(bin_dir / 'viper4linux') == str(launcher)
    assert os.readlink(bin_dir / 'v4l-cli') == str(build / 'viper-ctl')
    run('cmake', '--build', str(build), '--target', 'viper_source_links', env=env)

    # Collision checks occur before creating either shortcut.
    collision_dir = home / 'collision bin'
    collision_dir.mkdir()
    (collision_dir / 'viper4linux').write_text('user command\n')
    bad = run('cmake', '-DVIPER_SOURCE_ACTION=links',
              '-DVIPER_SOURCE_ROOT=' + str(source),
              '-DVIPER_SOURCE_BUILD_DIR=' + str(build),
              '-DVIPER_SOURCE_HOME=' + str(home),
              '-DVIPER_SOURCE_LINK_DIRECTORY=' + str(collision_dir),
              '-P', str(MODULE), env=env, success=False)
    assert 'Unrelated command already exists' in bad.stderr
    assert not (collision_dir / 'v4l-cli').exists()

    for unsafe in (str(base / 'outside private home'), '/usr/bin', 'relative/path'):
        bad = run('cmake', '-DVIPER_SOURCE_ACTION=links',
                  '-DVIPER_SOURCE_ROOT=' + str(source),
                  '-DVIPER_SOURCE_BUILD_DIR=' + str(build),
                  '-DVIPER_SOURCE_HOME=' + str(home),
                  '-DVIPER_SOURCE_LINK_DIRECTORY=' + unsafe,
                  '-P', str(MODULE), env=env, success=False)
        assert 'Refusing' in bad.stderr or 'absolute' in bad.stderr, bad.stderr

    outside = base / 'target outside'
    outside.mkdir()
    linked_dir = home / 'symlinked bin'
    linked_dir.symlink_to(outside, target_is_directory=True)
    bad = run('cmake', '-DVIPER_SOURCE_ACTION=links',
              '-DVIPER_SOURCE_ROOT=' + str(source),
              '-DVIPER_SOURCE_BUILD_DIR=' + str(build),
              '-DVIPER_SOURCE_HOME=' + str(home),
              '-DVIPER_SOURCE_LINK_DIRECTORY=' + str(linked_dir),
              '-P', str(MODULE), env=env, success=False)
    assert 'symlinked command directory' in bad.stderr, bad.stderr
    assert not list(outside.iterdir())

    (build / 'viper-ctl').unlink()
    bad = run('cmake', '--build', str(build), '--target', 'viper_source_links',
              env=env, success=False)
    assert 'Missing source build output' in bad.stdout + bad.stderr

    absent = base / 'no template build'
    bad = run('cmake', '-S', str(source), '-B', str(absent),
              '-DVIPER_SOURCE_ROUTING_TEMPLATE=' + str(base / 'missing template'),
              env=env, success=False)
    assert 'Missing PipeWire source-routing template' in bad.stdout + bad.stderr

    # Distribution package builds need no developer host filter-chain template.
    packaged = base / 'package build'
    run('cmake', '-S', str(source), '-B', str(packaged),
        '-DVIPER_ENABLE_SYSTEM_INSTALL=ON',
        '-DVIPER_SOURCE_ROUTING_TEMPLATE=' + str(base / 'missing template'), env=env)
    assert not (packaged / 'routing-config').exists()

print('PASS: CMake source routing, verification target, opt-in aliases and collision-safe setup')
