#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    std::unique_ptr<FloatParam> percentParam (const char* id, const char* name, float maxValue, float def)
    {
        return std::make_unique<FloatParam> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (0.0f, maxValue, 0.001f), def,
            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                  .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; }));
    }

    std::unique_ptr<FloatParam> unitParam (const char* id, const char* name,
                                           float lo, float hi, float skewCentre, float def,
                                           const char* unit, int decimals)
    {
        juce::NormalisableRange<float> range (lo, hi);
        range.setSkewForCentre (skewCentre);
        const juce::String u (unit);

        return std::make_unique<FloatParam> (
            juce::ParameterID { id, 1 }, name, range, def,
            Attr().withStringFromValueFunction ([u, decimals] (float v, int)
                                                {
                                                    return (decimals > 0 ? juce::String (v, decimals)
                                                                         : juce::String (juce::roundToInt (v))) + " " + u;
                                                })
                  .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); }));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout DreamyReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (percentParam ("mix",      "Mix",       1.0f, 0.40f));
    layout.add (percentParam ("size",     "Size",      1.0f, 0.65f));
    layout.add (unitParam    ("decay",    "Decay",     0.3f, 30.0f, 5.0f, 7.0f, "s", 1));
    layout.add (unitParam    ("predelay", "Pre-Delay", 0.0f, 500.0f, 80.0f, 25.0f, "ms", 0));
    layout.add (unitParam    ("highcut",  "High Cut",  1000.0f, 20000.0f, 5000.0f, 6500.0f, "Hz", 0));
    layout.add (unitParam    ("lowcut",   "Low Cut",   20.0f, 1000.0f, 150.0f, 120.0f, "Hz", 0));
    layout.add (percentParam ("moddepth", "Mod Depth", 1.0f, 0.45f));
    layout.add (unitParam    ("modrate",  "Mod Rate",  0.05f, 3.0f, 0.6f, 0.45f, "Hz", 2));
    layout.add (percentParam ("shimmer",  "Shimmer",   1.0f, 0.30f));
    layout.add (percentParam ("width",    "Width",     1.5f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));

    // added in version 1.1 (higher version hint, so the AU parameter order of 1.0 stays valid)
    juce::StringArray intervals;
    for (auto* name : dreamy::shimmerIntervalNames)
        intervals.add (name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "shimmode", 2 }, "Shimmer Interval", intervals, 0));

    return layout;
}

DreamyReverbAudioProcessor::DreamyReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pMix      = apvts.getRawParameterValue ("mix");
    pSize     = apvts.getRawParameterValue ("size");
    pDecay    = apvts.getRawParameterValue ("decay");
    pPreDelay = apvts.getRawParameterValue ("predelay");
    pHighCut  = apvts.getRawParameterValue ("highcut");
    pLowCut   = apvts.getRawParameterValue ("lowcut");
    pModDepth = apvts.getRawParameterValue ("moddepth");
    pModRate  = apvts.getRawParameterValue ("modrate");
    pShimmer  = apvts.getRawParameterValue ("shimmer");
    pWidth    = apvts.getRawParameterValue ("width");
    pFreeze   = apvts.getRawParameterValue ("freeze");
    pInterval = apvts.getRawParameterValue ("shimmode");
}

void DreamyReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate);
    scratch.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
}

double DreamyReverbAudioProcessor::getTailLengthSeconds() const
{
    // decay time plus room for the pre-delay and the shimmer bloom
    return (double) pDecay->load() * 1.5 + 1.0;
}

void DreamyReverbAudioProcessor::reset()
{
    reverb.reset();
}

bool DreamyReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // stereo -> stereo, mono -> stereo and mono -> mono
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();

    return out == juce::AudioChannelSet::mono() && in == juce::AudioChannelSet::mono();
}

void DreamyReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numInputs   = getTotalNumInputChannels();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    dreamy::Reverb::Params p;
    p.mix      = pMix->load();
    p.size     = pSize->load();
    p.decay    = pDecay->load();
    p.preDelay = pPreDelay->load();
    p.highCut  = pHighCut->load();
    p.lowCut   = pLowCut->load();
    p.modDepth = pModDepth->load();
    p.modRate  = pModRate->load();
    p.shimmer  = pShimmer->load();
    p.width    = pWidth->load();
    p.freeze   = pFreeze->load() > 0.5f;
    p.shimmerMode = juce::roundToInt (pInterval->load());
    reverb.setParams (p);

    if (numChannels >= 2)
    {
        // mono input -> stereo output: duplicate the input to the right channel
        if (numInputs < 2)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        reverb.process (l, r, l, r, numSamples);

        for (int ch = 2; ch < numChannels; ++ch)
            buffer.clear (ch, 0, numSamples);
    }
    else
    {
        // mono -> mono: run the stereo reverb and fold the result down
        if (scratch.getNumSamples() < numSamples)
            scratch.setSize (2, numSamples, false, false, true);

        auto* m  = buffer.getWritePointer (0);
        auto* sl = scratch.getWritePointer (0);
        auto* sr = scratch.getWritePointer (1);
        reverb.process (m, m, sl, sr, numSamples);

        for (int i = 0; i < numSamples; ++i)
            m[i] = 0.5f * (sl[i] + sr[i]);
    }
}

juce::AudioProcessorEditor* DreamyReverbAudioProcessor::createEditor()
{
    return new DreamyReverbAudioProcessorEditor (*this);
}

//==============================================================================
juce::String DreamyReverbAudioProcessor::getPresetName (int index)
{
    return juce::isPositiveAndBelow (index, dreamy::numPresets) ? juce::String (dreamy::factoryPresets[index].name)
                                                                : juce::String();
}

juce::String DreamyReverbAudioProcessor::getPresetCategory (int index)
{
    return juce::isPositiveAndBelow (index, dreamy::numPresets) ? juce::String (dreamy::factoryPresets[index].category)
                                                                : juce::String();
}

void DreamyReverbAudioProcessor::applyPresetValue (const char* paramId, float value)
{
    if (auto* param = apvts.getParameter (paramId))
    {
        const float normalised = param->convertTo0to1 (value);

        if (std::abs (param->getValue() - normalised) > 1.0e-6f)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (normalised);
            param->endChangeGesture();
        }
    }
}

bool DreamyReverbAudioProcessor::parameterMatches (const char* paramId, float value) const
{
    if (auto* param = apvts.getParameter (paramId))
        return std::abs (param->getValue() - param->convertTo0to1 (value)) < 0.002f;

    return true;
}

void DreamyReverbAudioProcessor::loadPreset (int index)
{
    index = juce::jlimit (0, dreamy::numPresets - 1, index);
    const auto& preset = dreamy::factoryPresets[index];

    applyPresetValue ("mix",      preset.mix);
    applyPresetValue ("size",     preset.size);
    applyPresetValue ("decay",    preset.decay);
    applyPresetValue ("predelay", preset.preDelay);
    applyPresetValue ("highcut",  preset.highCut);
    applyPresetValue ("lowcut",   preset.lowCut);
    applyPresetValue ("moddepth", preset.modDepth);
    applyPresetValue ("modrate",  preset.modRate);
    applyPresetValue ("shimmer",  preset.shimmer);
    applyPresetValue ("width",    preset.width);
    applyPresetValue ("shimmode", (float) preset.interval);
    applyPresetValue ("freeze",   0.0f);

    currentPreset.store (index);
    apvts.state.setProperty ("presetIndex", index, nullptr);
}

bool DreamyReverbAudioProcessor::isPresetModified() const
{
    const auto& preset = dreamy::factoryPresets[juce::jlimit (0, dreamy::numPresets - 1, currentPreset.load())];

    return ! (parameterMatches ("mix",      preset.mix)
           && parameterMatches ("size",     preset.size)
           && parameterMatches ("decay",    preset.decay)
           && parameterMatches ("predelay", preset.preDelay)
           && parameterMatches ("highcut",  preset.highCut)
           && parameterMatches ("lowcut",   preset.lowCut)
           && parameterMatches ("moddepth", preset.modDepth)
           && parameterMatches ("modrate",  preset.modRate)
           && parameterMatches ("shimmer",  preset.shimmer)
           && parameterMatches ("width",    preset.width)
           && parameterMatches ("shimmode", (float) preset.interval));
}

//==============================================================================
void DreamyReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPreset.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DreamyReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset.store (juce::jlimit (0, dreamy::numPresets - 1, (int) state.getProperty ("presetIndex", 0)));
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DreamyReverbAudioProcessor();
}
