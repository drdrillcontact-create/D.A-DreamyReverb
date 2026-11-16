#pragma once

#include <juce_graphics/juce_graphics.h>

namespace daaura
{

/** The pictures and the typefaces that are built into the installer (Installer/Assets).

    The pictures all come from the design, Installer/Design/Installer.png: the first page is
    that picture itself; the others are the same picture with the first page's words, or
    its buttons as well, painted out (see Installer/Design/make-assets.py). */
class Assets
{
public:
    Assets();

    enum class Picture
    {
        design,         // the window exactly as designed: the first page
        noText,         // without the headline and the two lines under it
        noButtons,      // without those and without the buttons
        finish          // without those, without Cancel, and with no label on the red button
    };

    /** One of the pictures above. Each shows the whole window, with `pictureScale` of its
        pixels to one of the view's units. */
    const juce::Image& getPicture (Picture which) const noexcept    { return pictures[(size_t) which]; }

    static constexpr float pictureScale = 1.8f;

    /** The outline of the window, in the picture's pixels: the frame the design draws around
        it, with its four cut corners. What the pictures show outside it is the dark sheet the
        design was presented on. That is no part of the window, and the installer shows none
        of it: outside this outline the window is not there. */
    static juce::Path outline();

    /** The rectangle that outline fits into, in the picture's pixels. */
    static juce::Rectangle<float> frame() noexcept;

    /** What could not be loaded: empty when everything is there. */
    juce::StringArray getMissing() const;

    // Red Hat Display, for the words that the pages after the first one add: the weights that
    // come closest to the design's own lettering. `spread` is extra room between the letters,
    // in font heights.
    juce::Font text          (float height, float spread = 0.0f) const   { return make (regular,   height, spread); }
    juce::Font textMedium    (float height, float spread = 0.0f) const   { return make (medium,    height, spread); }
    juce::Font textSemiBold  (float height, float spread = 0.0f) const   { return make (semiBold,  height, spread); }
    juce::Font textBold      (float height, float spread = 0.0f) const   { return make (bold,      height, spread); }
    juce::Font textExtraBold (float height, float spread = 0.0f) const   { return make (extraBold, height, spread); }

private:
    static juce::Font make (const juce::Typeface::Ptr&, float height, float spread);

    std::array<juce::Image, 4> pictures;
    juce::Typeface::Ptr regular, medium, semiBold, bold, extraBold;
};

} // namespace daaura
