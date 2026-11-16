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
// The aurora behind the knobs: a field of ribbons and stars that is at rest until the plug-in
// carries a signal, and then moves with it - the ribbons swell with the wet level, the stars
// drift with the spectrum, and each onset sends a ring of light outward from the middle.
class AuroraField : public juce::Component
{
public:
    AuroraField();

    /** Once per refresh (30 times a second): takes the signal's own numbers and moves the field. */
    void moveTo (const aura::Reverb::Life& life, double secondsSinceLastMove);

    void paint (juce::Graphics&) override;

    void setFieldScale (float newScale) noexcept   { fieldScale = newScale; }

private:
    struct Ribbon
    {
        float phase = 0.0f;       // where the ribbon's own wave is
        float speed = 0.0f;       // how fast it travels at rest
        float height = 0.0f;      // its line through the middle, 0..1 of the height
        float thickness = 1.0f;
        float amp = 0.0f;         // how far it wanders
        juce::Colour colour;
        float alpha = 0.0f;
    };

    struct Star
    {
        float x = 0.0f, y = 0.0f;     // 0..1 of the width and height
        float size = 1.0f, alpha = 0.0f, twinkle = 0.0f, drift = 0.0f;
    };

    struct Ring
    {
        float radius = 0.0f;          // 0..1 of the width
        float alpha = 0.0f;
        float width = 1.0f;
    };

    juce::ColourGradient backdrop();

    float fieldScale = 1.0f;
    float level = 0.0f, transient = 0.0f, delayLevel = 0.0f;
    float band[aura::Reverb::Life::numBands] {};
    float time = 0.0f;

    std::array<Ribbon, 7> ribbons;
    std::array<Star, 96> stars;
    std::array<Ring, 4> rings;
    int nextRing = 0;

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

    DAAuraAudioProcessor& proc;
    AuroraLookAndFeel lookAndFeel;
    AuroraField field;

    // the preset browser: the space, the preset's name, and the arrows that step through it
    juce::Label  presetSpace;
    juce::Label  presetName;
    juce::Label  presetCount;
    juce::TextButton previousPreset;   // (the arrows are drawn in AuroraLookAndFeel, not letters)
    juce::TextButton nextPreset;
    bool previousHovered = false, nextHovered = false;

    int shownPreset = -2;    // (what the labels last showed, so they are only touched when it changes)

    juce::OwnedArray<Knob> knobs;
    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DAAuraAudioProcessorEditor)
};
