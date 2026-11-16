#pragma once

#include <juce_graphics/juce_graphics.h>

namespace daaura
{

/** A copy of a picture in another size.

    The copy is `width` x `height` pixels, and each of its pixels covers `sourcePixelsPerPixel`
    of the source's, starting at the top left corner. It is made with a Lanczos filter, which
    averages when the copy is smaller and keeps fine lines and small lettering clean either
    way. (The systems' own scaling differs from one to the next, and JUCE's is rough.)

    `source` has to be an RGB picture held in plain memory (juce::SoftwareImageType); the
    copy is one too. */
juce::Image resampled (const juce::Image& source, int width, int height, double sourcePixelsPerPixel);

/** How a drawing of the window compares with a picture of it, pixel by pixel. */
struct Likeness
{
    int inside = 0;             // pixels that lie wholly inside the window's outline
    int differing = 0;          // ...how many of those are not the picture's
    int worst = 0;              // ...and how far out the worst of them is (0 to 255)
    int outside = 0;            // pixels that lie wholly outside the outline
    int spilt = 0;              // ...how many of those have anything at all drawn on them
    int onTheOutline = 0;       // pixels the outline runs through (not judged)

    bool isExact() const noexcept       { return inside > 0 && differing == 0 && spilt == 0; }
};

/** Compares `drawn` with the part of `picture` whose top left corner is at `at`.

    `drawn` is a picture with transparency, drawn on nothing; `outline` is the window's
    outline in its pixels. Of `drawn`, the first `rows` rows are compared. */
Likeness compare (const juce::Image& drawn, const juce::Image& picture, juce::Point<int> at,
                  const juce::Path& outline, int rows);

} // namespace daaura
