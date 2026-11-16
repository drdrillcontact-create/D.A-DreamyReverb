#pragma once

#include <JuceHeader.h>
#include "AuraDSP.h"
#include "Presets.h"

class DAAuraAudioProcessor : public juce::AudioProcessor
{
public:
    DAAuraAudioProcessor();
    ~DAAuraAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 30.0; }

    // The host's own program list is the factory presets, in the browser's order.
    int getNumPrograms() override                          { return aura::numPresets; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // ---- presets -------------------------------------------------------------------------
    // The preset parameter holds the preset's name ("" = the knobs have been moved since).
    // applyPreset() sets every knob; the editor's arrows call stepPreset().
    static constexpr const char* presetParameterId = "preset";

    void applyPreset (int index);
    void stepPreset (int direction);          // -1 = the previous preset of this space, +1 = the next
    int  currentPreset() const;               // -1 when the knobs say something of their own
    int  currentSpace() const;                // -1 alongside it
    aura::PresetValues valuesOfPreset (int index) const;

    /** What the window draws: the signal's own numbers (see AuraDSP.h, trackLife()). */
    const aura::Reverb::Life& reverbLife() const noexcept   { return reverb.life(); }

    /** The preset that is on, and what it is called, for the window's own reading. */
    const juce::String currentPresetName() const;
    juce::String currentSpaceName() const;

private:
    aura::Reverb reverb;
    juce::AudioBuffer<float> scratch;

    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pPreDelay = nullptr;
    std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pModDepth = nullptr;
    std::atomic<float>* pModRate = nullptr;
    std::atomic<float>* pShimmer = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pFreeze = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DAAuraAudioProcessor)
};
