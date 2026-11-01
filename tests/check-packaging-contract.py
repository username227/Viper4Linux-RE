#!/usr/bin/env python3
"""Read-only packaging checks; no system files or real PipeWire are touched."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
pkg = (root / 'packaging/arch/PKGBUILD').read_text()
launcher = (root / 'src/viper_launcher.py').read_text()
backend = (root / 'packaging/arch/start-backend').read_text()
for required in ("pkgname=viper4linux-re-git", 'pkgver() {',
                 'git rev-list --count HEAD', 'git rev-parse --short=12 HEAD',
                 'git+file://', 'Desktop/viper4linux-presets', 'ViPERDSP::git+https://',
                 'sha256sums=(\'SKIP\' \'SKIP\')', 'package() {',
                 'check() {', 'install -Dm755', 'usr/lib/ladspa/viper_dsp_ladspa.so'):
    assert required in pkg, required
assert "conflicts=('viper4linux-re' 'viper4linux' 'viper4linux-git')" in pkg
assert "provides=('viper4linux-re')" in pkg
assert not re.search(r'^\s*replaces=', pkg, flags=re.M)
assert 'jamesdsp' not in re.findall(r'^\s*conflicts=.*', pkg, flags=re.M)[0].lower()
assert 'SYSTEM_MODE' in launcher and "Path('/usr/bin/viper4linux-gui')" in launcher
assert 'SYSTEM_BACKEND' in launcher and 'SYSTEM_ROUTING' in launcher
assert 'VIPER_BACKEND_FINGERPRINT' in backend
assert 'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1' in backend
assert 'PIPEWIRE_CONFIG_DIR="$config"' in backend
assert '/usr/share/pipewire/filter-chain.conf' in backend
assert '/usr/share/viper4linux/routing-config/filter-chain.conf.d' in backend
assert (root/'packaging/arch/viper4linux.desktop').is_file()
print('PASS: Arch VCS PKGBUILD, system layout, fingerprinted launcher and session config')
