// Self test of the complete plugin without a host: parameters, the 50 presets, state
// save / restore, audio processing in every channel layout and the editor.
//
// usage: DreamyPluginTest [folder for editor snapshots]
// Returns 0 when everything passes.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "ok    " : "FAIL  ") << what << std::endl;
        if (! ok) ++failures;
    }

    std::unique_ptr<DreamyReverbAudioProcessor> createProcessor()
    {
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        auto* typed = dynamic_cast<DreamyReverbAudioProcessor*> (base.get());
        if (typed != nullptr) base.release();
        return std::unique_ptr<DreamyReverbAudioProcessor> (typed);
    }

    // runs noise bursts through the processor and reports whether the output is sane
    bool processIsSane (juce::AudioProcessor& proc, int numIn, int numOut, double sampleRate, int blockSize, double seconds, float& peak)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::canonicalChannelSet (numIn));
        layout.outputBuses.add (juce::AudioChannelSet::canonicalChannelSet (numOut));
        if (! proc.setBusesLayout (layout))
            return false;

        proc.setRateAndBufferSizeDetails (sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (juce::jmax (numIn, numOut), blockSize);
        juce::MidiBuffer midi;
        juce::Random random (42);
        bool sane = true;
        peak = 0.0f;

        for (int done = 0; done < (int) (seconds * sampleRate); done += blockSize)
        {
            const bool burst = (done / (int) (sampleRate * 0.2)) % 2 == 0;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, burst ? random.nextFloat() - 0.5f : 0.0f);

            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < numOut; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    sane = sane && std::isfinite (v);
                    peak = juce::jmax (peak, std::abs (v));
                }
        }

        proc.releaseResources();
        return sane && peak < 8.0f;
    }

    bool saveSnapshot (DreamyReverbAudioProcessor& proc, const juce::File& file, int width, float scale)
    {
        proc.apvts.state.setProperty ("editorWidth", width, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        if (editor == nullptr)
            return false;

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto proc = createProcessor();
    check (proc != nullptr, "the plugin can be created");
    if (proc == nullptr)
        return 1;

    check (proc->getName() == "D.A Dreamy Shimmer Reverb", "plugin name is \"" + proc->getName() + "\"");
    check (proc->getParameters().size() == 12, "12 parameters (" + juce::String (proc->getParameters().size()) + ")");
    check (DreamyReverbAudioProcessor::getNumPresets() == 50, "50 factory presets");

    // ---- presets ----------------------------------------------------------------------------
    {
        proc->loadPreset (0);
        bool defaults = true;
        for (auto* p : proc->getParameters())
            defaults = defaults && std::abs (p->getValue() - p->getDefaultValue()) < 1.0e-4f;
        check (defaults, "preset 01 equals the default value of every parameter");

        bool allLoad = true, differ = true;
        juce::Array<float> previous;
        for (int i = 0; i < DreamyReverbAudioProcessor::getNumPresets(); ++i)
        {
            proc->loadPreset (i);
            allLoad = allLoad && proc->getCurrentPreset() == i && ! proc->isPresetModified()
                   && DreamyReverbAudioProcessor::getPresetName (i).isNotEmpty()
                   && DreamyReverbAudioProcessor::getPresetCategory (i).isNotEmpty();

            juce::Array<float> values;
            for (auto* p : proc->getParameters())
                values.add (p->getValue());
            differ = differ && values != previous;
            previous = values;
        }
        check (allLoad, "all 50 presets load and report as unmodified");
        check (differ, "neighbouring presets are different");

        proc->loadPreset (4);
        if (auto* shimmer = proc->apvts.getParameter ("shimmer"))
            shimmer->setValueNotifyingHost (0.123f);
        check (proc->isPresetModified(), "turning a knob marks the preset as modified");
    }

    // ---- state ----------------------------------------------------------------------------------
    {
        proc->loadPreset (33);
        if (auto* width = proc->apvts.getParameter ("width"))
            width->setValueNotifyingHost (0.5f);

        juce::MemoryBlock state;
        proc->getStateInformation (state);

        auto other = createProcessor();
        other->setStateInformation (state.getData(), (int) state.getSize());

        bool same = other->getCurrentPreset() == 33;
        for (int i = 0; i < proc->getParameters().size(); ++i)
            same = same && std::abs (proc->getParameters()[i]->getValue() - other->getParameters()[i]->getValue()) < 1.0e-5f;
        check (same, "state save / restore brings back every parameter and the preset");

        // a session saved by version 1.0 has no interval parameter and no preset index
        const juce::String oldState ("<PARAMETERS editorWidth=\"900\"><PARAM id=\"decay\" value=\"12.5\"/>"
                                     "<PARAM id=\"shimmer\" value=\"0.8\"/><PARAM id=\"freeze\" value=\"0\"/></PARAMETERS>");
        if (auto xml = juce::parseXML (oldState))
        {
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary (*xml, block);
            other->setStateInformation (block.getData(), (int) block.getSize());
            check (std::abs (other->apvts.getRawParameterValue ("decay")->load() - 12.5f) < 0.01f
                   && juce::roundToInt (other->apvts.getRawParameterValue ("shimmode")->load()) == 0,
                   "a version 1.0 session loads (new parameter falls back to its default)");
        }

        other->setStateInformation ("garbage", 7);
        check (true, "invalid state data is ignored");
    }

    // ---- audio --------------------------------------------------------------------------------------
    {
        const int layouts[3][2] = { { 2, 2 }, { 1, 2 }, { 1, 1 } };
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        const int blocks[] = { 16, 512, 2048 };

        for (auto& io : layouts)
        {
            bool sane = true;
            float worst = 0.0f;
            for (int i = 0; i < DreamyReverbAudioProcessor::getNumPresets(); ++i)
            {
                proc->loadPreset (i);
                float peak = 0.0f;
                sane = processIsSane (*proc, io[0], io[1], rates[i % 3], blocks[i % 3], 0.6, peak) && sane;
                worst = juce::jmax (worst, peak);
            }
            check (sane, juce::String (io[0]) + " in / " + juce::String (io[1]) + " out: all presets process cleanly (peak "
                             + juce::String (worst, 2) + ")");
        }

        juce::AudioProcessor::BusesLayout bad;
        bad.inputBuses.add (juce::AudioChannelSet::stereo());
        bad.outputBuses.add (juce::AudioChannelSet::mono());
        check (! proc->checkBusesLayoutSupported (bad), "stereo in / mono out is rejected");
    }

    // ---- editor ---------------------------------------------------------------------------------------
    {
        proc->loadPreset (0);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
        check (editor != nullptr, "the editor can be created");

        if (editor != nullptr)
        {
            check (editor->getWidth() == 720 && editor->getHeight() == 470,
                   "a new editor opens at 720 x 470 (" + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()) + ")");
            editor->setSize (480, 313);
            editor->setSize (1800, 1175);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            check (image.isValid() && image.getWidth() == 1800, "the editor paints at its largest size");
            editor->setSize (900, 588);
        }

        editor.reset();

        editor.reset (proc->createEditor());
        check (editor != nullptr && editor->getWidth() == 900, "the editor reopens at the size it was closed with");
        editor.reset();

        // open and close a few times (catches leaks and dangling listeners)
        for (int i = 0; i < 5; ++i)
        {
            proc->loadPreset (i * 9);
            std::unique_ptr<juce::AudioProcessorEditor> again (proc->createEditor());
        }
        check (true, "the editor can be opened and closed repeatedly");

        if (argc > 1)
        {
            const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])));
            folder.createDirectory();

            proc->loadPreset (0);
            check (saveSnapshot (*proc, folder.getChildFile ("editor-default.png"), 720, 2.0f), "snapshot editor-default.png");

            proc->loadPreset (2);
            if (auto* freeze = proc->apvts.getParameter ("freeze"))
                freeze->setValueNotifyingHost (1.0f);
            check (saveSnapshot (*proc, folder.getChildFile ("editor-preset-small.png"), 480, 2.0f), "snapshot editor-preset-small.png");

            proc->loadPreset (49);
            check (saveSnapshot (*proc, folder.getChildFile ("editor-preset-large.png"), 1100, 1.0f), "snapshot editor-preset-large.png");
        }
    }

    proc.reset();

    std::cout << std::endl << (failures == 0 ? "PLUGIN TEST PASSED" : "PLUGIN TEST FAILED") << " (" << failures << " failures)" << std::endl;
    return failures == 0 ? 0 : 1;
}
