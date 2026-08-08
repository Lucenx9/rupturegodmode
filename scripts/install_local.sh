#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
default_game_root="/mnt/storage/SteamLibrary/steamapps/common/StarRupture"
game_root=${RGM_GAME_DIR:-${1:-$default_game_root}}
source_dll=${RGM_DLL:-"$project_root/build/RuptureGodMode.dll"}
binary_dir="$game_root/StarRupture/Binaries/Win64"
plugin_dir="$binary_dir/ModLoader/Plugins"

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
if [[ "$(od -An -N2 -tc "$source_dll" | tr -d ' ')" != "MZ" ]]; then
    echo "Build output is not a Windows DLL: $source_dll" >&2
    exit 1
fi

install -m 0644 "$source_dll" "$plugin_dir/RuptureGodMode.dll"
echo "Installed RuptureGodMode.dll in $plugin_dir"
