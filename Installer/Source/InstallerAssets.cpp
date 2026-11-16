#include "InstallerAssets.h"
#include <InstallerData.h>      // made from Installer/Assets by juce_add_binary_data

namespace daaura
{

namespace
{
// Where the patches lie on the picture, in its pixels (as Installer/Design/make-assets.py
// states them: TEXT and BUTTONS)
constexpr juce::Point<int> textPatchAt    { 846, 224 };
constexpr juce::Point<int> buttonsPatchAt { 834, 524 };

// The frame around the window, measured on the picture: its lines begin 14 pixels in from the
// left and from the top, and end 13 and 16 pixels short of the right and of the bottom; each
// corner is cut off at 45 degrees, 17 or 18 pixels along either side.
// (Tests/InstallerHarness.cpp holds these numbers against the design.)
constexpr float frameLeft = 14.0f, frameTop = 14.0f, frameRight = 1751.0f, frameBottom = 679.0f;

/** One of the built-in pictures, held in plain memory with three bytes to the pixel.

    (Whatever the system's own kind of picture is: macOS turns a picture without transparency
    into one with, Windows keeps pictures on the graphics card. The installer's pictures are
    measured out pixel by pixel, so they are kept in the one form that is the same everywhere.) */
juce::Image load (const char* data, int size)
{
    const auto decoded = juce::ImageFileFormat::loadFrom (data, (size_t) size);

    if (! decoded.isValid())
        return {};

    juce::Image plain (juce::Image::RGB, decoded.getWidth(), decoded.getHeight(), false, juce::SoftwareImageType());

    const juce::Image::BitmapData from (decoded, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData to (plain, juce::Image::BitmapData::writeOnly);

    if (from.data == nullptr || to.data == nullptr || to.pixelStride != 3)
        return {};

    for (int y = 0; y < plain.getHeight(); ++y)
    {
        auto* const line = reinterpret_cast<juce::PixelRGB*> (to.getLinePointer (y));

        for (int x = 0; x < plain.getWidth(); ++x)
        {
            const auto colour = from.getPixelColour (x, y);
            line[x].setARGB (255, colour.getRed(), colour.getGreen(), colour.getBlue());
        }
    }

    return plain;
}

/** A copy of `picture` with `patch` laid over it. */
juce::Image patched (const juce::Image& picture, const juce::Image& patch, juce::Point<int> at)
{
    if (! (picture.isValid() && patch.isValid()))
        return {};

    auto copy = picture.createCopy();
    juce::Graphics g (copy);
    g.setOpacity (1.0f);
    g.drawImageAt (patch, at.x, at.y);
    return copy;
}
}

Assets::Assets()
{
    const auto design = load (InstallerData::picture_png, InstallerData::picture_pngSize);
    const auto noText = patched (design, load (InstallerData::patchText_png, InstallerData::patchText_pngSize), textPatchAt);

    pictures[(size_t) Picture::design]    = design;
    pictures[(size_t) Picture::noText]    = noText;
    pictures[(size_t) Picture::noButtons] = patched (noText, load (InstallerData::patchNoButtons_png, InstallerData::patchNoButtons_pngSize), buttonsPatchAt);
    pictures[(size_t) Picture::finish]    = patched (noText, load (InstallerData::patchFinish_png, InstallerData::patchFinish_pngSize), buttonsPatchAt);

    const auto typeface = [] (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    };

    regular   = typeface (InstallerData::RedHatDisplayRegular_ttf,   InstallerData::RedHatDisplayRegular_ttfSize);
    medium    = typeface (InstallerData::RedHatDisplayMedium_ttf,    InstallerData::RedHatDisplayMedium_ttfSize);
    semiBold  = typeface (InstallerData::RedHatDisplaySemiBold_ttf,  InstallerData::RedHatDisplaySemiBold_ttfSize);
    bold      = typeface (InstallerData::RedHatDisplayBold_ttf,      InstallerData::RedHatDisplayBold_ttfSize);
    extraBold = typeface (InstallerData::RedHatDisplayExtraBold_ttf, InstallerData::RedHatDisplayExtraBold_ttfSize);
}

juce::Path Assets::outline()
{
    juce::Path path;
    path.startNewSubPath (32.0f, frameTop);
    path.lineTo (1733.0f, frameTop);
    path.lineTo (frameRight, 32.0f);
    path.lineTo (frameRight, 661.0f);
    path.lineTo (1733.0f, frameBottom);
    path.lineTo (31.0f, frameBottom);
    path.lineTo (frameLeft, 662.0f);
    path.lineTo (frameLeft, 32.0f);
    path.closeSubPath();
    return path;
}

juce::Rectangle<float> Assets::frame() noexcept
{
    return { frameLeft, frameTop, frameRight - frameLeft, frameBottom - frameTop };
}

juce::StringArray Assets::getMissing() const
{
    juce::StringArray missing;

    const auto there = [this] (Picture which)
    {
        const auto& picture = getPicture (which);
        return picture.isValid() && picture.getFormat() == juce::Image::RGB;
    };

    if (! there (Picture::design))      missing.add ("the picture");
    if (! there (Picture::noText))      missing.add ("the picture without its words");
    if (! there (Picture::noButtons))   missing.add ("the picture without its buttons");
    if (! there (Picture::finish))      missing.add ("the picture with the one button");
    if (regular == nullptr)     missing.add ("Red Hat Display Regular");
    if (medium == nullptr)      missing.add ("Red Hat Display Medium");
    if (semiBold == nullptr)    missing.add ("Red Hat Display SemiBold");
    if (bold == nullptr)        missing.add ("Red Hat Display Bold");
    if (extraBold == nullptr)   missing.add ("Red Hat Display ExtraBold");

    return missing;
}

juce::Font Assets::make (const juce::Typeface::Ptr& typeface, float height, float spread)
{
    // (Should a typeface ever fail to load, the system's own sans serif stands in.)
    const auto options = typeface != nullptr ? juce::FontOptions (typeface) : juce::FontOptions();
    return juce::Font (options.withHeight (height).withKerningFactor (spread));
}

} // namespace daaura
