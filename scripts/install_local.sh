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

destination_dll="$plugin_dir/RuptureGodMode.dll"
destination_sidecar="$plugin_dir/RuptureGodMode.json"
legacy_dll="$plugin_dir/RuptureGodMode-Client.dll"
legacy_sidecar="$plugin_dir/RuptureGodMode-Client.json"

path_exists() {
    [[ -e "$1" || -L "$1" ]]
}

for existing_path in \
    "$destination_dll" \
    "$destination_sidecar" \
    "$legacy_dll" \
    "$legacy_sidecar" \
    "$config_file"; do
    if path_exists "$existing_path"; then
        if [[ -L "$existing_path" || ! -f "$existing_path" ]]; then
            echo "Expected a regular non-symlink file but found another filesystem entry: $existing_path" >&2
            exit 1
        fi
    fi
done

transaction_directory=$(mktemp -d "$plugin_dir/.RuptureGodMode.install.XXXXXX")
rollback_directory="$transaction_directory/rollback"
staged_dll="$transaction_directory/RuptureGodMode.dll"
staged_sidecar="$transaction_directory/RuptureGodMode.json"
staged_config="$transaction_directory/modloader.ini"
mkdir "$rollback_directory"

had_dll=0
had_sidecar=0
had_config=0
had_legacy_dll=0
had_legacy_sidecar=0
path_exists "$destination_dll" && had_dll=1
path_exists "$destination_sidecar" && had_sidecar=1
path_exists "$config_file" && had_config=1
path_exists "$legacy_dll" && had_legacy_dll=1
path_exists "$legacy_sidecar" && had_legacy_sidecar=1

backed_up_dll=0
backed_up_sidecar=0
backed_up_config=0
backed_up_legacy_dll=0
backed_up_legacy_sidecar=0
installed_dll=0
installed_sidecar=0
installed_config=0
transaction_active=0

rollback_installation() {
    local rollback_failed=0

    restore_backup() {
        local backup_path=$1
        local destination_path=$2

        if path_exists "$backup_path"; then
            if path_exists "$destination_path"; then
                return 1
            fi
            mv -- "$backup_path" "$destination_path"
        elif ! path_exists "$destination_path"; then
            return 1
        fi
    }

    if ((installed_config)) && path_exists "$config_file"; then
        rm -f -- "$config_file" || rollback_failed=1
    fi
    if ((installed_sidecar)) && path_exists "$destination_sidecar"; then
        rm -f -- "$destination_sidecar" || rollback_failed=1
    fi
    if ((installed_dll)) && path_exists "$destination_dll"; then
        rm -f -- "$destination_dll" || rollback_failed=1
    fi

    if ((backed_up_config)); then
        if ! restore_backup "$rollback_directory/modloader.ini" "$config_file"; then
            rollback_failed=1
        fi
    fi
    if ((backed_up_sidecar)); then
        if ! restore_backup \
            "$rollback_directory/RuptureGodMode.json" "$destination_sidecar"; then
            rollback_failed=1
        fi
    fi
    if ((backed_up_dll)); then
        if ! restore_backup \
            "$rollback_directory/RuptureGodMode.dll" "$destination_dll"; then
            rollback_failed=1
        fi
    fi
    if ((backed_up_legacy_sidecar)); then
        if ! restore_backup \
            "$rollback_directory/RuptureGodMode-Client.json" "$legacy_sidecar"; then
            rollback_failed=1
        fi
    fi
    if ((backed_up_legacy_dll)); then
        if ! restore_backup \
            "$rollback_directory/RuptureGodMode-Client.dll" "$legacy_dll"; then
            rollback_failed=1
        fi
    fi

    ((rollback_failed == 0))
}

cleanup_on_exit() {
    local exit_status=$?
    trap - EXIT

    if ((transaction_active)); then
        if rollback_installation; then
            transaction_active=0
            rm -rf -- "$transaction_directory"
        else
            echo "Automatic rollback was incomplete. Recovery files were preserved in:" >&2
            echo "  $transaction_directory" >&2
        fi
    else
        rm -rf -- "$transaction_directory"
    fi

    exit "$exit_status"
}
trap cleanup_on_exit EXIT

install -m 0644 "$source_dll" "$staged_dll"
install -m 0644 "$source_sidecar" "$staged_sidecar"

if ((had_config)); then
    awk '
        function finish_auto_update_section() {
            if (in_auto_update && !section_has_enabled) print "Enabled=1"
        }
        /^\[AutoUpdate\][[:space:]]*$/ {
            finish_auto_update_section()
            found_auto_update=1
            in_auto_update=1
            section_has_enabled=0
            print
            next
        }
        /^\[/ {
            finish_auto_update_section()
            in_auto_update=0
            print
            next
        }
        in_auto_update && /^[[:space:]]*Enabled[[:space:]]*=/ {
            print "Enabled=1"
            section_has_enabled=1
            next
        }
        { print }
        END {
            finish_auto_update_section()
            if (!found_auto_update) {
                print ""
                print "[AutoUpdate]"
                print "Enabled=1"
            }
        }
    ' "$config_file" >"$staged_config" || {
        echo "Could not enable ModLoader auto-update in $config_file" >&2
        exit 1
    }
else
    printf '[AutoUpdate]\nEnabled=1\n' >"$staged_config"
fi

transaction_active=1
if ((had_dll)); then
    backed_up_dll=1
    mv -- "$destination_dll" "$rollback_directory/RuptureGodMode.dll"
fi
if ((had_sidecar)); then
    backed_up_sidecar=1
    mv -- "$destination_sidecar" "$rollback_directory/RuptureGodMode.json"
fi
if ((had_config)); then
    backed_up_config=1
    mv -- "$config_file" "$rollback_directory/modloader.ini"
fi
if ((had_legacy_dll)); then
    backed_up_legacy_dll=1
    mv -- "$legacy_dll" "$rollback_directory/RuptureGodMode-Client.dll"
fi
if ((had_legacy_sidecar)); then
    backed_up_legacy_sidecar=1
    mv -- "$legacy_sidecar" "$rollback_directory/RuptureGodMode-Client.json"
fi

installed_dll=1
mv -- "$staged_dll" "$destination_dll"
installed_sidecar=1
mv -- "$staged_sidecar" "$destination_sidecar"
installed_config=1
mv -- "$staged_config" "$config_file"

transaction_active=0
rm -rf -- "$transaction_directory"
trap - EXIT

echo "Installed RuptureGodMode.dll and auto-update sidecar in $plugin_dir"
