// Draws the window, headless, to pictures: at rest and with a signal, and at another size.
// Built and run by Tests/run-editor-pictures.sh; the pictures go to the folder it is given.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    void runMessages (int milliseconds)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
    }

    void save (juce::Component& c, const juce::File& folder, const juce::String& name)
    {
        juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
        {
            juce::Graphics g (image);
            c.paintEntireComponent (g, true);
        }

        juce::File file (folder.getChildFile (name));
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        png.writeImageToStream (image, stream);
        std::printf ("picture: %s\n", file.getFullPathName().toRawUTF8());
    }

    void play (DAAuraAudioProcessor& proc, int blocks, double& phase, float amplitude)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                const auto s = (float) (amplitude * std::sin (phase * 2.0 * juce::MathConstants<double>::pi));
                buffer.setSample (0, n, s);
                buffer.setSample (1, n, s);
                phase += 110.0 / 48000.0;
            }

            proc.processBlock (buffer, midi);
        }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto folder = juce::File (argc > 1 ? argv[1] : "/tmp/daaura-pictures");
    folder.createDirectory();

    DAAuraAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.applyPreset (aura::findPreset ("Dreamy Cloud"));

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (720, 430);
    runMessages (300);
    save (*editor, folder, "1-at-rest.png");

    double phase = 0.0;
    play (proc, 30, phase, 0.30f);
    runMessages (300);
    save (*editor, folder, "2-with-a-signal.png");

    play (proc, 6, phase, 0.85f);
    runMessages (200);
    save (*editor, folder, "3-loud.png");

    // the browsing: a picture of the window on a few presets
    proc.applyPreset (aura::findPreset ("Cathedral"));
    runMessages (150);
    save (*editor, folder, "4-cathedral.png");

    proc.applyPreset (aura::findPreset ("Frozen Glass"));
    runMessages (150);
    save (*editor, folder, "5-frozen-glass.png");

    editor->setSize (1100, 657);
    runMessages (200);
    save (*editor, folder, "6-larger.png");

    editor.reset();
    proc.releaseResources();
    return 0;
}
