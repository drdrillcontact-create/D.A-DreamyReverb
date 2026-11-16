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

    // value text in the centre (never wider than the ring, so that it cannot run over the
    // knob's own label underneath)
    g.setColour (bright);
    g.setFont (makeFont ((big ? 19.0f : 10.5f) * scale, false, 0.0f));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                      ring.reduced (juce::jmax (3.0f * scale, radius * 0.22f)).toNearestInt(),
                      juce::Justification::centred, 1, 0.62f);
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
        g.setColour (green.withAlpha (0.22f));
        g.fillRoundedRectangle (area.expanded (2.0f * scale), corner);
        g.setColour (green.withAlpha (down ? 0.85f : 0.95f));
        g.fillRoundedRectangle (area, corner);
    }
    else
    {
        g.setColour (green.withAlpha (highlighted || down ? 0.10f : 0.02f));
        g.fillRoundedRectangle (area, corner);
    }

    g.setColour (green.withAlpha (on ? 1.0f : (highlighted || down ? 0.95f : 0.65f)));
    g.drawRoundedRectangle (area, corner, 1.0f * scale);
}

void AuroraLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down)
{
    using namespace aurora_colours;

    // the preset browser's arrows and the FREEZE button say what they look like
    const auto id = button.getComponentID();

    if (id == "previous" || id == "next")
    {
        const auto area = button.getLocalBounds().toFloat();
        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float reach = 4.2f * scale;
        const float direction = id == "next" ? 1.0f : -1.0f;

        juce::Path arrow;
        arrow.startNewSubPath (cx - direction * reach, cy - reach);
        arrow.lineTo (cx + direction * reach * 0.55f, cy);
        arrow.lineTo (cx - direction * reach, cy + reach);

        const auto colour = (highlighted || down) ? bright : green;
        g.setColour (colour.withAlpha (button.isEnabled() ? 1.0f : 0.3f));
        g.strokePath (arrow, juce::PathStrokeType (1.8f * scale, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        return;
    }

    g.setColour (button.getToggleState() ? bgTop : green);
    g.setFont (makeFont (11.0f * scale, button.getToggleState(), 0.28f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}


//==============================================================================
// The aurora behind the knobs.
//
// At rest the picture is the design's own: a dark field with soft ribbons of green and blue
// light lying across the lower half, a few stars above them. When the plug-in carries a signal
// the whole field answers it (see Reverb::Life): the level lifts the ribbons and makes them
// travel, the sixteen bands of the spectrum set the stars' brightness and drift from the low
// ones on the left to the high ones on the right, and every onset that the signal's envelope
// reports sends a ring of light outward from the middle.
AuroraField::AuroraField()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    setRepaintsOnMouseActivity (false);

    using namespace aurora_colours;

    // The ribbons are the design's own light: wide, soft, and living in the lower part of the
    // window (the top stays dark, which is what makes the knobs read).
    struct Shape { float height, tilt, amplitude, thickness, alpha; };
    const Shape shapes[] =
    {
        //  height  tilt   amp  thick  alpha
        {   0.30f, -0.16f, 14.0f, 132.0f, 0.30f },   // the wide green band across the middle
        {   0.42f,  0.10f, 18.0f, 104.0f, 0.24f },   // and the blue one that follows it
        {   0.58f, -0.07f, 12.0f,  76.0f, 0.21f },
        {   0.74f,  0.05f, 16.0f, 112.0f, 0.22f },
        {   0.20f, -0.03f,  9.0f,  54.0f, 0.14f },   // a faint one higher up
        {   0.90f,  0.02f, 20.0f,  92.0f, 0.16f },
        {   0.50f,  0.03f, 10.0f,  44.0f, 0.12f },
    };

    const juce::Colour palette[] = { green, blue, green, blue, purple, green, blue };
    juce::Random random (0x0a17a);

    for (size_t i = 0; i < ribbons.size(); ++i)
    {
        auto& r = ribbons[i];
        const auto& shape = shapes[i % (sizeof (shapes) / sizeof (shapes[0]))];

        r.phase     = random.nextFloat() * juce::MathConstants<float>::twoPi;
        r.speed     = 0.05f + 0.04f * random.nextFloat();
        r.height    = shape.height;
        r.thickness = shape.thickness;
        r.amp       = shape.amplitude;
        r.tilt      = shape.tilt;
        r.colour    = palette[i % (sizeof (palette) / sizeof (palette[0]))];
        r.alpha     = shape.alpha;

    }

    for (size_t i = 0; i < stars.size(); ++i)
    {
        auto& s = stars[i];
        s.x       = random.nextFloat();
        s.y       = std::pow (random.nextFloat(), 2.4f) * 0.62f;   // (only in the dark top)
        s.size    = 0.5f + 1.5f * random.nextFloat();
        s.alpha   = 0.06f + 0.30f * random.nextFloat();
        s.twinkle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        s.drift   = 0.4f + 0.9f * random.nextFloat();
    }

    for (auto& r : rings)
    {
        r.radius = 1.0f;
        r.alpha  = 0.0f;
        r.width  = 1.0f;
    }

    buildAuroraImage (restImage,  0.0f);
    buildAuroraImage (awakeImage, 1.0f);
}

void AuroraField::moveTo (const aura::Reverb::Life& l, double secondsSinceLastMove)
{
    const float dt = juce::jlimit (0.001f, 0.2f, (float) secondsSinceLastMove);
    const float motion = getProperties().getWithDefault ("motion", 1.0f);

    // the signal's numbers, followed a little more so that nothing jumps between refreshes
    level      += 0.35f * (l.level.load() - level);
    delayLevel += 0.20f * (l.delayLevel.load() - delayLevel);

    const float onset = l.transient.load();
    transient = juce::jmax (onset, transient * std::pow (0.06f, dt));

    for (int b = 0; b < aura::Reverb::Life::numBands; ++b)
        band[b] += 0.30f * (l.band[b].load() - band[b]);

    // an onset starts a ring of light
    if (onset > 0.55f)
    {
        auto& ring = rings[(size_t) nextRing];
        nextRing = (nextRing + 1) % (int) rings.size();

        if (ring.alpha < 0.05f)
        {
            ring.radius = 0.06f;
            ring.alpha  = 0.35f + 0.45f * onset;
            ring.width  = 1.0f + 1.6f * onset;
        }
    }

    for (auto& ring : rings)
    {
        ring.radius += dt * (0.22f + 0.35f * level) * motion;
        ring.alpha  *= std::pow (0.35f, dt);
    }

    // the ribbons travel with the signal
    const float timeStep = dt * (0.25f + 2.2f * level) * motion;
    time += timeStep;

    for (auto& r : ribbons)
        r.phase += timeStep * r.speed * 6.0f;

    repaint();
}

// The aurora, drawn small and blurred once: the design's own soft light for the rest state,
// and a fuller, wider one for a signal. Drawing it at a sixth of the size and stretching it
// back up is what gives it the design's haze (and costs nothing at every refresh).
void AuroraField::buildAuroraImage (juce::Image& target, float strength) const
{
    // the signal's picture is the same light, one phase further on: it is what travels
    const float phaseShift = 2.4f * strength;

    constexpr float s = 1.0f / 3.0f;
    const int w = juce::roundToInt ((float) DAAuraAudioProcessorEditor::baseWidth * s);
    const int h = juce::roundToInt ((float) DAAuraAudioProcessorEditor::baseHeight * s);

    target = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (target);

    for (const auto& r : ribbons)
    {
        // the design's ribbons lie low and lean a little; a signal lifts them and widens them
        const float height = juce::jlimit (0.05f, 0.98f,
                                           r.height + 0.16f * strength * (0.5f - r.height));
        const float alpha  = r.alpha * (0.92f + 0.10f * strength);
        const float amp    = r.amp   * (0.55f + 1.9f * strength);

        juce::Path path;

        for (int x = -12; x <= w + 12; x += 6)
        {
            const float t = (float) x / (float) w;
            const float y = height * (float) h
                          + (t - 0.5f) * r.tilt * (float) h
                          + std::sin (r.phase + phaseShift + t * 4.1f) * amp * s
                          + std::sin (r.phase * 1.9f + phaseShift * 0.7f + t * 9.0f) * amp * 0.25f * s;

            if (x <= -12)
                path.startNewSubPath ((float) x, y);
            else
                path.lineTo ((float) x, y);
        }

        // its light fades in from the left and out before the right edge: a gradient across
        // the whole picture, which is only ever used once (the picture is finished here)
        juce::ColourGradient gradient (r.colour.withAlpha (0.0f), 0.0f, 0.0f,
                                       r.colour.withAlpha (0.0f), (float) w, 0.0f, false);
        gradient.addColour (0.02, r.colour.withAlpha (0.0f));
        gradient.addColour (0.20, r.colour.withAlpha (alpha * 0.65f));
        gradient.addColour (0.40, r.colour.withAlpha (alpha));
        gradient.addColour (0.68, r.colour.withAlpha (alpha * 0.55f));
        gradient.addColour (0.90, r.colour.withAlpha (alpha * 0.10f));
        gradient.addColour (1.00, r.colour.withAlpha (0.0f));

        g.setGradientFill (gradient);
        g.strokePath (path, juce::PathStrokeType ((r.thickness * (1.0f + 0.30f * strength)) * s * 1.35f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    juce::ImageConvolutionKernel blur (17);
    blur.createGaussianBlur (2.6f);
    blur.applyToImage (target, target, target.getBounds());
}

juce::ColourGradient AuroraField::backdrop()
{
    using namespace aurora_colours;

    const auto area = getLocalBounds().toFloat();

    // the design's own two colours, warmed a little by the signal that passes through
    juce::ColourGradient gradient (bgTop.brighter (0.10f * level),
                                   area.getCentreX(), 0.0f,
                                   bgBottom.brighter (0.16f * level),
                                   area.getCentreX(), area.getBottom(), false);
    gradient.addColour (0.55, bgTop.interpolatedWith (bgBottom, 0.45f).brighter (0.08f * delayLevel));

    return gradient;
}

void AuroraField::paint (juce::Graphics& g)
{
    using namespace aurora_colours;

    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth(), h = area.getHeight();

    if (w < 2.0f || h < 2.0f)
        return;

    // the window is the design's rounded panel: nothing of the aurora is drawn outside it
    const float corner = 16.0f * fieldScale;
    juce::Path panel;
    panel.addRoundedRectangle (area, corner);

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (panel, {});

    g.setGradientFill (backdrop());
    g.fillAll();

    // the aurora's own light: the design's rest picture, and the fuller one a signal brings,
    // blended by the level (both are drawn small and stretched, so they stay soft)
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    const float awake = juce::jlimit (0.0f, 1.0f, level * 1.15f);

    if (restImage.isValid())
    {
        g.setOpacity (1.0f - 0.30f * awake);
        g.drawImage (restImage, area, juce::RectanglePlacement::stretchToFit);
    }

    if (awakeImage.isValid() && awake > 0.01f)
    {
        g.setOpacity (0.42f * awake);
        g.drawImage (awakeImage, area, juce::RectanglePlacement::stretchToFit);
    }

    g.setOpacity (1.0f);

    // the light sits in the middle of the window and fades away towards its top and bottom,
    // so that the aurora is a glow behind the knobs and never a painted band across them
    {
        juce::ColourGradient fade (bgTop.withAlpha (0.55f), 0.0f, 0.0f,
                                   bgTop.withAlpha (0.0f), 0.0f, h * 0.42f, false);
        fade.addColour (0.60, bgTop.withAlpha (0.06f));
        fade.addColour (1.0, bgTop.withAlpha (0.0f));
        g.setGradientFill (fade);
        g.fillRect (0.0f, 0.0f, w, h * 0.42f);

        juce::ColourGradient lower (bgBottom.withAlpha (0.0f), 0.0f, h * 0.55f,
                                    bgBottom.withAlpha (0.55f), 0.0f, h, false);
        g.setGradientFill (lower);
        g.fillRect (0.0f, h * 0.55f, w, h * 0.45f);
    }

    // the stars: the higher bands light the ones on the right
    const auto brightness = [this] (float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, x);
        const float at = t * (float) (aura::Reverb::Life::numBands - 1);
        const int b0 = (int) at;
        const int b1 = juce::jmin (b0 + 1, aura::Reverb::Life::numBands - 1);
        return band[b0] + (band[b1] - band[b0]) * (at - (float) b0);
    };

    for (auto& s : stars)
    {
        const float b = brightness (s.x);
        const float twinkle = 0.70f + 0.30f * std::sin (s.twinkle + time * s.drift * 4.0f);
        const float alpha = s.alpha * twinkle * (0.35f + 1.65f * b);
        const float size  = s.size * fieldScale * (1.0f + 1.4f * transient);

        g.setColour (bright.withAlpha (juce::jlimit (0.0f, 0.95f, alpha)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ s.x * w, s.y * h }));
    }

    // the rings an onset sends outward
    for (auto& ring : rings)
    {
        if (ring.alpha < 0.01f)
            continue;

        const float radius = ring.radius * w;
        const auto centre = juce::Point<float> (w * 0.5f, h * 0.45f);

        g.setColour (green.withAlpha (juce::jlimit (0.0f, 0.5f, ring.alpha * 0.55f)));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                       ring.width * fieldScale);

        g.setColour (blue.withAlpha (juce::jlimit (0.0f, 0.35f, ring.alpha * 0.25f)));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                       ring.width * 3.0f * fieldScale);
    }

    // the design's own edge, a hair line along the panel
    g.setColour (bright.withAlpha (0.055f));
    g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);

    // the signal itself, as a soft light across the middle that follows the wet level
    if (level > 0.01f)
    {
        juce::ColourGradient glow (green.withAlpha (0.10f * level), w * 0.5f, h * 0.45f,
                                   green.withAlpha (0.0f), w * 0.5f, h, true);
        g.setGradientFill (glow);
        g.fillAll();
    }
}

//==============================================================================
DAAuraAudioProcessorEditor::DAAuraAudioProcessorEditor (DAAuraAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    // the aurora, behind everything: it is added first, so the knobs and labels sit in front
    addAndMakeVisible (field);

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

    // the product's name, in front of the aurora (it used to be painted by the editor itself,
    // which the field then covered)
    title.setText ("D.A AURA", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setInterceptsMouseClicks (false, false);
    title.setColour (juce::Label::textColourId, aurora_colours::text);
    addAndMakeVisible (title);

    // the preset browser: the space, the preset, how far through the space it is, and the arrows
    const auto setUpLabel = [this] (juce::Label& label, float height, float kerning, bool bold)
    {
        label.setFont (makeFont (height, bold, kerning));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);
        label.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (label);
    };

    setUpLabel (presetSpace, 10.5f, 0.30f, true);
    setUpLabel (presetName,  19.0f, 0.06f, false);
    setUpLabel (presetCount, 10.0f, 0.24f, false);

    presetSpace.setColour (juce::Label::textColourId, aurora_colours::green.withAlpha (0.85f));
    presetName.setColour  (juce::Label::textColourId, aurora_colours::bright);
    presetCount.setColour (juce::Label::textColourId, aurora_colours::green.withAlpha (0.55f));
    presetCount.setJustificationType (juce::Justification::centredRight);

    const auto setUpArrow = [this] (juce::TextButton& button, const char* id, const char* name)
    {
        button.setComponentID (id);
        button.setName (name);                       // what a screen reader says
        button.setTooltip (name);
        button.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (button);
    };

    setUpArrow (previousPreset, "previous", "Previous preset");
    setUpArrow (nextPreset,     "next",     "Next preset");

    previousPreset.onClick = [this] { proc.stepPreset (-1); refreshPresetLabels(); };
    nextPreset.onClick     = [this] { proc.stepPreset (1);  refreshPresetLabels(); };

    refreshPresetLabels();

    startTimerHz (30);

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

DAAuraAudioProcessorEditor::~DAAuraAudioProcessorEditor()
{
    stopTimer();
    freezeAttachment.reset();
    knobs.clear();
    setLookAndFeel (nullptr);
}

void DAAuraAudioProcessorEditor::timerCallback()
{
    // the aurora moves with the signal the plug-in is carrying (30 times a second)
    static constexpr double refresh = 1.0 / 30.0;
    field.moveTo (proc.reverbLife(), refresh);

    // the browser follows the knobs (a host may change the preset without the arrows)
    if (proc.currentPreset() != shownPreset)
        refreshPresetLabels();
}

void DAAuraAudioProcessorEditor::refreshPresetLabels()
{
    const int index = proc.currentPreset();
    shownPreset = index;

    if (index < 0)
    {
        presetSpace.setText ("NO PRESET",               juce::dontSendNotification);
        presetName.setText  ("--",                      juce::dontSendNotification);
        presetCount.setText ({},                        juce::dontSendNotification);
        return;
    }

    const auto name  = juce::String (aura::presets[index].name);
    const auto space = juce::String (aura::presets[index].space);
    const auto spaceIndex = proc.currentSpace();

    presetSpace.setText (space.toUpperCase(), juce::dontSendNotification);
    presetName.setText  (name,               juce::dontSendNotification);
    presetCount.setText (spaceIndex >= 0
                             ? juce::String (index - aura::spaces[spaceIndex].first + 1)
                                   + " / " + juce::String (aura::spaces[spaceIndex].count)
                             : juce::String (index + 1) + " / " + juce::String (aura::numPresets),
                         juce::dontSendNotification);
}

void DAAuraAudioProcessorEditor::addKnob (const juce::String& paramId, const juce::String& title,
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

void DAAuraAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace aurora_colours;

    const auto area = getLocalBounds().toFloat();
    const float scale = area.getWidth() / (float) baseWidth;

    // (the aurora behind the knobs is the field component, in front of this: see AuroraField)
    if (! field.isVisible() || field.getWidth() == 0)
    {
        g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, area.getHeight(), false));
        g.fillAll();
    }

}

void DAAuraAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    lookAndFeel.setScale (scale);

    field.setBounds (getLocalBounds());
    field.setFieldScale (scale);

    // the browser: under the name on the left, the arrows on the right of the second row
    const auto scaled = [scale] (juce::Rectangle<float> r)
    {
        return r.transformedBy (juce::AffineTransform::scale (scale)).toNearestInt();
    };

    title.setFont (makeFont (20.0f * scale, false, 0.38f));
    title.setBounds (scaled ({ 30.0f, 22.0f, 480.0f, 32.0f }));

    presetSpace.setFont (makeFont (10.5f * scale, true, 0.30f));
    presetSpace.setBounds (scaled ({ 30.0f, 58.0f, 220.0f, 14.0f }));

    presetName.setFont (makeFont (19.0f * scale, false, 0.06f));
    presetName.setBounds (scaled ({ 30.0f, 72.0f, 330.0f, 26.0f }));

    presetCount.setFont (makeFont (10.0f * scale, false, 0.24f));
    presetCount.setBounds (scaled ({ 360.0f, 78.0f, 100.0f, 16.0f }));

    previousPreset.setBounds (scaled ({ 470.0f, 72.0f, 26.0f, 26.0f }));
    nextPreset.setBounds     (scaled ({ 502.0f, 72.0f, 26.0f, 26.0f }));

    freezeButton.setBounds (juce::Rectangle<float> (594.0f, 26.0f, 96.0f, 24.0f)
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
