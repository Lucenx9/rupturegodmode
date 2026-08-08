#!/usr/bin/env bash
set -euo pipefail

: "${SDK_DIR:?set SDK_DIR to the StarRupture-Plugin-SDK checkout}"
: "${WIN_SDK_DIR:?set WIN_SDK_DIR to an xwin splat directory}"

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
game_sdk_root=${GAME_SDK_ROOT:-"$SDK_DIR/StarRupture SDK"}
game_sdk_client="$game_sdk_root/Client"
game_sdk_source="$game_sdk_client/SDK"
plugin_interface_header="$SDK_DIR/include/plugin_interface.h"
object_dir=${OBJECT_DIR:-"$project_root/build/plugin-objects"}
output_dll=${OUTPUT_DLL:-"$project_root/build/RuptureGodMode.dll"}
plugin_version=${PLUGIN_VERSION:-dev}
expected_plugin_interface=60

actual_plugin_interface=$(awk \
    '$2 == "PLUGIN_INTERFACE_VERSION" { print $3; exit }' \
    "$plugin_interface_header")
if [[ "$actual_plugin_interface" != "$expected_plugin_interface" ]]; then
    echo "Unsupported plugin SDK interface: expected $expected_plugin_interface, got ${actual_plugin_interface:-missing}" >&2
    exit 1
fi

mkdir -p "$object_dir" "$(dirname -- "$output_dll")"
rm -f "$object_dir"/*.obj "$output_dll"

common_flags=(
    /nologo /std:c++20 /EHsc /O2 /clang:-Wno-c++11-narrowing
    /clang:-Wno-invalid-offsetof /clang:-Wno-nonportable-include-path
    /clang:-Wno-pragma-pack /clang:-Wno-ignored-pragma-intrinsic
    /clang:-Wno-unknown-pragmas /clang:-Wno-missing-braces
    /clang:-Wno-unused-local-typedef /clang:-Wno-unused-private-field
    /DNDEBUG /DMODLOADER_CLIENT_BUILD /D_WINDOWS /D_USRDLL
    "/DMODLOADER_BUILD_TAG=\"$plugin_version\""
    "/I$SDK_DIR/include"
    "/I$game_sdk_client"
    "/I$game_sdk_source"
    "/I$project_root/plugin"
    "/I$WIN_SDK_DIR/crt/include"
    "/I$WIN_SDK_DIR/sdk/include/ucrt"
    "/I$WIN_SDK_DIR/sdk/include/shared"
    "/I$WIN_SDK_DIR/sdk/include/um"
)

compile_source() {
    local warnings=$1
    local source=$2
    local object
    object="$object_dir/$(basename "${source%.cpp}").obj"
    clang-cl "${common_flags[@]}" "$warnings" /c "$source" "/Fo:$object"
}

sdk_sources=(
    "$game_sdk_source/Basic.cpp"
    "$game_sdk_source/CoreUObject_functions.cpp"
    "$game_sdk_source/Engine_functions.cpp"
    "$game_sdk_source/Chimera_functions.cpp"
)
project_sources=(
    "$project_root/plugin/dllmain.cpp"
    "$project_root/plugin/god_mode_controller.cpp"
    "$project_root/plugin/plugin.cpp"
    "$project_root/plugin/star_rupture_effects.cpp"
)

for source in "${sdk_sources[@]}"; do
    compile_source /W0 "$source"
done
for source in "${project_sources[@]}"; do
    compile_source /W4 "$source"
done

lld-link \
    /nologo /dll /machine:x64 "/out:$output_dll" \
    "$object_dir"/*.obj \
    "/libpath:$WIN_SDK_DIR/crt/lib/x86_64" \
    "/libpath:$WIN_SDK_DIR/sdk/lib/ucrt/x86_64" \
    "/libpath:$WIN_SDK_DIR/sdk/lib/um/x86_64" \
    user32.lib

file "$output_dll"
sha256sum "$output_dll"
