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
}

void DreamyReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate);
    scratch.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
}

void DreamyReverbAudioProcessor::reset()
{
    reverb.reset();
}

bool DreamyReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
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
        // Fallback for hosts that hand us a single channel
        if (scratch.getNumSamples() < numSamples)
            scratch.setSize (2, numSamples, false, true, true);

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

void DreamyReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DreamyReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DreamyReverbAudioProcessor();
}
