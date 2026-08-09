#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
test_binary="${TMPDIR:-/tmp}/rupturegodmode-controller-tests"
cxx_compiler=${CXX_COMPILER:-clang++}

"$cxx_compiler" \
    -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -I"$project_root/plugin" \
    "$project_root/tests/test_god_mode_controller.cpp" \
    "$project_root/plugin/god_mode_controller.cpp" \
    "$project_root/plugin/immortality_state.cpp" \
    -o "$test_binary"
"$test_binary"
python3 -m unittest "$project_root/tests/test_release_packaging.py" -v
