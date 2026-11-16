#!/bin/bash
# Compiles and runs the preset checks: the factory presets, without a plug-in host.
#
#   Tests/run-preset-checks.sh
#
# Needs only g++ (no JUCE: Source/Presets.cpp is plain C++, like AuraDSP.h).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${DAAURA_PRESET_CHECKS_OUT:-/tmp/daaura-preset-checks}"
mkdir -p "$OUT"

g++ -std=c++17 -O1 -Wall -Wextra -Werror \
    "$HERE/PresetChecks.cpp" "$HERE/../Source/Presets.cpp" \
    -o "$OUT/preset-checks"

"$OUT/preset-checks"
