// Checks the factory presets, without a plug-in host: Tests/run-preset-checks.sh compiles this
// with Source/Presets.cpp and runs it. It holds Presets.cpp to the rules written at its top,
// and prints what the browser will show, so that the list can be read through in one go.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "../Source/Presets.h"

namespace
{
    int failures = 0;

    void check (bool ok, const std::string& what)
    {
        std::printf ("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.c_str());

        if (! ok)
            ++failures;
    }
}

int main()
{
    using namespace aura;

    check (numPresets == 50, "there are fifty presets");
    check (numSpaces == 5, "in five spaces");

    // every space is inside the list and the spaces tile it, in order
    int covered = 0;
    bool tiles = true;

    for (int s = 0; s < numSpaces; ++s)
    {
        const auto& space = spaces[s];

        if (space.first != covered || space.count <= 0)
            tiles = false;

        covered += space.count;

        for (int i = space.first; i < space.first + space.count; ++i)
            if (std::strcmp (presets[i].space, space.name) != 0)
                tiles = false;
    }

    check (tiles, "the spaces follow each other without a gap and each preset names its space");
    check (covered == numPresets, "and together they hold every preset");

    // names are all different
    std::set<std::string> names;
    bool unique = true;

    for (int i = 0; i < numPresets; ++i)
    {
        if (! names.insert (presets[i].name).second)
            unique = false;

        if (presets[i].name == nullptr || presets[i].name[0] == 0)
            unique = false;
    }

    check (unique, "every preset has its own name");

    // every value inside its parameter's range
    bool ranges = true;
    int frozen = 0;

    for (int i = 0; i < numPresets; ++i)
    {
        const auto& v = presets[i].values;
        const auto in = [i] (float value, float lo, float hi)
        {
            if (! (value >= lo && value <= hi))
            {
                std::printf ("       %s: %f is outside %f .. %f\n", presets[i].name, (double) value, (double) lo, (double) hi);
                return false;
            }

            return true;
        };

        ranges = in (v.mix, 0.0f, 1.0f)       && ranges;
        ranges = in (v.size, 0.0f, 1.0f)      && ranges;
        ranges = in (v.decay, 0.3f, 30.0f)    && ranges;
        ranges = in (v.preDelay, 0.0f, 500.0f)&& ranges;
        ranges = in (v.highCut, 1000.0f, 20000.0f) && ranges;
        ranges = in (v.lowCut, 20.0f, 1000.0f)&& ranges;
        ranges = in (v.modDepth, 0.0f, 1.0f)  && ranges;
        ranges = in (v.modRate, 0.05f, 3.0f)  && ranges;
        ranges = in (v.shimmer, 0.0f, 1.0f)   && ranges;
        ranges = in (v.width, 0.0f, 1.5f)     && ranges;

        if (v.freeze)
            ++frozen;
    }

    check (ranges, "every value is inside the range of the parameter it sets");
    check (frozen == 2, "two presets are frozen, the rest are live");

    // no two presets alike
    bool allDifferent = true;

    for (int i = 0; i < numPresets && allDifferent; ++i)
        for (int j = i + 1; j < numPresets; ++j)
        {
            const auto& a = presets[i].values;
            const auto& b = presets[j].values;

            if (std::fabs (a.mix - b.mix) < 0.001f && std::fabs (a.size - b.size) < 0.001f
                && std::fabs (a.decay - b.decay) < 0.01f && std::fabs (a.preDelay - b.preDelay) < 0.1f
                && std::fabs (a.highCut - b.highCut) < 1.0f && std::fabs (a.lowCut - b.lowCut) < 1.0f
                && std::fabs (a.modDepth - b.modDepth) < 0.001f && std::fabs (a.modRate - b.modRate) < 0.001f
                && std::fabs (a.shimmer - b.shimmer) < 0.001f && std::fabs (a.width - b.width) < 0.001f
                && a.freeze == b.freeze)
            {
                std::printf ("       %s and %s are the same\n", presets[i].name, presets[j].name);
                allDifferent = false;
            }
        }

    check (allDifferent, "no two presets are the same");

    // within a space, from the short and tight ones to the long and large ones
    bool ordered = true;

    for (int s = 0; s < numSpaces; ++s)
    {
        const auto& space = spaces[s];

        for (int i = space.first + 1; i < space.first + space.count; ++i)
            if (presets[i].values.decay < presets[i - 1].values.decay - 0.001f)
                ordered = false;
    }

    check (ordered, "inside a space the decay grows from the first preset to the last");

    // the browsing: the arrows stay inside the space and wrap around it
    bool browsing = true;

    for (int s = 0; s < numSpaces; ++s)
    {
        const auto& space = spaces[s];
        const int last = space.first + space.count - 1;

        if (nextPresetInSpace (last, 1) != space.first)          browsing = false;
        if (nextPresetInSpace (space.first, -1) != last)         browsing = false;
        if (nextPresetInSpace (space.first, 1) != space.first + 1) browsing = false;
        if (nextPresetInSpace (last, -1) != last - 1)            browsing = false;
    }

    check (browsing, "the arrows step through a space and wrap around inside it");

    bool finding = true;

    for (int i = 0; i < numPresets; ++i)
        if (findPreset (presets[i].name) != i)
            finding = false;

    check (finding, "a preset is found again by its name");
    check (findPreset ("No Such Preset") == -1, "and an unknown name is not found");

    // what the browser shows
    std::printf ("\nThe presets, as the browser lists them:\n");

    for (int s = 0; s < numSpaces; ++s)
    {
        const auto& space = spaces[s];
        std::printf ("\n  %s\n", space.name);

        for (int i = space.first; i < space.first + space.count; ++i)
            std::printf ("    %2d  %-16s  decay %5.1f s   size %3.0f %%   shimmer %3.0f %%   width %3.0f %%\n",
                         i + 1, presets[i].name,
                         (double) presets[i].values.decay,
                         (double) (presets[i].values.size * 100.0f),
                         (double) (presets[i].values.shimmer * 100.0f),
                         (double) (presets[i].values.width * 100.0f));
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
