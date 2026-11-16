#include "InstallerPicture.h"

namespace daaura
{

namespace
{
/** For one pixel of the copy: which of the source's pixels (in a row or a column) go into
    it, and how much of each. */
struct Taps
{
    int first = 0;
    std::vector<float> weights;
};

double lanczos (double x)
{
    constexpr double lobes = 3.0;

    if (std::abs (x) < 1.0e-9)  return 1.0;
    if (std::abs (x) >= lobes)  return 0.0;

    const double px = juce::MathConstants<double>::pi * x;
    return lobes * std::sin (px) * std::sin (px / lobes) / (px * px);
}

std::vector<Taps> makeTaps (int length, double sourcePixelsPerPixel)
{
    // (Shrinking, the filter is widened by as much: it then averages over what it leaves out.)
    const double widening = juce::jmax (1.0, sourcePixelsPerPixel);
    const double reach = 3.0 * widening;

    std::vector<Taps> taps ((size_t) length);

    for (int i = 0; i < length; ++i)
    {
        const double centre = ((double) i + 0.5) * sourcePixelsPerPixel - 0.5;

        auto& t = taps[(size_t) i];
        t.first = (int) std::ceil (centre - reach);
        t.weights.resize ((size_t) ((int) std::floor (centre + reach) - t.first + 1));

        double sum = 0.0;

        for (size_t k = 0; k < t.weights.size(); ++k)
        {
            const double weight = lanczos (((double) (t.first + (int) k) - centre) / widening);
            t.weights[k] = (float) weight;
            sum += weight;
        }

        for (auto& weight : t.weights)
            weight = (float) ((double) weight / sum);
    }

    return taps;
}
}

juce::Image resampled (const juce::Image& source, int width, int height, double sourcePixelsPerPixel)
{
    jassert (source.getFormat() == juce::Image::RGB);

    // (in plain memory, three bytes to the pixel, on every system)
    juce::Image copy (juce::Image::RGB, juce::jmax (1, width), juce::jmax (1, height), false, juce::SoftwareImageType());

    if (! source.isValid() || source.getFormat() != juce::Image::RGB || sourcePixelsPerPixel <= 0.0)
    {
        copy.clear (copy.getBounds(), juce::Colours::black);
        return copy;
    }

    const int sourceWidth = source.getWidth(), sourceHeight = source.getHeight();
    width = copy.getWidth();
    height = copy.getHeight();

    const auto across = makeTaps (width, sourcePixelsPerPixel);
    const auto down   = makeTaps (height, sourcePixelsPerPixel);

    const juce::Image::BitmapData from (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData to (copy, juce::Image::BitmapData::writeOnly);

    if (from.data == nullptr || to.data == nullptr || from.pixelStride < 3 || to.pixelStride < 3)
    {
        copy.clear (copy.getBounds(), juce::Colours::black);
        return copy;
    }

    const int channels = 3;
    const auto rowLength = (size_t) (width * channels);

    // First every row of the source is brought to the copy's width...
    std::vector<float> rows ((size_t) sourceHeight * rowLength);

    for (int y = 0; y < sourceHeight; ++y)
    {
        const juce::uint8* const line = from.getLinePointer (y);
        float* out = rows.data() + (size_t) y * rowLength;

        for (int x = 0; x < width; ++x)
        {
            const auto& t = across[(size_t) x];
            float sum[3] { 0.0f, 0.0f, 0.0f };

            for (size_t k = 0; k < t.weights.size(); ++k)
            {
                const juce::uint8* const pixel = line + (size_t) juce::jlimit (0, sourceWidth - 1, t.first + (int) k) * (size_t) from.pixelStride;
                const float weight = t.weights[k];

                sum[0] += weight * (float) pixel[0];
                sum[1] += weight * (float) pixel[1];
                sum[2] += weight * (float) pixel[2];
            }

            *out++ = sum[0];
            *out++ = sum[1];
            *out++ = sum[2];
        }
    }

    // ...then those rows to its height
    std::vector<float> line (rowLength);

    for (int y = 0; y < height; ++y)
    {
        const auto& t = down[(size_t) y];
        std::fill (line.begin(), line.end(), 0.0f);

        for (size_t k = 0; k < t.weights.size(); ++k)
        {
            const float* const row = rows.data() + (size_t) juce::jlimit (0, sourceHeight - 1, t.first + (int) k) * rowLength;
            const float weight = t.weights[k];

            for (size_t i = 0; i < rowLength; ++i)
                line[i] += weight * row[i];
        }

        juce::uint8* out = to.getLinePointer (y);

        for (int x = 0; x < width; ++x)
        {
            for (int c = 0; c < channels; ++c)
                out[c] = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (line[(size_t) (x * channels + c)]));

            out += to.pixelStride;
        }
    }

    return copy;
}

Likeness compare (const juce::Image& drawn, const juce::Image& picture, juce::Point<int> at,
                  const juce::Path& outline, int rows)
{
    Likeness result;

    if (! (drawn.isValid() && picture.isValid() && drawn.hasAlphaChannel()))
        return result;

    const int width = drawn.getWidth();
    rows = juce::jmin (rows, drawn.getHeight(), picture.getHeight() - at.y);

    if (at.x < 0 || at.y < 0 || at.x + width > picture.getWidth())
        return result;

    // How much of each pixel lies inside the outline: 255 is all of it, 0 none
    juce::Image cover (juce::Image::SingleChannel, width, drawn.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (cover);
        g.setColour (juce::Colours::white);
        g.fillPath (outline);
    }

    const juce::Image::BitmapData c (cover, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData d (drawn, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData p (picture, juce::Image::BitmapData::readOnly);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int covered = *c.getPixelPointer (x, y);
            const auto colour = d.getPixelColour (x, y);

            if (covered == 255)
            {
                const auto wanted = p.getPixelColour (at.x + x, at.y + y);
                const int out = colour.getAlpha() != 255 ? 255
                                                         : juce::jmax (std::abs ((int) colour.getRed()   - (int) wanted.getRed()),
                                                                       std::abs ((int) colour.getGreen() - (int) wanted.getGreen()),
                                                                       std::abs ((int) colour.getBlue()  - (int) wanted.getBlue()));
                ++result.inside;
                result.differing += out != 0 ? 1 : 0;
                result.worst = juce::jmax (result.worst, out);
            }
            else if (covered == 0)
            {
                ++result.outside;
                result.spilt += colour.getAlpha() != 0 ? 1 : 0;
            }
            else
            {
                ++result.onTheOutline;
            }
        }
    }

    return result;
}

} // namespace daaura
