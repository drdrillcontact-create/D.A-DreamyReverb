#pragma once

#include <JuceHeader.h>
#include "DreamyReverbDSP.h"
#include "Presets.h"

class DreamyReverbAudioProcessor : public juce::AudioProcessor
{
public:
    DreamyReverbAudioProcessor();
    ~DreamyReverbAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override;

    // The factory presets are handled by the plugin itself (preset bar in the editor),
    // so the host only ever sees a single program.
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // Factory presets
    static int getNumPresets() noexcept                    { return dreamy::numPresets; }
    static juce::String getPresetName (int index);
    static juce::String getPresetCategory (int index);

    int  getCurrentPreset() const noexcept                 { return currentPreset.load(); }
    void loadPreset (int index);          // call on the message thread
    bool isPresetModified() const;        // true when a knob no longer matches the current preset

    juce::AudioProcessorValueTreeState apvts;

private:
    void applyPresetValue (const char* paramId, float value);
    bool parameterMatches (const char* paramId, float value) const;

    dreamy::Reverb reverb;
    juce::AudioBuffer<float> scratch;
    std::atomic<int> currentPreset { 0 };

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
    std::atomic<float>* pInterval = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamyReverbAudioProcessor)
};
