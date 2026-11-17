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

    // (the preset's name is a button now, so it asks for a button's colours as well as the
    //  colours below; the others are what the arrows and FREEZE are drawn with)
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, aurora_colours::text);
    setColour (juce::TextButton::textColourOnId, aurora_colours::bgTop);

    // the browser's menu, in the plug-in's own colours
    setColour (juce::PopupMenu::backgroundColourId, aurora_colours::bgTop.withAlpha (0.97f));
    setColour (juce::PopupMenu::textColourId, aurora_colours::text);
    setColour (juce::PopupMenu::headerTextColourId, aurora_colours::text.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, aurora_colours::green.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, aurora_colours::bright);
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

    // (the preset's name has no frame of its own: it is text over the aurora)
    if (button.getComponentID() == "presetname")
        return;

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

    // (the preset's name: the big one, drawn like the label it stands in for)
    if (id == "presetname")
    {
        const auto colour = (highlighted || down) ? bright : text;
        g.setColour (colour);
        g.setFont (makeFont (17.0f * scale, false, 0.06f));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                          juce::Justification::centredLeft, 1, 0.6f);
        return;
    }

    g.setColour (button.getToggleState() ? bgTop : green);
    g.setFont (makeFont (11.0f * scale, button.getToggleState(), 0.28f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}


//==============================================================================
// The aurora behind the knobs.
//
// The design's light: one wide green ridge running up from a little left of the middle to the
// top right corner, a blue-violet band following above it, a low teal wash across the bottom
// half, and a purple one on the right. At rest they are exactly where the design puts them.
//
// When the plug-in carries a signal the whole field answers it (see Reverb::Life): the level
// opens the bands out and lifts them, the ribbons travel faster, the sixteen bands of the
// spectrum make the stars in the dark top twinkle from the low ones on the left to the high
// ones on the right, and each onset sends a ring of light outward from the middle.
//
// Every band is a plain filled path with a vertical gradient - nothing pre-rendered - so that
// it looks the same on Windows and on macOS, at any window size.
AuroraField::AuroraField()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    setRepaintsOnMouseActivity (false);

    using namespace aurora_colours;

    //                                              from              to                bow   width strength reach fade  colour
    bands =
    {{
        // the green ridge: from the left of the middle up to the top right, the design's own light
        // (it is the brightest thing in the design, and it climbs steeply towards the right)
        { { -0.06f, 0.55f }, { 1.06f, 0.09f }, 0.05f, 0.180f, 0.52f, 1.00f, 0.05f, green,  0.024f, 0.0f, 0.9f, 0.10f },
        { { -0.06f, 0.47f }, { 1.06f, 0.15f }, 0.04f, 0.165f, 0.29f, 1.00f, 0.08f, green,  0.030f, 2.6f, 1.0f, 0.08f },
        // the blue-violet above and to the right of it
        { { 0.10f, 0.30f }, { 1.06f, 0.01f }, 0.02f, 0.200f, 0.26f, 1.00f, 0.12f, blue,   0.026f, 1.7f, 0.8f, 0.08f },
        // the low teal wash across the lower half
        { { -0.06f, 0.76f }, { 1.06f, 0.58f }, 0.03f, 0.160f, 0.22f, 1.00f, 0.06f, green,  0.030f, 3.1f, 1.1f, 0.06f },
        // a purple one towards the right, low down
        { { 0.30f, 0.68f }, { 1.06f, 0.44f }, 0.03f, 0.160f, 0.17f, 1.00f, 0.12f, purple, 0.026f, 4.4f, 1.0f, 0.05f },
        // the even glow the design has right across the middle of the window
        { { -0.06f, 0.68f }, { 1.06f, 0.28f }, 0.06f, 0.320f, 0.15f, 1.00f, 0.08f, green,  0.034f, 5.3f, 0.7f, 0.06f },
        // and a faint one across the upper middle, which is where the design's light is widest
        { { -0.06f, 0.55f }, { 1.06f, 0.17f }, 0.05f, 0.250f, 0.12f, 1.00f, 0.10f, blue,   0.030f, 6.1f, 0.6f, 0.05f },
    }};

    juce::Random random (0x0a17a);

    for (size_t i = 0; i < stars.size(); ++i)
    {
        auto& s = stars[i];
        s.x       = random.nextFloat();
        s.y       = std::pow (random.nextFloat(), 2.2f) * 0.55f;   // (only in the dark top)
        s.size    = 0.6f + 1.5f * random.nextFloat();
        s.alpha   = 0.05f + 0.26f * random.nextFloat();
        s.twinkle = random.nextFloat() * juce::MathConstants<float>::twoPi;
    }
}

void AuroraField::moveTo (const aura::Reverb::Life& l, double secondsSinceLastMove)
{
    const float dt = juce::jlimit (0.001f, 0.2f, (float) secondsSinceLastMove);

    // the signal's numbers, followed so that nothing jumps between refreshes
    level      += 0.30f * (l.level.load() - level);
    delayLevel += 0.20f * (l.delayLevel.load() - delayLevel);

    const float onset = l.transient.load();
    transient = juce::jmax (onset, transient * std::pow (0.06f, dt));

    for (int b = 0; b < aura::Reverb::Life::numBands; ++b)
        band[b] += 0.30f * (l.band[b].load() - band[b]);

    // the bands travel - slowly at rest, so that the window is never a still picture, and
    // quickly with a signal
    const float step = dt * (0.85f + 2.2f * level);
    time += step;

    for (auto& b : bands)
        b.phase += step * b.speed * 2.0f;

    repaint();
}

float AuroraField::centreAt (const Band& b, float t, float awakeness) const
{
    const float across = juce::jlimit (0.0f, 1.0f, t);

    float y = b.from.y + (b.to.y - b.from.y) * across;

    // the line bows a little (a parabola through the middle of its span, 0 at both ends)
    y += b.bow * std::sin (across * juce::MathConstants<float>::pi);

    // it undulates - a little at rest, more with a signal
    if (b.wave > 0.0001f)
        y += b.wave * (0.30f + 1.30f * awakeness) * std::sin (b.phase + across * 5.4f);

    // a signal lifts the light towards the middle of the window
    y += b.lift * awakeness * (0.5f - y);

    return y;
}

void AuroraField::drawBand (juce::Graphics& g, const Band& b, float awakeness, float w, float h) const
{
    // A signal opens the band out and makes it brighter.
    const float width    = b.width    * (1.0f + 0.30f * awakeness) * h;
    const float strength = b.strength * (1.0f + 0.10f * awakeness);

    if (width < 0.5f || strength <= 0.001f)
        return;

    // The band's shape: its centre line, taken out to half its thickness on each side. It runs
    // from before the left edge to beyond the right one, so that the window's own rounded panel
    // is what ends it - the design's light leaves the window, it does not stop inside it.
    const int steps = juce::jlimit (24, 128, (int) (w / 6.0f));
    const float from = -0.06f, to = 1.06f;

    const auto halfAt = [&b, awakeness, width] (float t)
    {
        // (a signal makes it breathe; at rest this is the same all along)
        return 0.5f * width * (1.0f + (0.10f + 0.22f * awakeness) * std::sin (t * 4.2f + b.phase));
    };

    juce::Path shape;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = from + (to - from) * (float) i / (float) steps;
        const float x = t * w;
        const float y = centreAt (b, t, awakeness) * h;

        if (i == 0) shape.startNewSubPath (x, y - halfAt (t));
        else        shape.lineTo (x, y - halfAt (t));
    }

    for (int i = steps; i >= 0; --i)
    {
        const float t = from + (to - from) * (float) i / (float) steps;
        shape.lineTo (t * w, centreAt (b, t, awakeness) * h + halfAt (t));
    }

    shape.closeSubPath();

    // The light across the band: brightest along its centre line, thinning to nothing at either
    // edge. The gradient runs *across* the band (perpendicular to the way it travels), so the
    // light is even along its whole length however steeply it climbs - and, being one gradient
    // over one shape, there is no step or seam anywhere in it. (A gradient down the screen
    // instead would be even only on a band that runs level; a gradient along the band would end
    // in a hard edge at each of its ends.)
    const auto start  = juce::Point<float> (b.from.x * w, b.from.y * h);
    const auto finish = juce::Point<float> (b.to.x * w, b.to.y * h);
    const auto along  = finish - start;
    const float length = juce::jmax (1.0f, along.getDistanceFromOrigin());
    const auto across = juce::Point<float> (-along.y / length, along.x / length);

    const auto middle = juce::Point<float> ((start.x + finish.x) * 0.5f, (start.y + finish.y) * 0.5f);
    const float half = width * 0.62f;

    juce::ColourGradient glow (b.colour.withAlpha (0.0f),
                               middle.x - across.x * half, middle.y - across.y * half,
                               b.colour.withAlpha (0.0f),
                               middle.x + across.x * half, middle.y + across.y * half,
                               false);

    const float s = juce::jlimit (0.0f, 1.0f, strength);
    glow.addColour (0.00, b.colour.withAlpha (0.0f));
    glow.addColour (0.18, b.colour.withAlpha (s * 0.16f));
    glow.addColour (0.36, b.colour.withAlpha (s * 0.46f));
    glow.addColour (0.50, b.colour.withAlpha (s * 0.62f));
    glow.addColour (0.64, b.colour.withAlpha (s * 0.46f));
    glow.addColour (0.82, b.colour.withAlpha (s * 0.16f));
    glow.addColour (1.00, b.colour.withAlpha (0.0f));

    g.setGradientFill (glow);
    g.fillPath (shape);
}

juce::ColourGradient AuroraField::backdrop() const
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

    const float awakeness = juce::jlimit (0.0f, 1.0f, level * 1.20f);

    // the design's light, and what a signal makes of it
    for (const auto& b : bands)
        drawBand (g, b, awakeness, w, h);

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
        const float twinkle = 0.70f + 0.30f * std::sin (s.twinkle + time * 2.4f);
        const float alpha = s.alpha * twinkle * (0.55f + 1.60f * b);
        const float size  = s.size * fieldScale * (1.0f + 1.2f * transient);

        if (alpha <= 0.003f)
            continue;

        g.setColour (bright.withAlpha (juce::jlimit (0.0f, 0.85f, alpha)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ s.x * w, s.y * h }));
    }

    // the design's own edge, a hair line along the panel
    g.setColour (bright.withAlpha (0.055f));
    g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
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
    setUpLabel (presetCount, 10.0f, 0.24f, false);

    presetSpace.setColour (juce::Label::textColourId, aurora_colours::green.withAlpha (0.85f));
    presetCount.setColour (juce::Label::textColourId, aurora_colours::green.withAlpha (0.55f));
    presetCount.setJustificationType (juce::Justification::centredRight);

    // the preset's name: a button, so that clicking it opens the whole list (it is drawn as
    // plain text over the aurora: see AuroraLookAndFeel)
    presetName.setComponentID ("presetname");
    presetName.setName ("Preset");
    presetName.setTooltip ("Choose a preset");
    presetName.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    presetName.setTriggeredOnMouseDown (false);
    presetName.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetName);

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

    // The size the window was last left at. It is read before the limits are set, because
    // setting them puts the window into the smallest size allowed (and resized() would then
    // write that size back into the state). The property comes back as a var, and a var that
    // XML has round-tripped is a string - reading it with a cast reads both, a string included.
    const auto saved = proc.apvts.state.getProperty ("editorWidth", baseWidth);
    const int savedWidth = juce::jlimit (minWidth, maxWidth,
                                         saved.isVoid() ? baseWidth : (int) saved);

    // resizable with a fixed aspect ratio
    const double ratio = (double) baseWidth / (double) baseHeight;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (minWidth, juce::roundToInt (minWidth / ratio),
                     maxWidth, juce::roundToInt (maxWidth / ratio));

    setSize (savedWidth, juce::roundToInt ((double) savedWidth / ratio));
    isSettingUp = false;
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
        presetName.setButtonText ("--");
        presetCount.setText ({},                        juce::dontSendNotification);
        return;
    }

    const auto name  = juce::String (aura::presets[index].name);
    const auto space = juce::String (aura::presets[index].space);
    const auto spaceIndex = proc.currentSpace();

    presetSpace.setText (space.toUpperCase(), juce::dontSendNotification);
    presetName.setButtonText (name);
    presetCount.setText (spaceIndex >= 0
                             ? juce::String (index - aura::spaces[spaceIndex].first + 1)
                                   + " / " + juce::String (aura::spaces[spaceIndex].count)
                             : juce::String (index + 1) + " / " + juce::String (aura::numPresets),
                         juce::dontSendNotification);
}

juce::PopupMenu DAAuraAudioProcessorEditor::buildPresetMenu (int current)
{
    // Every preset, under the space it belongs to, with the one that is on ticked - so that all
    // fifty can be read at once, and picked.
    juce::PopupMenu menu;

    for (int s = 0; s < aura::numSpaces; ++s)
    {
        const auto& space = aura::spaces[s];

        juce::PopupMenu group;

        for (int i = space.first; i < space.first + space.count; ++i)
            group.addItem (i + 1, juce::String (aura::presets[i].name), true, i == current);

        menu.addSubMenu (juce::String (space.name).toUpperCase() + "   " + juce::String (space.count) + " presets",
                         group, true);
    }

    return menu;
}

void DAAuraAudioProcessorEditor::showPresetMenu()
{
    auto menu = buildPresetMenu (proc.currentPreset());

    // (it is opened under the name, as wide as the name's own room, so that it reads in place)
    const auto where = localAreaToGlobal (presetName.getBounds());

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&presetName)
                            .withTargetScreenArea (where.withWidth (juce::jmax (240, where.getWidth())))
                            .withMinimumWidth (240)
                            .withMaximumNumColumns (2),
                        [this] (int chosen)
                        {
                            if (chosen > 0)
                            {
                                proc.applyPreset (chosen - 1);
                                refreshPresetLabels();
                            }
                        });
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

    presetName.setBounds (scaled ({ 30.0f, 69.0f, 340.0f, 30.0f }));
    lookAndFeel.setScale (scale);

    presetCount.setFont (makeFont (10.0f * scale, false, 0.24f));
    presetCount.setBounds (scaled ({ 378.0f, 78.0f, 62.0f, 16.0f }));

    // the two arrows, right after the preset's name (where the browser is read) rather than
    // over the knobs; each has a box a little wider than the circle drawn in it, so that it is
    // easy to hit
    previousPreset.setBounds (scaled ({ 452.0f, 70.0f, 30.0f, 30.0f }));
    nextPreset.setBounds     (scaled ({ 488.0f, 70.0f, 30.0f, 30.0f }));

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
    // (not while the window is being set up: the size it is given at the end is the one to keep)
    if (getWidth() > 0 && ! isSettingUp)
        proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
}
