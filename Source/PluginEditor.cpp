#include "PluginEditor.h"

namespace aurora_colours
{
    const juce::Colour bgTop    { 0xff031018 };
    const juce::Colour bgBottom { 0xff0a2a33 };
    const juce::Colour green    { 0xff3dffb4 };
    const juce::Colour blue     { 0xff6f8cff };
    const juce::Colour purple   { 0xffc06bff };
    const juce::Colour text     { 0xffd9fff3 };
    const juce::Colour bright   { 0xffeafff8 };
}

namespace
{
    juce::Font makeFont (float height, bool bold, float kerning)
    {
        juce::Font f (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
        f.setExtraKerningFactor (kerning);
        return f;
    }
}

//==============================================================================
AuroraLookAndFeel::AuroraLookAndFeel()
{
    setColour (juce::Label::textColourId, aurora_colours::text);
    setColour (juce::ResizableWindow::backgroundColourId, aurora_colours::bgTop);
}

void AuroraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float pos, float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace aurora_colours;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmax (4.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 8.0f * scale);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool big = radius > 40.0f * scale;
    const bool active = slider.isMouseOverOrDragging();
    const auto ring = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // body
    g.setColour (bgTop.withAlpha (0.45f));
    g.fillEllipse (ring);
    g.setColour (text.withAlpha (active ? 0.38f : 0.18f));
    g.drawEllipse (ring, 1.5f * scale);

    // value arc with a soft glow
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    const auto stroke = [&] (float w, float alpha)
    {
        g.setColour (green.withAlpha (alpha));
        g.strokePath (arc, juce::PathStrokeType (w * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    stroke (10.0f, 0.07f);
    stroke (6.5f, 0.14f);
    stroke (2.6f, 1.0f);

    // glowing dot at the tip
    const juce::Point<float> tip (centre.x + std::sin (angle) * radius, centre.y - std::cos (angle) * radius);
    const float dot = (big ? 5.0f : 3.6f) * scale;
    g.setColour (bright.withAlpha (0.16f));
    g.fillEllipse (juce::Rectangle<float> (dot * 4.0f, dot * 4.0f).withCentre (tip));
    g.setColour (green.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (dot * 2.8f, dot * 2.8f).withCentre (tip));
    g.setColour (bright);
    g.fillEllipse (juce::Rectangle<float> (dot * 2.0f, dot * 2.0f).withCentre (tip));

    // value text in the centre
    g.setColour (bright);
    g.setFont (makeFont ((big ? 19.0f : 10.5f) * scale, false, 0.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()),
                ring.reduced (3.0f * scale), juce::Justification::centred, false);
}

void AuroraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool highlighted, bool down)
{
    using namespace aurora_colours;

    const auto area = button.getLocalBounds().toFloat().reduced (1.0f * scale);
    const float corner = area.getHeight() * 0.5f;
    const bool on = button.getToggleState();

    if (on)
    {
        g.setColour (green.withAlpha (0.25f));
        g.fillRoundedRectangle (area.expanded (1.0f * scale), corner);
        g.setColour (green.withAlpha (down ? 0.75f : 0.9f));
        g.fillRoundedRectangle (area, corner);
    }
    else
    {
        g.setColour (green.withAlpha (highlighted || down ? 0.16f : 0.04f));
        g.fillRoundedRectangle (area, corner);
    }

    g.setColour (green.withAlpha (on ? 1.0f : 0.7f));
    g.drawRoundedRectangle (area, corner, 1.2f * scale);
}

void AuroraLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    using namespace aurora_colours;

    g.setColour (button.getToggleState() ? bgTop : green);
    g.setFont (makeFont (11.0f * scale, button.getToggleState(), 0.28f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
DreamyReverbAudioProcessorEditor::DreamyReverbAudioProcessorEditor (DreamyReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    // three hero knobs
    addKnob ("decay",    "DECAY",     180.0f, 172.0f, 56.0f);
    addKnob ("size",     "SIZE",      360.0f, 172.0f, 56.0f);
    addKnob ("shimmer",  "SHIMMER",   540.0f, 172.0f, 56.0f);

    // bottom row
    const char* ids[]    = { "mix", "predelay",  "highcut",  "lowcut",  "moddepth",  "modrate",  "width" };
    const char* titles[] = { "MIX", "PRE-DELAY", "HIGH CUT", "LOW CUT", "MOD DEPTH", "MOD RATE", "WIDTH" };
    for (int i = 0; i < 7; ++i)
        addKnob (ids[i], titles[i], 75.0f + 95.0f * (float) i, 338.0f, 25.0f);

    freezeButton.setClickingTogglesState (true);
    freezeButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "freeze", freezeButton);

    buildAuroraImage();

    // resizable with a fixed aspect ratio
    const double ratio = (double) baseWidth / (double) baseHeight;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (minWidth, juce::roundToInt (minWidth / ratio),
                     maxWidth, juce::roundToInt (maxWidth / ratio));

    const int savedWidth = juce::jlimit (minWidth, maxWidth,
                                         (int) proc.apvts.state.getProperty ("editorWidth", baseWidth));
    setSize (savedWidth, juce::roundToInt (savedWidth / ratio));
}

DreamyReverbAudioProcessorEditor::~DreamyReverbAudioProcessorEditor()
{
    freezeAttachment.reset();
    knobs.clear();
    setLookAndFeel (nullptr);
}

void DreamyReverbAudioProcessorEditor::addKnob (const juce::String& paramId, const juce::String& title,
                                                float cx, float cy, float radius)
{
    auto* k = knobs.add (new Knob());
    k->cx = cx;
    k->cy = cy;
    k->radius = radius;

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k->slider);

    k->label.setText (title, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setInterceptsMouseClicks (false, false);
    k->label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (k->label);

    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k->slider);
}

// The blurred aurora ribbons are rendered once at low resolution and stretched in paint().
void DreamyReverbAudioProcessorEditor::buildAuroraImage()
{
    using namespace aurora_colours;

    const float s = 1.0f / 6.0f;
    const int w = juce::roundToInt ((float) baseWidth * s);
    const int h = juce::roundToInt ((float) baseHeight * s);

    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (img);

        juce::ColourGradient grad (green.withAlpha (0.0f), 0.0f, 0.0f,
                                   purple.withAlpha (0.0f), (float) w, 0.0f, false);
        grad.addColour (0.40, green.withAlpha (0.62f));
        grad.addColour (0.75, blue.withAlpha (0.56f));

        juce::Path top;
        top.startNewSubPath (-20.0f, 150.0f);
        top.cubicTo (150.0f, 40.0f, 300.0f, 230.0f, 460.0f, 120.0f);
        top.cubicTo (620.0f, 10.0f, 680.0f, 60.0f, 760.0f, 150.0f);
        top.applyTransform (juce::AffineTransform::scale (s));
        g.setGradientFill (grad);
        g.strokePath (top, juce::PathStrokeType (70.0f * s));

        juce::Path bottom;
        bottom.startNewSubPath (-20.0f, 300.0f);
        bottom.cubicTo (200.0f, 220.0f, 330.0f, 360.0f, 520.0f, 280.0f);
        bottom.cubicTo (710.0f, 200.0f, 700.0f, 250.0f, 760.0f, 300.0f);
        bottom.applyTransform (juce::AffineTransform::scale (s));
        g.setGradientFill (grad);
        g.setOpacity (0.6f);
        g.strokePath (bottom, juce::PathStrokeType (40.0f * s));
    }

    juce::ImageConvolutionKernel blur (23);
    blur.createGaussianBlur (3.7f);
    blur.applyToImage (img, img, img.getBounds());

    aurora = img;
}

void DreamyReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace aurora_colours;

    const auto area = getLocalBounds().toFloat();
    const float scale = area.getWidth() / (float) baseWidth;

    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, area.getHeight(), false));
    g.fillAll();

    if (aurora.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (aurora, area);
    }

    g.setColour (text);
    g.setFont (makeFont (20.0f * scale, false, 0.38f));
    g.drawText ("D.A AURA",
                juce::Rectangle<float> (30.0f * scale, 22.0f * scale, 480.0f * scale, 32.0f * scale),
                juce::Justification::centredLeft, false);
}

void DreamyReverbAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    lookAndFeel.setScale (scale);

    freezeButton.setBounds (juce::Rectangle<float> (590.0f, 24.0f, 104.0f, 28.0f)
                                .transformedBy (juce::AffineTransform::scale (scale)).toNearestInt());

    for (auto* k : knobs)
    {
        const bool big = k->radius > 40.0f;
        const float half = k->radius + 8.0f;

        k->slider.setBounds (juce::Rectangle<float> (k->cx - half, k->cy - half, half * 2.0f, half * 2.0f)
                                 .transformedBy (juce::AffineTransform::scale (scale)).toNearestInt());

        const float labelW = big ? 170.0f : 95.0f;
        const float labelH = big ? 22.0f : 18.0f;
        k->label.setFont (makeFont ((big ? 13.0f : 11.0f) * scale, true, big ? 0.18f : 0.14f));
        k->label.setBounds (juce::Rectangle<float> (k->cx - labelW * 0.5f, k->cy + k->radius + 7.0f, labelW, labelH)
                                .transformedBy (juce::AffineTransform::scale (scale)).toNearestInt());
    }

    // remember the size for the next time the editor is opened / the session is reloaded
    proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
}
