#pragma once

#include <juce_graphics/juce_graphics.h>

namespace daaura
{

/** The D.A Production mark: an "A" built from two legs around a small triangle.
    Drawn to fill `area` (which should be about 1.3 times as wide as it is high). */
inline void drawBrandMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto at = [area] (float u, float v)
    {
        return juce::Point<float> (area.getX() + u * area.getWidth(), area.getY() + v * area.getHeight());
    };

    const auto add = [] (juce::Path& p, std::initializer_list<juce::Point<float>> corners)
    {
        bool first = true;

        for (const auto corner : corners)
        {
            if (first) p.startNewSubPath (corner); else p.lineTo (corner);
            first = false;
        }

        p.closeSubPath();
    };

    // Filled where an odd number of these shapes overlap: the outline, minus the counter and
    // the gap between the feet, plus the small triangle that sits inside the counter.
    juce::Path mark;
    mark.setUsingNonZeroWinding (false);
    add (mark, { at (0.000f, 1.000f), at (0.451f, 0.000f), at (0.549f, 0.000f), at (1.000f, 1.000f) });
    add (mark, { at (0.500f, 0.273f), at (0.765f, 0.839f), at (0.235f, 0.839f) });
    add (mark, { at (0.382f, 0.839f), at (0.618f, 0.839f), at (0.686f, 1.000f), at (0.304f, 1.000f) });
    add (mark, { at (0.500f, 0.545f), at (0.612f, 0.769f), at (0.392f, 0.774f) });

    g.setColour (colour);
    g.fillPath (mark);
}

} // namespace daaura
