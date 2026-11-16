#!/bin/bash
# Syntax-checks the plug-in's sources against JUCE without building the whole plug-in: needs
# the JUCE tree that Tests/local-juce.sh fetches (it is fetched if it is not there yet).
#
#   Tests/syntax-check.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/local-juce.sh"

mkdir -p "$LOCAL_ROOT/hdr"
printf '#pragma once\n#include <juce_audio_utils/juce_audio_utils.h>\n#include <juce_gui_extra/juce_gui_extra.h>\n#ifndef JucePlugin_Name\n #define JucePlugin_Name "D.A AURA"\n#endif\n' > "$LOCAL_ROOT/hdr/JuceHeader.h"

local_juce_setup > /dev/null

status=0
for f in "$REPO_ROOT"/Source/AuraDSP.h "$REPO_ROOT"/Source/Presets.cpp "$REPO_ROOT"/Source/Presets.h \
         "$REPO_ROOT"/Source/PluginProcessor.cpp "$REPO_ROOT"/Source/PluginEditor.cpp; do
    echo "-- $(basename "$f")"
    g++ $JUCE_CXXFLAGS -Wall -Wextra -Werror -I"$LOCAL_ROOT/hdr" -I"$REPO_ROOT/Source" \
        -fsyntax-only "$f" || status=1
done

[ $status -eq 0 ] && echo "ALL PASSED"
exit $status
