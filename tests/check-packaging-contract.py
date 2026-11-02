#!/usr/bin/env python3
"""Read-only packaging checks; no system files or real PipeWire are touched."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
pkg = (root / 'packaging/arch/PKGBUILD').read_text()
stable = (root / 'packaging/arch/PKGBUILD.stable.in').read_text()
installer = (root / 'packaging/arch/install-package.sh').read_text()
launcher = (root / 'src/viper_launcher.py').read_text()
backend = (root / 'packaging/arch/start-backend').read_text()
for required in ("pkgname=viper4linux-re-git", 'pkgver() {',
                 'git rev-list --count HEAD', 'git rev-parse --short=12 HEAD',
                 'github.com/username227/viper4linux-re.git', 'ViPERDSP::git+https://',
                 'git submodule init', 'submodule.third_party/ViPERDSP.url',
                 'protocol.file.allow=always submodule update', 'ViPERDSP-patched',
                 'VIPERDSP_SOURCE_DIR=',
                 'sha256sums=(\'SKIP\'', 'package() {',
                 'check() {', 'packaging/arch/install-package.sh'):
    assert required in pkg, required
assert "conflicts=('viper4linux-re' 'viper4linux' 'viper4linux-git')" in pkg
assert "provides=('viper4linux-re')" in pkg
assert not re.search(r'^\s*replaces=', pkg, flags=re.M)
assert 'jamesdsp' not in re.findall(r'^\s*conflicts=.*', pkg, flags=re.M)[0].lower()
assert (root / '.gitmodules').read_text().find('path = third_party/ViPERDSP') >= 0
assert (root / '.gitmodules').read_text().find('https://github.com/likelikeslike/ViPERDSP.git') >= 0
assert 'ln -sfn' not in pkg
assert 'install -Dm' not in pkg, 'PKGBUILD must delegate file layout to install script'
assert pkg.count('bash "$tree/packaging/arch/install-package.sh" "$tree" "$built" "$pkgdir" "$pkgname"') == 1
assert stable.count('bash "$tree/packaging/arch/install-package.sh" "$tree" "$built" "$pkgdir" "$pkgname"') == 1
assert 'install -Dm' not in stable
assert 'bash tests/check-package.sh' in pkg
assert 'bash tests/check-package.sh' in stable
assert 'bash tests/check-mock.sh' not in pkg
assert 'bash tests/check-mock.sh' not in stable
smoke = (root / 'tests/check-package.sh').read_text()
developer = (root / 'tests/check-developer.sh').read_text()
assert 'python3 tests/check-launcher.py' in smoke
assert 'python3 tests/check-arch-install.py' in smoke
assert "python3 -m unittest discover -s tests -p 'test_output_*.py'" in smoke
assert 'bash tests/check-mock.sh' in developer
assert 'python3 tests/check-native-state-contract.py' in developer
assert 'ln -s -- viper4linux-ctl "$pkgdir/usr/bin/v4l-cli"' in installer
assert 'ln -s -- viper4linux-ctl.1 "$pkgdir/usr/share/man/man1/v4l-cli.1"' in installer
assert 'usr/share/licenses/$pkgname/LICENSE' in installer
assert 'usr/lib/ladspa/viper_dsp_ladspa.so' in installer
assert 'viper-output-state' in (root / 'CMakeLists.txt').read_text()
assert 'viper4linux-output-state' in installer
assert 'viper4linux-output' in installer
assert (root / 'tests/check-package.sh').is_file()
assert (root / 'tests/check-developer.sh').is_file()
assert 'src/viper_*.py' in installer
assert 'viper_output_monitor.py' in installer
assert 'Missing package source:' in installer
assert (root / 'LICENSE').is_file()
assert (root / 'THIRD-PARTY-NOTICES.md').is_file()
assert 'git -C third_party/ViPERDSP archive HEAD' in pkg
assert '-DVIPERDSP_SOURCE_DIR=' in pkg
assert 'SYSTEM_MODE' in launcher and "Path('/usr/bin/viper4linux-gui')" in launcher
assert 'SYSTEM_BACKEND' in launcher and 'SYSTEM_ROUTING' in launcher
assert 'VIPER_BACKEND_FINGERPRINT' in backend
assert 'VIPER_CLEAN_INSTANCE=viperlinux-unified-v1' in backend
assert 'PIPEWIRE_CONFIG_DIR="$config"' in backend
assert '/usr/share/pipewire/filter-chain.conf' in backend
assert '/usr/share/viper4linux/routing-config/filter-chain.conf.d' in backend
assert (root/'packaging/arch/viper4linux.desktop').is_file()
print('PASS: unified Arch VCS/stable installation, source checks, fingerprinted launcher and session config')
