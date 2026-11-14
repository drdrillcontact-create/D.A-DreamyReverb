#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// "Aurora" look: thin glowing rings with the value shown in the centre,
// set in the Jost typeface (embedded, SIL Open Font License).
class AuroraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class Weight { light, book, medium };

    AuroraLookAndFeel();

    void setScale (float newScale) noexcept          { scale = newScale; }
    float getScale() const noexcept                  { return scale; }

    // size and tracking are given in em units (like CSS font-size / letter-spacing), unscaled
    juce::Font font (Weight weight, float emSize, float tracking = 0.0f) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getPopupMenuFont() override;

private:
    juce::Typeface::Ptr lightFace, bookFace, mediumFace;
    float scale = 1.0f;
};

//==============================================================================
// Preset selector:  [ < ]  [ 01  PRESET NAME  v ]  [ > ]
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (AuroraLookAndFeel&);

    void setPreset (int index, const juce::String& name, bool modified);

    std::function<void()> onPrevious, onNext, onShowMenu;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Zone { none, previous, name, next };

    Zone zoneAt (juce::Point<float>) const;
    juce::Rectangle<float> zoneBounds (Zone) const;
    void setHover (Zone);

    AuroraLookAndFeel& lnf;
    juce::String number, title;
    Zone hover = Zone::none, pressed = Zone::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

//==============================================================================
class DreamyReverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit DreamyReverbAudioProcessorEditor (DreamyReverbAudioProcessor&);
    ~DreamyReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth  = 720;
    static constexpr int baseHeight = 470;
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
        float cx = 0.0f, cy = 0.0f, radius = 25.0f;   // in base (720 x 470) coordinates
    };

    void timerCallback() override;

    void addKnob (const juce::String& paramId, const juce::String& title, float cx, float cy, float radius);
    void buildAuroraImage();

    void stepPreset (int delta);
    void showPresetMenu();
    void showIntervalMenu();
    void refreshPresetBar (bool force);

    DreamyReverbAudioProcessor& proc;
    AuroraLookAndFeel lookAndFeel;
    juce::Image aurora;

    juce::OwnedArray<Knob> knobs;
    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    PresetBar presetBar { lookAndFeel };
    int  shownPreset = -1;
    bool shownModified = false;

    juce::TextButton intervalButton;
    std::unique_ptr<juce::ParameterAttachment> intervalAttachment;
    int shownInterval = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamyReverbAudioProcessorEditor)
};
