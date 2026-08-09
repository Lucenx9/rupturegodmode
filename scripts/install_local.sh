#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
default_game_root="/mnt/storage/SteamLibrary/steamapps/common/StarRupture"
game_root=${RGM_GAME_DIR:-${1:-$default_game_root}}
source_dll=${RGM_DLL:-"$project_root/build/RuptureGodMode.dll"}
source_sidecar="$project_root/release/RuptureGodMode.json"
binary_dir="$game_root/StarRupture/Binaries/Win64"
plugin_dir="$binary_dir/ModLoader/Plugins"
config_file="$binary_dir/ModLoader/modloader.ini"

if [[ ! -f "$binary_dir/StarRuptureGameSteam-Win64-Shipping.exe" ]]; then
    echo "StarRupture was not found under: $game_root" >&2
    exit 1
fi
if [[ ! -f "$binary_dir/dwmapi.dll" || ! -d "$plugin_dir" ]]; then
    echo "StarRupture ModLoader is not installed under: $binary_dir" >&2
    exit 1
fi
if [[ ! -f "$source_dll" ]]; then
    echo "Plugin DLL was not found: $source_dll" >&2
    echo "Run scripts/build_cross.sh first." >&2
    exit 1
fi
if [[ ! -f "$source_sidecar" ]]; then
    echo "Plugin auto-update sidecar was not found: $source_sidecar" >&2
    exit 1
fi
if [[ "$(od -An -N2 -tc "$source_dll" | tr -d ' ')" != "MZ" ]]; then
    echo "Build output is not a Windows DLL: $source_dll" >&2
    exit 1
fi

install -m 0644 "$source_dll" "$plugin_dir/RuptureGodMode.dll"
install -m 0644 "$source_sidecar" "$plugin_dir/RuptureGodMode.json"
rm -f -- "$plugin_dir/RuptureGodMode-Client.dll" "$plugin_dir/RuptureGodMode-Client.json"

if [[ -f "$config_file" ]]; then
    config_update="$config_file.rgm-update.$$"
    trap 'rm -f -- "$config_update"' EXIT
    awk '
        /^\[AutoUpdate\][[:space:]]*$/ { in_auto_update=1; print; next }
        /^\[/ { in_auto_update=0 }
        in_auto_update && /^[[:space:]]*Enabled[[:space:]]*=/ {
            print "Enabled=1"; changed=1; next
        }
        { print }
        END { if (!changed) exit 2 }
    ' "$config_file" >"$config_update" || {
        echo "Could not enable ModLoader auto-update in $config_file" >&2
        exit 1
    }
    mv -f -- "$config_update" "$config_file"
    trap - EXIT
fi

echo "Installed RuptureGodMode.dll and auto-update sidecar in $plugin_dir"
