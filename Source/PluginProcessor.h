#pragma once

#include <JuceHeader.h>
#include "AuraDSP.h"

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

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

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
