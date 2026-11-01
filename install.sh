#!/usr/bin/env bash
# Build a self-contained application source tree from a completely EMPTY CMake build dir.
# This script never touches ~/Desktop/viper-for-linux or user PipeWire services.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
for tool in git cmake g++ gcc pkg-config pipewire python3 nm; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
[[ -r /usr/include/ladspa.h ]] || { echo 'Install Arch package ladspa' >&2; exit 1; }
pkg-config --exists Qt6Widgets || { echo 'Install Arch package qt6-base' >&2; exit 1; }
[[ -r /usr/share/pipewire/filter-chain.conf ]] || {
  echo 'Missing /usr/share/pipewire/filter-chain.conf (Arch PipeWire package)' >&2; exit 1;
}
# Protect against inadvertently mixing the new tree with an old one.
if [[ -e build/CMakeCache.txt || -e build/viper-gui || -e build/viper_dsp_ladspa.so ]]; then
  echo 'Build directory already exists. This installer requires a pristine build directory.' >&2
  echo 'For a fresh attempt, copy/extract this project to a new empty directory.' >&2
  exit 1
fi
# Before cloning, compile and run the INCLUDED isolated DSP regression suite.
echo '== Isolated DSP and native Convolver regression tests =='
bash ./tests/check-mock.sh
python3 ./tests/check-launcher.py
# Fetch the exact pinned upstream engine. The independent source tree contains
# all OUR code, scripts, tests and configuration; ViPERDSP is a third-party dep.
readonly revision=6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933
if [[ ! -e third_party/ViPERDSP/.git ]]; then
  if [[ -e third_party/ViPERDSP ]]; then
    echo 'Unexpected non-git third_party/ViPERDSP; will not overwrite.' >&2; exit 1
  fi
  git clone --depth 50 https://github.com/likelikeslike/ViPERDSP.git third_party/ViPERDSP
fi
if ! git -C third_party/ViPERDSP cat-file -e "${revision}^{commit}" 2>/dev/null; then
  git -C third_party/ViPERDSP fetch --unshallow origin 2>/dev/null || git -C third_party/ViPERDSP fetch origin
fi
git -C third_party/ViPERDSP cat-file -e "${revision}^{commit}" || {
    echo 'Pinned ViPERDSP commit not found; refusing to use an unverified release.' >&2; exit 1;
}
git -C third_party/ViPERDSP checkout --detach "$revision"
[[ $(git -C third_party/ViPERDSP rev-parse HEAD) == "$revision" ]] || {
    echo 'Upstream engine revision mismatch.' >&2; exit 1;
}
# Correct the pinned library's LUFS speed-setting logic only in this local
# dependency checkout; refuse any unexpected upstream source.
python3 patches/fix_lufs_speed.py
python3 patches/preallocate_stereo_imager.py
# Use a private per-project copy of the distribution filter-chain config so
# nothing gets added to /etc/pipewire or ~/.config/pipewire.
cp -- /usr/share/pipewire/filter-chain.conf routing-config/filter-chain.conf
mkdir -p build
[[ ! -e build/CMakeCache.txt ]] || { echo 'Build not clean.' >&2; exit 1; }
echo '== Fresh native CMake configure and compile =='
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target viper_dsp_ladspa viper-ctl viper-gui -j4
for item in build/viper_dsp_ladspa.so build/viper-ctl build/viper-gui; do
    [[ -s "$item" ]] || { echo "Missing expected compiled binary: $item" >&2; exit 1; }
done
nm -D build/viper_dsp_ladspa.so | grep -Eq ' [TW] ladspa_descriptor$' || {
  echo 'LADSPA module missing ladspa_descriptor.' >&2; exit 1;
}
for old in build/viperbass_ladspa.so build/viperbass-ctl build/viperbass-gui; do
  [[ ! -e "$old" ]] || { echo "Old filename present: $old" >&2; exit 1; }
done
echo '== Repeating regression tests after native build =='
bash ./tests/check-mock.sh
printf '\nCOMPLETE: isolated regression suite and fresh native build succeeded.\n'
# Expose a plain `viper4linux` command when ~/.local/bin is available. Never
# overwrite an unrelated program owned by the user. Fresh-tree installations
# replace the symlink from an older Viper4Linux source checkout automatically.
launcher_dir=${XDG_BIN_HOME:-$HOME/.local/bin}
launcher_link=$launcher_dir/viper4linux
mkdir -p -- "$launcher_dir"
if [[ ! -e $launcher_link && ! -L $launcher_link ]]; then
    ln -s -- "$PWD/viper4linux" "$launcher_link"
elif [[ -L $launcher_link ]]; then
    old_target=$(readlink -f -- "$launcher_link" 2>/dev/null || true)
    if [[ $old_target == "$PWD/viper4linux" || $old_target == */viper-for-linux-*/viper4linux || $old_target == */viper4linux/viper4linux ]]; then
        ln -sfn -- "$PWD/viper4linux" "$launcher_link"
    else
        echo "Launcher already exists at $launcher_link; not replacing an unrelated symlink." >&2
    fi
else
    echo "Launcher already exists at $launcher_link; not replacing a user-owned command." >&2
fi
if [[ :$PATH: != *:$launcher_dir:* ]]; then
    echo "Note: $launcher_dir is not on PATH; use ./viper4linux or add that directory to PATH." >&2
fi
printf 'Launch with viper4linux (or ./viper4linux from the source directory).\n'
printf 'Use ./verify.sh while Rhythmbox is playing.\n'
