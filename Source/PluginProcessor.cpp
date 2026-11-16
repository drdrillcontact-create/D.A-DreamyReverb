#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    // The preset parameter is a choice: the fifty preset names, and "" for "the knobs say
    // something of their own" (a preset the user has moved away from).
    juce::StringArray presetChoices()
    {
        juce::StringArray choices;
        choices.add ("");

        for (int i = 0; i < aura::numPresets; ++i)
            choices.add (aura::presets[i].name);

        return choices;
    }

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

juce::AudioProcessorValueTreeState::ParameterLayout DAAuraAudioProcessor::createParameterLayout()
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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { DAAuraAudioProcessor::presetParameterId, 1 }, "Preset",
        presetChoices(), 0));   // (0 = "", nothing of the factory list)

    return layout;
}

DAAuraAudioProcessor::DAAuraAudioProcessor()
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

void DAAuraAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate);
    scratch.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
}

void DAAuraAudioProcessor::reset()
{
    reverb.reset();
}

bool DAAuraAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DAAuraAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numInputs   = getTotalNumInputChannels();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    aura::Reverb::Params p;
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

    // what the window draws moves with the signal (see AuraDSP.h, trackLife())
    reverb.trackLife (buffer.getReadPointer (0),
                      numChannels > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0),
                      numSamples);
}

//==============================================================================
// Presets
//
// The preset parameter keeps the name of the preset that is on. Setting a parameter from
// applyPreset() does not change it by itself; the presets are applied through
// setValueNotifyingHost(), so the host (and the window) see the whole move in one go.
namespace
{
    constexpr const char* parameterIds[] =
    {
        "mix", "size", "decay", "predelay", "highcut", "lowcut",
        "moddepth", "modrate", "shimmer", "width", "freeze"
    };

    constexpr int numParameterIds = (int) (sizeof (parameterIds) / sizeof (parameterIds[0]));

    juce::String nameOfPreset (int index)
    {
        return juce::isPositiveAndBelow (index, aura::numPresets) ? juce::String (aura::presets[index].name)
                                                                  : juce::String();
    }
}

aura::PresetValues DAAuraAudioProcessor::valuesOfPreset (int index) const
{
    if (juce::isPositiveAndBelow (index, aura::numPresets))
        return aura::presets[index].values;

    return {};
}

int DAAuraAudioProcessor::currentPreset() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (presetParameterId)))
    {
        const auto& name = choice->getCurrentChoiceName();
        return name.isEmpty() ? -1 : aura::findPreset (name.toRawUTF8());
    }

    return -1;
}

int DAAuraAudioProcessor::currentSpace() const
{
    const int index = currentPreset();

    if (index < 0)
        return -1;

    for (int s = 0; s < aura::numSpaces; ++s)
        if (index >= aura::spaces[s].first && index < aura::spaces[s].first + aura::spaces[s].count)
            return s;

    return -1;
}

const juce::String DAAuraAudioProcessor::currentPresetName() const
{
    const int index = currentPreset();
    return index < 0 ? juce::String ("Custom") : nameOfPreset (index);
}

juce::String DAAuraAudioProcessor::currentSpaceName() const
{
    const int space = currentSpace();
    return space < 0 ? juce::String ("--") : juce::String (aura::spaces[space].name);
}

void DAAuraAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, aura::numPresets))
        return;

    const auto& values = aura::presets[index].values;

    const auto setReal = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    setReal ("mix",      values.mix);
    setReal ("size",     values.size);
    setReal ("decay",    values.decay);
    setReal ("predelay", values.preDelay);
    setReal ("highcut",  values.highCut);
    setReal ("lowcut",   values.lowCut);
    setReal ("moddepth", values.modDepth);
    setReal ("modrate",  values.modRate);
    setReal ("shimmer",  values.shimmer);
    setReal ("width",    values.width);

    if (auto* p = apvts.getParameter ("freeze"))
        p->setValueNotifyingHost (values.freeze ? 1.0f : 0.0f);

    if (auto* p = apvts.getParameter (presetParameterId))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (index + 1)));   // (choice 0 is "")
}

void DAAuraAudioProcessor::stepPreset (int direction)
{
    const int current = currentPreset();

    if (current < 0)
    {
        // (the knobs were moved: the arrows start again from the first preset of the space
        //  the last one was in, or from the first preset of all)
        applyPreset (0);
        return;
    }

    applyPreset (aura::nextPresetInSpace (current, direction < 0 ? -1 : 1));
}

int DAAuraAudioProcessor::getCurrentProgram()
{
    const int index = currentPreset();
    return index < 0 ? 0 : index;
}

void DAAuraAudioProcessor::setCurrentProgram (int index)
{
    applyPreset (juce::jlimit (0, aura::numPresets - 1, index));
}

const juce::String DAAuraAudioProcessor::getProgramName (int index)
{
    return nameOfPreset (index);
}

juce::AudioProcessorEditor* DAAuraAudioProcessor::createEditor()
{
    return new DAAuraAudioProcessorEditor (*this);
}

void DAAuraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DAAuraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DAAuraAudioProcessor();
}
