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
// At rest the picture is the design's own: a dark field with faint ribbons and a few stars.
// When the plug-in carries a signal the whole field answers it (see Reverb::Life): the level
// lifts the ribbons and makes them travel, the sixteen bands of the spectrum set the stars'
// brightness and drift from the low ones on the left to the high ones on the right, and every
// onset that the signal's envelope reports sends a ring of light outward from the middle.
// The motion is scaled by the "aurora" setting of the window, so it can be set to still.
AuroraField::AuroraField()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    setRepaintsOnMouseActivity (false);

    using namespace aurora_colours;

    const juce::Colour palette[] = { green, blue, purple, green, blue };

    for (size_t i = 0; i < ribbons.size(); ++i)
    {
        auto& r = ribbons[i];
        const float t = (float) i / (float) (ribbons.size() - 1);

        r.phase     = t * 5.3f + 0.7f;
        r.speed     = 0.05f + 0.03f * (float) ((i * 7) % 5);
        r.height    = 0.20f + 0.60f * t;
        r.thickness = 18.0f + 30.0f * (float) ((i * 3) % 4) * 0.33f;
        r.amp       = 10.0f + 22.0f * (float) ((i * 5) % 3);
        r.colour    = palette[i % 5];
        r.alpha     = 0.05f + 0.035f * t;
    }

    juce::Random random (0x0a17a);

    for (size_t i = 0; i < stars.size(); ++i)
    {
        auto& s = stars[i];
        s.x       = random.nextFloat();
        s.y       = std::pow (random.nextFloat(), 1.7f);      // (denser towards the top)
        s.size    = 0.6f + 1.8f * random.nextFloat();
        s.alpha   = 0.10f + 0.55f * random.nextFloat();
        s.twinkle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        s.drift   = 0.4f + 0.9f * random.nextFloat();
    }

    for (auto& r : rings)
    {
        r.radius = 1.0f;
        r.alpha  = 0.0f;
        r.width  = 1.0f;
    }
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

    // the ribbons travel with the signal, and their light follows it
    const float timeStep = dt * (0.25f + 2.2f * level) * motion;
    time += timeStep;

    for (auto& r : ribbons)
        r.phase += timeStep * r.speed * 6.0f;

    repaint();
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

    // the ribbons: the design's three, plus the ones the signal brings out of the dark
    const int lit = juce::jlimit (0, (int) ribbons.size() - 1, juce::roundToInt (1.0f + 5.0f * level));

    for (int i = 0; i < (int) ribbons.size(); ++i)
    {
        auto& r = ribbons[(size_t) i];
        const bool awake = i < lit;

        if (! awake && r.alpha * 0.4f < 0.004f)
            continue;

        juce::Path path;
        const float baseY = r.height * h;
        const float step = w / 64.0f;
        path.startNewSubPath (-step, baseY + std::sin (r.phase) * r.amp * (0.4f + level));

        for (float x = 0.0f; x <= w + step; x += step)
        {
            const float t = x / juce::jmax (1.0f, w);
            const float y = baseY
                          + std::sin (r.phase + t * 5.4f) * r.amp * (0.4f + 1.6f * level)
                          + std::sin (r.phase * 1.7f + t * 11.0f) * r.amp * 0.3f;
            path.lineTo (x, y);
        }

        juce::ColourGradient gradient (r.colour.withAlpha (0.0f), 0.0f, 0.0f,
                                       r.colour.withAlpha (0.0f), w, 0.0f, false);
        const float glow = awake ? r.alpha * (0.45f + 1.0f * level + 0.30f * delayLevel) : r.alpha * 0.35f;
        gradient.addColour (0.30, r.colour.withAlpha (glow));
        gradient.addColour (0.70, r.colour.withAlpha (glow * 0.75f));

        g.setGradientFill (gradient);

        // three widths, each one dimmer than the one inside it: light, not a painted band
        const float thickness = r.thickness * fieldScale * (1.0f + 0.35f * level);
        for (const auto [width, share] : { std::pair<float, float> { 2.6f, 0.22f },
                                           std::pair<float, float> { 1.4f, 0.38f },
                                           std::pair<float, float> { 0.7f, 1.0f } })
        {
            g.setOpacity (share);
            g.strokePath (path, juce::PathStrokeType (thickness * width,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        g.setOpacity (1.0f);
    }

    // the light sits in the middle of the window and fades away towards its top and bottom,
    // so that the aurora is a glow behind the knobs and never a painted band across them
    {
        juce::ColourGradient fade (bgTop.withAlpha (0.85f), 0.0f, 0.0f,
                                   bgTop.withAlpha (0.0f), 0.0f, h * 0.42f, false);
        fade.addColour (0.60, bgTop.withAlpha (0.10f));
        fade.addColour (1.0, bgTop.withAlpha (0.0f));
        g.setGradientFill (fade);
        g.fillRect (0.0f, 0.0f, w, h * 0.42f);

        juce::ColourGradient lower (bgBottom.withAlpha (0.0f), 0.0f, h * 0.55f,
                                    bgBottom.withAlpha (0.80f), 0.0f, h, false);
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

    g.setColour (text);
    g.setFont (makeFont (20.0f * scale, false, 0.38f));
    g.drawText ("D.A AURA",
                juce::Rectangle<float> (30.0f * scale, 22.0f * scale, 480.0f * scale, 32.0f * scale),
                juce::Justification::centredLeft, false);
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
