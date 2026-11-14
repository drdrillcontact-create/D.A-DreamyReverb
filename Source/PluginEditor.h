#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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
class DreamyReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DreamyReverbAudioProcessorEditor (DreamyReverbAudioProcessor&);
    ~DreamyReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

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
    void buildAuroraImage();

    DreamyReverbAudioProcessor& proc;
    AuroraLookAndFeel lookAndFeel;
    juce::Image aurora;

    juce::OwnedArray<Knob> knobs;
    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamyReverbAudioProcessorEditor)
};
