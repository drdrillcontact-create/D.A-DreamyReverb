#!/bin/bash
# Draws the window, headless, to pictures (at rest, with a signal, a few presets, larger).
#
#   Tests/run-editor-pictures.sh [folder]     (default: /tmp/daaura-pictures)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/.."
source "$HERE/local-juce.sh"
local_juce_setup > /dev/null

OUT="$LOCAL_ROOT/editor-pictures-build"
mkdir -p "$OUT" "$LOCAL_ROOT/hdr"
printf '#pragma once\n#include <juce_audio_utils/juce_audio_utils.h>\n#include <juce_gui_extra/juce_gui_extra.h>\n#ifndef JucePlugin_Name\n #define JucePlugin_Name "D.A AURA"\n#endif\n' > "$LOCAL_ROOT/hdr/JuceHeader.h"

for f in "$ROOT/Source/PluginProcessor.cpp" "$ROOT/Source/PluginEditor.cpp" "$ROOT/Source/Presets.cpp" \
         "$HERE/EditorPictures.cpp"; do
    g++ $JUCE_CXXFLAGS -I"$LOCAL_ROOT/hdr" -I"$ROOT/Source" -c "$f" -o "$OUT/$(basename "$f").o" &
done
wait

g++ "$OUT"/*.o "$JUCE_OBJ"/*.o $JUCE_LIBS -rdynamic -o "$OUT/editor-pictures"
"$OUT/editor-pictures" "${1:-/tmp/daaura-pictures}"
