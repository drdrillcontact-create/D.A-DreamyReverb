#pragma once

#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"

namespace aurora_colours
{
    extern const juce::Colour bgTop;
    extern const juce::Colour bgBottom;
    extern const juce::Colour green;
    extern const juce::Colour blue;
    extern const juce::Colour purple;
    extern const juce::Colour text;
    extern const juce::Colour bright;
}

//==============================================================================
// "Aurora" look: thin glowing rings with the value shown in the centre.
class AuroraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuroraLookAndFeel();

    void setScale (float newScale) noexcept          { scale = newScale; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    float scale = 1.0f;
};

//==============================================================================
// The aurora behind the knobs: the design's own light - a wide green ridge sweeping up from
// the left of the middle to the top right, a blue-violet one following above it, a low teal
// wash and a purple one on the right - which, while the plug-in carries a signal, opens out,
// travels, and lights a few stars with the spectrum. At rest it is exactly the design.
//
// It is drawn as a handful of soft bands, each a filled path with a vertical gradient, so that
// what the screen shows is the same on every platform (a small pre-blurred image, stretched,
// is what the design's own haze looked like on Linux but not on Windows).
class AuroraField : public juce::Component
{
public:
    AuroraField();

    /** Once per refresh (30 times a second): takes the signal's own numbers and moves the field. */
    void moveTo (const aura::Reverb::Life& life, double secondsSinceLastMove);

    void paint (juce::Graphics&) override;

    void setFieldScale (float newScale) noexcept   { fieldScale = newScale; }

private:
    /** One band of light: where its centre line runs, how wide and how bright it is. */
    struct Band
    {
        juce::Point<float> from { 0.0f, 0.5f }, to { 1.0f, 0.1f };  // the centre line, 0..1
        float bow = 0.0f;        // how far the line bows away from straight, 0..1 of the height
        float width = 0.10f;     // its thickness, 0..1 of the height
        float strength = 0.5f;   // how bright it is
        float reach = 1.0f;      // how far along the width its light reaches (0..1 = to the end)
        float fadeIn = 0.12f;    // how quickly it comes in from its start
        juce::Colour colour;
        float wave = 0.0f;       // how far the signal makes it undulate, 0..1 of the height
        float phase = 0.0f;      // where its own wave is
        float speed = 1.0f;      // how fast that wave travels
        float lift = 0.0f;       // how much a signal lifts it towards the middle
    };

    /** A point of light in the dark top of the window, which the spectrum makes twinkle. */
    struct Star
    {
        float x = 0.0f, y = 0.0f;
        float size = 1.0f, alpha = 0.0f, twinkle = 0.0f;
    };

    juce::ColourGradient backdrop() const;

    /** The centre line of a band at `t` along the width (0..1), as a fraction of the height. */
    float centreAt (const Band&, float t, float awakeness) const;

    /** Draws one band as a soft, filled shape. */
    void drawBand (juce::Graphics&, const Band&, float awakeness, float w, float h) const;

    float fieldScale = 1.0f;
    float level = 0.0f, transient = 0.0f, delayLevel = 0.0f;
    float band[aura::Reverb::Life::numBands] {};
    float time = 0.0f;

    std::array<Band, 7> bands;
    std::array<Star, 80> stars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuroraField)
};

//==============================================================================
class DAAuraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  public juce::Timer
{
public:
    explicit DAAuraAudioProcessorEditor (DAAuraAudioProcessor&);
    ~DAAuraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

    /** The list of every preset, by space, with the one that is on ticked.
        (Made without showing it, which is what the tests read: every preset in it, once.) */
    static juce::PopupMenu buildPresetMenu (int current);

    static constexpr int baseWidth  = 720;
    static constexpr int baseHeight = 430;
    static constexpr int minWidth   = 480;
    static constexpr int maxWidth   = 1800;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
        float cx = 0.0f, cy = 0.0f, radius = 25.0f;   // in base (720 x 430) coordinates
    };

    void addKnob (const juce::String& paramId, const juce::String& title, float cx, float cy, float radius);
    void refreshPresetLabels();

    /** The list of every preset, by space, with the one that is on ticked: opens under the name. */
    void showPresetMenu();

    DAAuraAudioProcessor& proc;
    AuroraLookAndFeel lookAndFeel;
    AuroraField field;

    // the product's name, drawn over the aurora (a label, so that it sits in front of the field)
    juce::Label  title;

    // the preset browser: the space, the preset's name (which opens the list when clicked),
    // how far through the space it is, and the arrows
    juce::Label  presetSpace;
    juce::TextButton presetName;      // (the name, over the aurora: the whole of it opens the list)
    juce::Label  presetCount;
    juce::TextButton previousPreset;   // (the arrows are drawn in AuroraLookAndFeel, not letters)
    juce::TextButton nextPreset;
    bool previousHovered = false, nextHovered = false;

    int shownPreset = -2;    // (what the labels last showed, so they are only touched when it changes)
    bool isSettingUp = true; // (the window is taking its first size: that one is not written back)

    juce::OwnedArray<Knob> knobs;
    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DAAuraAudioProcessorEditor)
};
