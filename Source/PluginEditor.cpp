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
    const juce::Colour menuBg   { 0xff06181f };
}

namespace
{
    using Weight = AuroraLookAndFeel::Weight;

    // Jost: ascender + descender = 1.445 em. JUCE font heights are measured over that span.
    constexpr float heightPerEm = 1.445f;

    juce::Font makeFont (const juce::Typeface::Ptr& face, float emSize, float tracking)
    {
        const float height = juce::jmax (1.0f, emSize * heightPerEm);
        const float kerning = tracking / heightPerEm;   // JUCE kerning is relative to the font height

        if (face != nullptr)
            return juce::Font (juce::FontOptions (face).withHeight (height)).withExtraKerningFactor (kerning);

        return juce::Font (juce::FontOptions (height * 0.85f)).withExtraKerningFactor (kerning);
    }

    float textWidth (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }

    // direction: -1 points left, +1 points right, 0 points down
    void strokeChevron (juce::Graphics& g, juce::Point<float> centre, float size, int direction, float thickness)
    {
        juce::Path p;

        if (direction == 0)
        {
            p.startNewSubPath (centre.x - size, centre.y - size * 0.5f);
            p.lineTo (centre.x, centre.y + size * 0.5f);
            p.lineTo (centre.x + size, centre.y - size * 0.5f);
        }
        else
        {
            const float d = (float) direction;
            p.startNewSubPath (centre.x - d * size * 0.5f, centre.y - size);
            p.lineTo (centre.x + d * size * 0.5f, centre.y);
            p.lineTo (centre.x - d * size * 0.5f, centre.y + size);
        }

        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Rectangle<int> scaled (juce::Rectangle<float> r, float scale)
    {
        return r.transformedBy (juce::AffineTransform::scale (scale)).toNearestInt();
    }
}

//==============================================================================
AuroraLookAndFeel::AuroraLookAndFeel()
{
    using namespace aurora_colours;

    lightFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::JostLight_ttf,  (size_t) BinaryData::JostLight_ttfSize);
    bookFace   = juce::Typeface::createSystemTypefaceFor (BinaryData::JostBook_ttf,   (size_t) BinaryData::JostBook_ttfSize);
    mediumFace = juce::Typeface::createSystemTypefaceFor (BinaryData::JostMedium_ttf, (size_t) BinaryData::JostMedium_ttfSize);

    setColour (juce::Label::textColourId, text);
    setColour (juce::ResizableWindow::backgroundColourId, bgTop);

    setColour (juce::PopupMenu::backgroundColourId, menuBg);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, green);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, green.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, bright);
}

juce::Font AuroraLookAndFeel::font (Weight weight, float emSize, float tracking) const
{
    const auto& face = weight == Weight::light ? lightFace
                     : (weight == Weight::medium ? mediumFace : bookFace);
    return makeFont (face, emSize * scale, tracking);
}

juce::Font AuroraLookAndFeel::getPopupMenuFont()
{
    return makeFont (bookFace, 13.5f, 0.02f);   // menus are separate windows: not scaled with the editor
}

void AuroraLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    // three short diagonal lines in the corner
    const float fw = (float) w, fh = (float) h;
    g.setColour (aurora_colours::green.withAlpha (isMouseOver || isMouseDragging ? 0.8f : 0.35f));

    for (float inset : { 0.35f, 0.55f, 0.75f })
        g.drawLine (fw * inset, fh * 0.92f, fw * 0.92f, fh * inset, juce::jmax (1.0f, 1.1f * scale));
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
    g.setFont (font (Weight::book, big ? 17.0f : 9.4f, 0.01f));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                      ring.reduced ((big ? 8.0f : 4.0f) * scale).toNearestInt(),
                      juce::Justification::centred, 1, 0.8f);
}

void AuroraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool highlighted, bool down)
{
    using namespace aurora_colours;

    const auto area = button.getLocalBounds().toFloat().reduced (1.0f * scale);
    const float corner = area.getHeight() * 0.5f;
    const bool on = button.getToggleState();
    const bool dropdown = button.getProperties().contains ("dropdown");

    if (on)
    {
        g.setColour (green.withAlpha (0.25f));
        g.fillRoundedRectangle (area.expanded (1.0f * scale), corner);
        g.setColour (green.withAlpha (down ? 0.75f : 0.9f));
        g.fillRoundedRectangle (area, corner);
    }
    else
    {
        g.setColour (bgTop.withAlpha (dropdown ? 0.45f : 0.0f));
        g.fillRoundedRectangle (area, corner);
        g.setColour (green.withAlpha (highlighted || down ? 0.16f : 0.04f));
        g.fillRoundedRectangle (area, corner);
    }

    g.setColour (green.withAlpha (on ? 1.0f : (dropdown ? 0.5f : 0.7f)));
    g.drawRoundedRectangle (area, corner, 1.2f * scale);
}

void AuroraLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    using namespace aurora_colours;

    const bool dropdown = button.getProperties().contains ("dropdown");
    const float em = (float) button.getHeight() / juce::jmax (0.01f, scale) * (dropdown ? 0.40f : 0.36f);
    const float tracking = dropdown ? 0.16f : 0.26f;
    const auto f = font (Weight::medium, em, tracking);
    auto area = button.getLocalBounds().toFloat();

    g.setColour (button.getToggleState() ? bgTop : green);
    g.setFont (f);

    if (dropdown)
    {
        // text and a small arrow, centred together
        const float arrow = 3.2f * scale;
        const float gap = 7.0f * scale;
        const float w = textWidth (f, button.getButtonText());
        const float x0 = area.getCentreX() - (w + gap + arrow * 2.0f) * 0.5f;

        g.drawText (button.getButtonText(), juce::Rectangle<float> (x0, area.getY(), w + 4.0f * scale, area.getHeight()),
                    juce::Justification::centredLeft, false);
        strokeChevron (g, { x0 + w + gap + arrow, area.getCentreY() + 0.5f * scale }, arrow, 0, 1.3f * scale);
    }
    else
    {
        // the tracking also adds space after the last letter: shift by half of it to stay centred
        g.drawText (button.getButtonText(), area.translated (tracking * em * scale * 0.5f, 0.0f),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
PresetBar::PresetBar (AuroraLookAndFeel& lookAndFeelToUse) : lnf (lookAndFeelToUse)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setRepaintsOnMouseActivity (false);
}

void PresetBar::setPreset (int index, const juce::String& name, bool modified)
{
    number = juce::String (index + 1).paddedLeft ('0', 2);
    title  = name.toUpperCase() + (modified ? " *" : "");
    repaint();
}

juce::Rectangle<float> PresetBar::zoneBounds (Zone zone) const
{
    const auto area = getLocalBounds().toFloat();
    const float arrowWidth = juce::jmin (area.getWidth() * 0.25f, area.getHeight() * 1.25f);

    switch (zone)
    {
        case Zone::previous: return area.withWidth (arrowWidth);
        case Zone::next:     return area.withLeft (area.getRight() - arrowWidth);
        case Zone::name:     return area.withTrimmedLeft (arrowWidth).withTrimmedRight (arrowWidth);
        case Zone::none:     break;
    }

    return {};
}

PresetBar::Zone PresetBar::zoneAt (juce::Point<float> position) const
{
    if (! getLocalBounds().toFloat().contains (position))
        return Zone::none;

    for (auto zone : { Zone::previous, Zone::next, Zone::name })
        if (zoneBounds (zone).contains (position))
            return zone;

    return Zone::none;
}

void PresetBar::setHover (Zone zone)
{
    if (hover != zone)
    {
        hover = zone;
        repaint();
    }
}

void PresetBar::mouseMove (const juce::MouseEvent& e)   { setHover (zoneAt (e.position)); }

void PresetBar::mouseExit (const juce::MouseEvent&)     { setHover (Zone::none); }

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    pressed = zoneAt (e.position);
    hover = pressed;
    repaint();
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    const auto released = zoneAt (e.position);
    const bool clicked = released == pressed && pressed != Zone::none;
    const auto zone = pressed;

    pressed = Zone::none;
    hover = released;
    repaint();

    if (! clicked)
        return;

    if (zone == Zone::previous && onPrevious != nullptr)       onPrevious();
    else if (zone == Zone::next && onNext != nullptr)          onNext();
    else if (zone == Zone::name && onShowMenu != nullptr)      onShowMenu();
}

void PresetBar::paint (juce::Graphics& g)
{
    using namespace aurora_colours;

    const float s = lnf.getScale();
    const auto area = getLocalBounds().toFloat().reduced (1.0f * s);
    const float corner = area.getHeight() * 0.5f;

    juce::Path pill;
    pill.addRoundedRectangle (area, corner);

    g.setColour (bgTop.withAlpha (0.55f));
    g.fillPath (pill);

    if (hover != Zone::none)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (pill);
        g.setColour (green.withAlpha (pressed == hover ? 0.22f : 0.10f));
        g.fillRect (zoneBounds (hover));
    }

    g.setColour (green.withAlpha (0.55f));
    g.strokePath (pill, juce::PathStrokeType (1.2f * s));

    const auto previous = zoneBounds (Zone::previous);
    const auto next     = zoneBounds (Zone::next);
    const auto name     = zoneBounds (Zone::name);

    // separators
    g.setColour (green.withAlpha (0.22f));
    g.fillRect (juce::Rectangle<float> (previous.getRight(), area.getY() + 7.0f * s, 1.0f * s, area.getHeight() - 14.0f * s));
    g.fillRect (juce::Rectangle<float> (next.getX() - 1.0f * s, area.getY() + 7.0f * s, 1.0f * s, area.getHeight() - 14.0f * s));

    // arrows
    g.setColour (hover == Zone::previous ? bright : green);
    strokeChevron (g, previous.getCentre().translated (1.5f * s, 0.0f), 4.6f * s, -1, 1.6f * s);
    g.setColour (hover == Zone::next ? bright : green);
    strokeChevron (g, next.getCentre().translated (-1.5f * s, 0.0f), 4.6f * s, 1, 1.6f * s);

    // preset number and name, centred together, with a small menu arrow on the right
    const auto numberFont = lnf.font (Weight::medium, 10.0f, 0.10f);
    const auto titleFont  = lnf.font (Weight::book, 11.5f, 0.15f);
    const float arrow = 3.4f * s;
    const float gap = 9.0f * s;
    const float numberWidth = textWidth (numberFont, number);
    const float maxTitleWidth = name.getWidth() - numberWidth - gap * 2.0f - arrow * 2.0f - 16.0f * s;
    const float titleWidth = juce::jmin (textWidth (titleFont, title), maxTitleWidth);
    const float total = numberWidth + gap + titleWidth + gap + arrow * 2.0f;
    float x = name.getCentreX() - total * 0.5f;

    g.setFont (numberFont);
    g.setColour (green.withAlpha (0.9f));
    g.drawText (number, juce::Rectangle<float> (x, name.getY(), numberWidth + 2.0f * s, name.getHeight()),
                juce::Justification::centredLeft, false);
    x += numberWidth + gap;

    g.setFont (titleFont);
    g.setColour (bright);
    g.drawFittedText (title, juce::Rectangle<float> (x, name.getY(), titleWidth + 2.0f * s, name.getHeight()).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.7f);
    x += titleWidth + gap;

    g.setColour (hover == Zone::name ? bright : green.withAlpha (0.8f));
    strokeChevron (g, { x + arrow, name.getCentreY() + 0.5f * s }, arrow, 0, 1.3f * s);
}

//==============================================================================
DreamyReverbAudioProcessorEditor::DreamyReverbAudioProcessorEditor (DreamyReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // Read the saved size first: setting the resize limits below already resizes the
    // (still empty) editor to its minimum size.
    const int savedWidth = juce::jlimit (minWidth, maxWidth,
                                         (int) proc.apvts.state.getProperty ("editorWidth", baseWidth));

    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    // three hero knobs
    addKnob ("decay",    "DECAY",     180.0f, 212.0f, 56.0f);
    addKnob ("size",     "SIZE",      360.0f, 212.0f, 56.0f);
    addKnob ("shimmer",  "SHIMMER",   540.0f, 212.0f, 56.0f);

    // bottom row
    const char* ids[]    = { "mix", "predelay",  "highcut",  "lowcut",  "moddepth",  "modrate",  "width" };
    const char* titles[] = { "MIX", "PRE-DELAY", "HIGH CUT", "LOW CUT", "MOD DEPTH", "MOD RATE", "WIDTH" };
    for (int i = 0; i < 7; ++i)
        addKnob (ids[i], titles[i], 75.0f + 95.0f * (float) i, 392.0f, 25.0f);

    freezeButton.setClickingTogglesState (true);
    freezeButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "freeze", freezeButton);

    // preset bar: previous / name (opens the list) / next
    presetBar.onPrevious = [this] { stepPreset (-1); };
    presetBar.onNext     = [this] { stepPreset (1); };
    presetBar.onShowMenu = [this] { showPresetMenu(); };
    addAndMakeVisible (presetBar);
    refreshPresetBar (true);

    // shimmer interval selector below the Shimmer knob
    intervalButton.getProperties().set ("dropdown", true);
    intervalButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    intervalButton.onClick = [this] { showIntervalMenu(); };
    addAndMakeVisible (intervalButton);

    if (auto* param = proc.apvts.getParameter ("shimmode"))
    {
        intervalAttachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float value)
        {
            shownInterval = juce::jlimit (0, dreamy::numShimmerIntervals - 1, juce::roundToInt (value));
            intervalButton.setButtonText (juce::String (dreamy::shimmerIntervalNames[shownInterval]).toUpperCase());
        });
        intervalAttachment->sendInitialUpdate();
    }

    buildAuroraImage();

    // resizable with a fixed aspect ratio
    const double ratio = (double) baseWidth / (double) baseHeight;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (minWidth, juce::roundToInt (minWidth / ratio),
                     maxWidth, juce::roundToInt (maxWidth / ratio));

    setSize (savedWidth, juce::roundToInt (savedWidth / ratio));

    constructed = true;
    proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);

    startTimerHz (10);
}

DreamyReverbAudioProcessorEditor::~DreamyReverbAudioProcessorEditor()
{
    stopTimer();
    juce::PopupMenu::dismissAllActiveMenus();
    intervalAttachment.reset();
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

//==============================================================================
void DreamyReverbAudioProcessorEditor::timerCallback()
{
    // follows preset changes made by the host (session recall) and marks edited presets with a star
    refreshPresetBar (false);
}

void DreamyReverbAudioProcessorEditor::refreshPresetBar (bool force)
{
    const int index = proc.getCurrentPreset();
    const bool modified = proc.isPresetModified();

    if (force || index != shownPreset || modified != shownModified)
    {
        shownPreset = index;
        shownModified = modified;
        presetBar.setPreset (index, DreamyReverbAudioProcessor::getPresetName (index), modified);
    }
}

void DreamyReverbAudioProcessorEditor::stepPreset (int delta)
{
    const int count = DreamyReverbAudioProcessor::getNumPresets();
    proc.loadPreset ((proc.getCurrentPreset() + delta + count) % count);
    refreshPresetBar (true);
}

void DreamyReverbAudioProcessorEditor::showPresetMenu()
{
    const int count = DreamyReverbAudioProcessor::getNumPresets();
    const int current = proc.getCurrentPreset();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);

    // one sub menu per category
    for (int i = 0; i < count;)
    {
        const auto category = DreamyReverbAudioProcessor::getPresetCategory (i);
        juce::PopupMenu sub;
        bool containsCurrent = false;

        for (; i < count && DreamyReverbAudioProcessor::getPresetCategory (i) == category; ++i)
        {
            sub.addItem (i + 1,
                         juce::String (i + 1).paddedLeft ('0', 2) + "   " + DreamyReverbAudioProcessor::getPresetName (i),
                         true, i == current);
            containsCurrent = containsCurrent || i == current;
        }

        menu.addSubMenu (category, sub, true, juce::Image(), containsCurrent);
    }

    juce::Component::SafePointer<DreamyReverbAudioProcessorEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBar),
                        [safeThis] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                            {
                                safeThis->proc.loadPreset (result - 1);
                                safeThis->refreshPresetBar (true);
                            }
                        });
}

void DreamyReverbAudioProcessorEditor::showIntervalMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);

    for (int i = 0; i < dreamy::numShimmerIntervals; ++i)
        menu.addItem (i + 1, dreamy::shimmerIntervalNames[i], true, i == shownInterval);

    juce::Component::SafePointer<DreamyReverbAudioProcessorEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (intervalButton),
                        [safeThis] (int result)
                        {
                            if (safeThis != nullptr && result > 0 && safeThis->intervalAttachment != nullptr)
                                safeThis->intervalAttachment->setValueAsCompleteGesture ((float) (result - 1));
                        });
}

//==============================================================================
// The blurred aurora ribbons are rendered once at low resolution and stretched in paint().
void DreamyReverbAudioProcessorEditor::buildAuroraImage()
{
    using namespace aurora_colours;

    const float s = 1.0f / 6.0f;
    const int w = juce::roundToInt ((float) baseWidth * s);
    const int h = juce::roundToInt ((float) baseHeight * s);

    // the ribbons were drawn for a 720 x 430 canvas
    const auto toImage = juce::AffineTransform::scale (s, s * (float) baseHeight / 430.0f);

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
        top.applyTransform (toImage);
        g.setGradientFill (grad);
        g.strokePath (top, juce::PathStrokeType (70.0f * s));

        juce::Path bottom;
        bottom.startNewSubPath (-20.0f, 300.0f);
        bottom.cubicTo (200.0f, 220.0f, 330.0f, 360.0f, 520.0f, 280.0f);
        bottom.cubicTo (710.0f, 200.0f, 700.0f, 250.0f, 760.0f, 300.0f);
        bottom.applyTransform (toImage);
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

    // wordmark:  D.A | DREAMY SHIMMER REVERB
    const auto brandFont = lookAndFeel.font (Weight::medium, 15.0f, 0.30f);
    const auto nameFont  = lookAndFeel.font (Weight::light,  15.0f, 0.34f);
    const float top = 21.0f * scale, height = 30.0f * scale;
    float x = 30.0f * scale;

    const float brandWidth = textWidth (brandFont, "D.A");
    g.setColour (green);
    g.setFont (brandFont);
    g.drawText ("D.A", juce::Rectangle<float> (x, top, brandWidth + 4.0f * scale, height),
                juce::Justification::centredLeft, false);
    x += brandWidth + 5.0f * scale;

    g.setColour (text.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (x, top + 8.5f * scale, juce::jmax (1.0f, scale), height - 17.0f * scale));
    x += 10.5f * scale;

    g.setColour (text);
    g.setFont (nameFont);
    g.drawText ("DREAMY SHIMMER REVERB", juce::Rectangle<float> (x, top, area.getWidth() - x, height),
                juce::Justification::centredLeft, false);
}

void DreamyReverbAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    lookAndFeel.setScale (scale);

    freezeButton.setBounds (scaled ({ 590.0f, 22.0f, 100.0f, 28.0f }, scale));
    presetBar.setBounds (scaled ({ 190.0f, 68.0f, 340.0f, 32.0f }, scale));
    intervalButton.setBounds (scaled ({ 478.0f, 303.0f, 124.0f, 22.0f }, scale));

    for (auto* k : knobs)
    {
        const bool big = k->radius > 40.0f;
        const float half = k->radius + 8.0f;

        k->slider.setBounds (scaled ({ k->cx - half, k->cy - half, half * 2.0f, half * 2.0f }, scale));

        const float labelW = big ? 170.0f : 95.0f;
        const float labelH = big ? 22.0f : 18.0f;
        const float tracking = big ? 0.24f : 0.18f;
        const float em = big ? 11.5f : 9.2f;

        k->label.setFont (lookAndFeel.font (Weight::medium, em, tracking));
        // the tracking also adds space after the last letter: shift by half of it to stay centred
        k->label.setBounds (scaled ({ k->cx - labelW * 0.5f + tracking * em * 0.5f, k->cy + k->radius + 7.0f, labelW, labelH }, scale));
    }

    // remember the size for the next time the editor is opened / the session is reloaded
    if (constructed)
        proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
}
