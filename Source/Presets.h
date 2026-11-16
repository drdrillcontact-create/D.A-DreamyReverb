// The plug-in's factory presets: fifty settings, in five spaces of eight to twelve each.
//
// Plain C++ and JUCE-free, like AuraDSP.h, so that the preset list can be checked without a
// plug-in host (Tests/run-preset-checks.sh). The values are in the parameters' own units - the
// units the plug-in's knobs show - and Presets.cpp holds them in one table (PRESETS); the tests
// hold that table to the rules stated at the top of Presets.cpp.
#pragma once

#include <cstddef>
#include <cstring>

namespace aura
{

/** What a preset says: every parameter of the plug-in, in its own units. */
struct PresetValues
{
    float mix        = 0.40f;    // 0..1        dry/wet
    float size       = 0.65f;    // 0..1        room size
    float decay      = 7.0f;     // 0.3..30 s
    float preDelay   = 25.0f;    // 0..500 ms
    float highCut    = 6500.0f;  // 1000..20000 Hz
    float lowCut     = 120.0f;   // 20..1000 Hz
    float modDepth   = 0.45f;    // 0..1
    float modRate    = 0.45f;    // 0.05..3 Hz
    float shimmer    = 0.30f;    // 0..1
    float width      = 1.00f;    // 0..1.5
    bool  freeze     = false;
};

struct Preset
{
    const char* name;            // "Cathedral", "Plate", ...
    const char* space;           // the space it is for, one of the five in Spaces below
    PresetValues values;
};

/** The spaces the presets are grouped by, in the order the browser shows them. */
struct Space
{
    const char* name;            // the group's heading in the browser
    int first, count;            // the presets of the group: PRESETS[first .. first + count)
};

// (stated here so that both Presets.cpp and the tests agree on the count)
constexpr int numPresets    = 50;
constexpr int numSpaces     = 5;
constexpr int presetsPerPage = 12;   // what the browser lists at once

extern const Preset presets[numPresets];
extern const Space  spaces[numSpaces];

/** The preset's index in PRESETS, or -1. Matches the name as it is written in the list. */
inline int findPreset (const char* name)
{
    if (name == nullptr)
        return -1;

    for (int i = 0; i < numPresets; ++i)
        if (std::strcmp (presets[i].name, name) == 0)
            return i;

    return -1;
}

/** The next preset in the same space, wrapping around inside it. */
inline int nextPresetInSpace (int index, int direction)
{
    if (index < 0 || index >= numPresets)
        return 0;

    for (int s = 0; s < numSpaces; ++s)
    {
        const auto& space = spaces[s];

        if (index >= space.first && index < space.first + space.count)
        {
            const int inside = index - space.first;
            const int moved  = ((inside + direction) % space.count + space.count) % space.count;
            return space.first + moved;
        }
    }

    return index;
}

} // namespace aura
