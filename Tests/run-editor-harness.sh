#!/bin/bash
# Builds Tests/EditorHarness.cpp with the plug-in's sources and runs it: the presets, the
# browser's arrows, the window's labels and the aurora's response to a signal - headless.
#
#   Tests/run-editor-harness.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/.."
source "$HERE/local-juce.sh"
local_juce_setup > /dev/null

OUT="${DAAURA_EDITOR_HARNESS_OUT:-$LOCAL_ROOT/editor-harness}"
mkdir -p "$OUT" "$LOCAL_ROOT/hdr"

printf '#pragma once\n#include <juce_audio_utils/juce_audio_utils.h>\n#include <juce_gui_extra/juce_gui_extra.h>\n#ifndef JucePlugin_Name\n #define JucePlugin_Name "D.A AURA"\n#endif\n' > "$LOCAL_ROOT/hdr/JuceHeader.h"

objs=""
for f in "$ROOT/Source/PluginProcessor.cpp" "$ROOT/Source/PluginEditor.cpp" "$ROOT/Source/Presets.cpp" \
         "$HERE/EditorHarness.cpp"; do
    o="$OUT/$(basename "$f").o"
    g++ $JUCE_CXXFLAGS -I"$LOCAL_ROOT/hdr" -I"$ROOT/Source" -c "$f" -o "$o" &
    objs="$objs $o"
done
wait

g++ $objs "$JUCE_OBJ"/*.o $JUCE_LIBS -rdynamic -o "$OUT/editor-harness"

# (JUCE's leak detector reports on exit: the harness leaves without a full shutdown)
"$OUT/editor-harness"
