#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temporary_directory=$(mktemp -d)
trap 'rm -rf -- "$temporary_directory"' EXIT

game_root="$temporary_directory/StarRupture"
binary_directory="$game_root/StarRupture/Binaries/Win64"
plugin_directory="$binary_directory/ModLoader/Plugins"
config_file="$binary_directory/ModLoader/modloader.ini"
source_dll="$temporary_directory/RuptureGodMode.dll"

mkdir -p "$plugin_directory"
printf '' >"$binary_directory/StarRuptureGameSteam-Win64-Shipping.exe"
printf '' >"$binary_directory/dwmapi.dll"
printf 'MZnew-plugin' >"$source_dll"
printf 'old-plugin' >"$plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' >"$plugin_directory/RuptureGodMode.json"
printf 'legacy-plugin' >"$plugin_directory/RuptureGodMode-Client.dll"
printf 'legacy-sidecar' >"$plugin_directory/RuptureGodMode-Client.json"
printf '[General]\nEnabled=0\n' >"$config_file"

RGM_DLL="$source_dll" "$project_root/scripts/install_local.sh" "$game_root"

cmp "$source_dll" "$plugin_directory/RuptureGodMode.dll"
cmp "$project_root/release/RuptureGodMode.json" "$plugin_directory/RuptureGodMode.json"
test ! -e "$plugin_directory/RuptureGodMode-Client.dll"
test ! -e "$plugin_directory/RuptureGodMode-Client.json"
awk '
    { sub(/\r$/, "") }
    /^\[AutoUpdate\]$/ { in_auto_update=1; found=1; next }
    /^\[/ && in_auto_update { in_auto_update=0 }
    in_auto_update && /^[[:space:]]*Enabled[[:space:]]*=[[:space:]]*1[[:space:]]*$/ {
        enabled=1
    }
    END { if (!found || !enabled) exit 1 }
' "$config_file"

symlink_game_root="$temporary_directory/SymlinkGame"
symlink_binary_directory="$symlink_game_root/StarRupture/Binaries/Win64"
symlink_plugin_directory="$symlink_binary_directory/ModLoader/Plugins"
symlink_config_file="$symlink_binary_directory/ModLoader/modloader.ini"
managed_config="$temporary_directory/managed-modloader.ini"
expected_managed_config="$temporary_directory/expected-managed-modloader.ini"

mkdir -p "$symlink_plugin_directory"
printf '' >"$symlink_binary_directory/StarRuptureGameSteam-Win64-Shipping.exe"
printf '' >"$symlink_binary_directory/dwmapi.dll"
printf 'old-plugin' >"$symlink_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' >"$symlink_plugin_directory/RuptureGodMode.json"
printf '[AutoUpdate]\nEnabled=0\n' >"$managed_config"
cp "$managed_config" "$expected_managed_config"
ln -s "$managed_config" "$symlink_config_file"

if RGM_DLL="$source_dll" \
    "$project_root/scripts/install_local.sh" "$symlink_game_root"; then
    printf 'Installer unexpectedly accepted a symlinked destination\n' >&2
    exit 1
fi

test -L "$symlink_config_file"
cmp "$expected_managed_config" "$managed_config"
printf 'old-plugin' | cmp - "$symlink_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' | cmp - "$symlink_plugin_directory/RuptureGodMode.json"
if compgen -G "$symlink_plugin_directory/.RuptureGodMode.install.*" >/dev/null; then
    printf 'Installer left transaction files after rejecting a symlink\n' >&2
    exit 1
fi

rollback_game_root="$temporary_directory/RollbackGame"
rollback_binary_directory="$rollback_game_root/StarRupture/Binaries/Win64"
rollback_plugin_directory="$rollback_binary_directory/ModLoader/Plugins"
rollback_config_file="$rollback_binary_directory/ModLoader/modloader.ini"
expected_config="$temporary_directory/expected-modloader.ini"

mkdir -p "$rollback_plugin_directory/RuptureGodMode-Client.dll"
printf '' >"$rollback_binary_directory/StarRuptureGameSteam-Win64-Shipping.exe"
printf '' >"$rollback_binary_directory/dwmapi.dll"
printf 'old-plugin' >"$rollback_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' >"$rollback_plugin_directory/RuptureGodMode.json"
printf '[AutoUpdate]\nEnabled=0\n' >"$rollback_config_file"
cp "$rollback_config_file" "$expected_config"

if RGM_DLL="$source_dll" \
    "$project_root/scripts/install_local.sh" "$rollback_game_root"; then
    printf 'Installer unexpectedly succeeded during rollback scenario\n' >&2
    exit 1
fi

printf 'old-plugin' | cmp - "$rollback_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' | cmp - "$rollback_plugin_directory/RuptureGodMode.json"
cmp "$expected_config" "$rollback_config_file"
test -d "$rollback_plugin_directory/RuptureGodMode-Client.dll"

fault_game_root="$temporary_directory/FaultGame"
fault_binary_directory="$fault_game_root/StarRupture/Binaries/Win64"
fault_plugin_directory="$fault_binary_directory/ModLoader/Plugins"
fault_config_file="$fault_binary_directory/ModLoader/modloader.ini"
fault_expected_config="$temporary_directory/fault-expected-modloader.ini"
real_mv=$(command -v mv)

mkdir -p "$fault_plugin_directory"
printf '' >"$fault_binary_directory/StarRuptureGameSteam-Win64-Shipping.exe"
printf '' >"$fault_binary_directory/dwmapi.dll"
printf 'old-plugin' >"$fault_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' >"$fault_plugin_directory/RuptureGodMode.json"
printf '[AutoUpdate]\nEnabled=0\n' >"$fault_config_file"
cp "$fault_config_file" "$fault_expected_config"

if PATH="$project_root/tests/fixtures/fail_staged_sidecar_mv:$PATH" \
    RGM_TEST_REAL_MV="$real_mv" \
    RGM_DLL="$source_dll" \
    "$project_root/scripts/install_local.sh" "$fault_game_root"; then
    printf 'Installer unexpectedly succeeded after an injected commit failure\n' >&2
    exit 1
fi

printf 'old-plugin' | cmp - "$fault_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' | cmp - "$fault_plugin_directory/RuptureGodMode.json"
cmp "$fault_expected_config" "$fault_config_file"
if compgen -G "$fault_plugin_directory/.RuptureGodMode.install.*" >/dev/null; then
    printf 'Installer left transaction files after a successful rollback\n' >&2
    exit 1
fi

interrupt_game_root="$temporary_directory/InterruptGame"
interrupt_binary_directory="$interrupt_game_root/StarRupture/Binaries/Win64"
interrupt_plugin_directory="$interrupt_binary_directory/ModLoader/Plugins"
interrupt_config_file="$interrupt_binary_directory/ModLoader/modloader.ini"
interrupt_expected_config="$temporary_directory/interrupt-expected-modloader.ini"

mkdir -p "$interrupt_plugin_directory"
printf '' >"$interrupt_binary_directory/StarRuptureGameSteam-Win64-Shipping.exe"
printf '' >"$interrupt_binary_directory/dwmapi.dll"
printf 'old-plugin' >"$interrupt_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' >"$interrupt_plugin_directory/RuptureGodMode.json"
printf '[AutoUpdate]\nEnabled=0\n' >"$interrupt_config_file"
cp "$interrupt_config_file" "$interrupt_expected_config"

if PATH="$project_root/tests/fixtures/interrupt_after_mv:$PATH" \
    RGM_TEST_REAL_MV="$real_mv" \
    RGM_TEST_INTERRUPT_MODE=backup-dll \
    RGM_DLL="$source_dll" \
    "$project_root/scripts/install_local.sh" "$interrupt_game_root"; then
    printf 'Installer unexpectedly succeeded after an injected interrupt\n' >&2
    exit 1
fi

printf 'old-plugin' | cmp - "$interrupt_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' | cmp - "$interrupt_plugin_directory/RuptureGodMode.json"
cmp "$interrupt_expected_config" "$interrupt_config_file"
if compgen -G "$interrupt_plugin_directory/.RuptureGodMode.install.*" >/dev/null; then
    printf 'Installer left transaction files after interrupt rollback\n' >&2
    exit 1
fi

if PATH="$project_root/tests/fixtures/interrupt_after_mv:$PATH" \
    RGM_TEST_REAL_MV="$real_mv" \
    RGM_TEST_INTERRUPT_MODE=install-dll \
    RGM_DLL="$source_dll" \
    "$project_root/scripts/install_local.sh" "$interrupt_game_root"; then
    printf 'Installer unexpectedly succeeded after an injected install interrupt\n' >&2
    exit 1
fi

printf 'old-plugin' | cmp - "$interrupt_plugin_directory/RuptureGodMode.dll"
printf 'old-sidecar' | cmp - "$interrupt_plugin_directory/RuptureGodMode.json"
cmp "$interrupt_expected_config" "$interrupt_config_file"
if compgen -G "$interrupt_plugin_directory/.RuptureGodMode.install.*" >/dev/null; then
    printf 'Installer left transaction files after install interrupt rollback\n' >&2
    exit 1
fi

printf 'Linux installer integration test passed\n'
