#!/usr/bin/env bash
# Optional source-tree setup only. CMake builds the application; this helper
# never compiles, installs system files, or starts the PipeWire backend/GUI.
set -Eeuo pipefail

if (( $# != 1 )); then
    echo 'Usage: scripts/setup-source-checkout.sh --check|--prepare-routing|--link-commands' >&2
    exit 2
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
routing="$root/routing-config/filter-chain.conf"

check_build() {
    for binary in viper-gui viper-ctl viper-output-state; do
        [[ -x "$root/build/$binary" ]] || {
            echo "Missing source build output: build/$binary (run cmake -S . -B build && cmake --build build)" >&2
            return 1
        }
    done
    [[ -s "$root/build/viper_dsp_ladspa.so" && -x "$root/viper4linux" ]] || {
        echo 'Missing native LADSPA module or source launcher in build/.' >&2
        return 1
    }
}

case $1 in
    --check)
        check_build
        [[ -f "$routing" && ! -L "$routing" ]] || {
            echo 'Missing source routing template; use --prepare-routing.' >&2
            exit 1
        }
        echo 'PASS: source checkout build and routing files are ready (nothing modified).'
        ;;
    --prepare-routing)
        template=${VIPER_PIPEWIRE_FILTER_TEMPLATE:-/usr/share/pipewire/filter-chain.conf}
        [[ -r "$template" && -f "$template" ]] || {
            echo "PipeWire filter-chain template missing: $template" >&2
            exit 1
        }
        [[ -d "$root/routing-config" && ! -L "$root/routing-config" ]] || {
            echo 'Source routing-config directory missing or symbolic; refusing.' >&2
            exit 1
        }
        [[ ! -L "$routing" ]] || {
            echo "Refusing to replace symlink: $routing" >&2
            exit 1
        }
        if [[ -e "$routing" ]]; then
            [[ -f "$routing" ]] || {
                echo "Source routing path is not a regular file: $routing" >&2
                exit 1
            }
            echo 'Preserving existing source routing configuration.'
        else
            install -m 0644 -- "$template" "$routing"
            echo 'Created source routing configuration; did not touch /etc or ~/.config.'
        fi
        ;;
    --link-commands)
        check_build
        bindir=${XDG_BIN_HOME:-$HOME/.local/bin}
        [[ $bindir == /* && $bindir != / && $bindir != /usr/bin &&
           $bindir != /usr/local/bin && ! -L "$bindir" ]] || {
            echo 'Refusing non-private, relative or symlinked command directory.' >&2
            exit 1
        }
        [[ ! -e "$bindir" || -d "$bindir" ]] || {
            echo "Command directory is not a directory: $bindir" >&2
            exit 1
        }
        # Verify both destinations before making either link, avoiding a
        # partial update when an unrelated executable is already installed.
        for name in viper4linux v4l-cli; do
            if [[ $name == viper4linux ]]; then
                target="$root/viper4linux"
            else
                target="$root/build/viper-ctl"
            fi
            link="$bindir/$name"
            if [[ -L "$link" ]]; then
                [[ $(readlink -- "$link") == "$target" ]] || {
                    echo "Unrelated command symlink exists: $link" >&2
                    exit 1
                }
            elif [[ -e "$link" ]]; then
                echo "Unrelated command exists: $link" >&2
                exit 1
            fi
        done
        mkdir -p -- "$bindir"
        [[ ! -L "$bindir" ]] || {
            echo "Command directory changed to symlink: $bindir" >&2
            exit 1
        }
        [[ -L "$bindir/viper4linux" ]] || ln -s -- "$root/viper4linux" "$bindir/viper4linux"
        [[ -L "$bindir/v4l-cli" ]] || ln -s -- "$root/build/viper-ctl" "$bindir/v4l-cli"
        echo "PASS: private command aliases ready under $bindir (unrelated entries untouched)."
        ;;
    *)
        echo 'Usage: scripts/setup-source-checkout.sh --check|--prepare-routing|--link-commands' >&2
        exit 2
        ;;
esac
