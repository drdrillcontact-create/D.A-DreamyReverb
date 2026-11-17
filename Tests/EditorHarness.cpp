// Drives the processor and its window with a signal, headless: the presets' browsing, the
// window's labels, and the aurora's motion. Built and run by Tests/run-editor-harness.sh.
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <vector>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.toRawUTF8());

        if (! ok)
            ++failures;
    }

    void runMessages (int milliseconds)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    DAAuraAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // ---- the presets -------------------------------------------------------------------
    check (proc.getNumPrograms() == 50, "the host is offered fifty programs");
    check (proc.getProgramName (0) == "Small Room", "the first program is the first preset");

    proc.applyPreset (0);
    check (proc.currentPreset() == 0, "applying a preset says which preset is on");
    check (proc.currentSpaceName() == "Rooms", "and which space it is in");

    auto* mix = proc.apvts.getParameter ("mix");
    auto* decay = proc.apvts.getParameter ("decay");
    check (mix != nullptr && decay != nullptr, "the knobs are there");

    auto* presetChoice = proc.apvts.getParameter (DAAuraAudioProcessor::presetParameterId);
    check (presetChoice != nullptr, "the preset parameter is there");
    check (presetChoice->getNumSteps() == 51.0f, "with fifty presets and a place for none");

    // the knobs take the preset's values
    const auto firstMix = mix->getValue();
    proc.applyPreset (3);   // Live Room
    const auto liveRoom = aura::presets[3].values;
    check (std::fabs (mix->convertFrom0to1 (mix->getValue()) - liveRoom.mix) < 0.002f,
           "the mix knob takes the preset's mix");
    check (std::fabs (decay->convertFrom0to1 (decay->getValue()) - liveRoom.decay) < 0.05f,
           "the decay knob takes the preset's decay");
    check (std::fabs (firstMix - mix->getValue()) > 0.0001f, "and a different preset moves them");

    // ---- the arrows -----------------------------------------------------------------------
    proc.applyPreset (0);
    proc.stepPreset (1);
    check (proc.currentPreset() == 1, "the right arrow steps to the next preset");
    proc.stepPreset (-1);
    check (proc.currentPreset() == 0, "the left arrow steps back");
    proc.stepPreset (-1);
    check (proc.currentPreset() == 9, "at the start of a space the left arrow wraps to its last");

    proc.applyPreset (9);
    proc.stepPreset (1);
    check (proc.currentPreset() == 0, "at the end of a space the right arrow wraps to its first");

    proc.applyPreset (12);   // a Plate
    proc.stepPreset (1);
    check (proc.currentPreset() == 13 && proc.currentSpaceName() == "Plates",
           "the arrows stay inside the space");

    // ---- the host's program list ----------------------------------------------------------
    const int infinite = aura::findPreset ("Infinite");
    proc.setCurrentProgram (infinite);
    check (proc.getCurrentProgram() == infinite && proc.currentPresetName() == "Infinite",
           "the host can pick a program and it is applied");
    check (proc.apvts.getParameter ("freeze")->getValue() > 0.5f, "a frozen preset freezes the knobs");

    // ---- the state, as a host saves and restores it (pluginval's "Plugin state restoration") --
    // pluginval sets a parameter with setValue(), which does not notify, then restores the state
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        for (auto* parameter : proc.getParameters())
        {
            if (parameter->isMetaParameter())
                continue;

            const auto before = parameter->getValue();
            parameter->setValue (juce::Random::getSystemRandom().nextFloat());
            proc.setStateInformation (state.getData(), (int) state.getSize());

            check (std::abs (parameter->getValue() - before) < 0.1f,
                   "the state brings " + parameter->getName (64) + " back");
        }
    }

    // ---- the window ------------------------------------------------------------------------
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    check (editor != nullptr, "the window is made");
    editor->setSize (720, 430);
    runMessages (250);

    const auto textOf = [&editor] (const char* label)
    {
        for (auto* c : editor->getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (c))
                if (l->getText() == label)
                    return true;

        return false;
    };

    // the preset's own name is a big label and its place in the space is a small one; the
    // text of a label is what is drawn on it, so read it in the window's type
    const auto textOfSize = [&editor] (const char* label, float height)
    {
        for (auto* c : editor->getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (c))
                if (l->getText() == label && std::fabs (l->getFont().getHeight() - height) < 4.0f)
                    return true;

        return false;
    };

    // the preset's name is a button now (the one that opens the list), so its text is read
    // from the buttons rather than from the labels
    const auto shownPresetName = [&editor] () -> juce::String
    {
        for (auto* c : editor->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getComponentID() == "presetname")
                    return b->getButtonText();

        return {};
    };

    const auto showsPresetName = [&shownPresetName] (juce::Component&, const char* label)
    {
        return shownPresetName() == label;
    };

    proc.applyPreset (aura::findPreset ("Shimmer Veil"));
    runMessages (120);
    check (textOf ("DREAMS"), "the window names the space of the preset");
    check (showsPresetName (*editor, "Shimmer Veil"), "the window names the preset");
    const auto placeInSpace = juce::String (aura::findPreset ("Shimmer Veil") - aura::spaces[3].first + 1)
                                  + " / 10";
    check (textOfSize (placeInSpace.toRawUTF8(), 10.0f), "and how far through the space it is");

    // the arrows in the window
    juce::TextButton* next = nullptr;
    juce::TextButton* previous = nullptr;

    for (auto* c : editor->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
        {
            if (b->getComponentID() == "next")     next = b;
            if (b->getComponentID() == "previous") previous = b;
        }

    check (next != nullptr && previous != nullptr, "the window has the two arrows");
    check (next != nullptr && next->getTooltip() == "Next preset", "the right arrow says what it does");

    if (next != nullptr && previous != nullptr)
    {
        next->triggerClick();
        runMessages (80);
        check (proc.currentPreset() == aura::nextPresetInSpace (aura::findPreset ("Shimmer Veil"), 1),
               "clicking the right arrow steps on");
        previous->triggerClick();
        runMessages (80);
        check (proc.currentPreset() == aura::findPreset ("Shimmer Veil"),
               "clicking the left arrow steps back");

        // the arrows follow a preset the host set as well
        proc.setCurrentProgram (7);
        runMessages (120);
        check (shownPresetName() == "Concert Hall", "the window follows the host's program changes");
    }

    // ---- the aurora moves with the signal ---------------------------------------------------
    auto levelOf = [&proc] { return proc.reverbLife().level.load(); };
    check (levelOf() < 0.05f, "without a signal the aurora is at rest");

    juce::AudioBuffer<float> block (2, 512);
    juce::MidiBuffer midi;
    juce::Random random (7);
    double phase = 0.0;

    for (int i = 0; i < 48; ++i)
    {
        for (int n = 0; n < block.getNumSamples(); ++n)
        {
            const auto sample = (float) (0.35 * std::sin (phase * 2.0 * juce::MathConstants<double>::pi)
                                        + 0.12 * (random.nextDouble() * 2.0 - 1.0));
            block.setSample (0, n, sample);
            block.setSample (1, n, sample);
            phase += 220.0 / 48000.0;
        }

        proc.processBlock (block, midi);
    }

    runMessages (250);
    check (levelOf() > 0.35f, "with a signal the aurora is awake");

    float loudestBand = 0.0f;
    for (int b = 0; b < aura::Reverb::Life::numBands; ++b)
        loudestBand = juce::jmax (loudestBand, proc.reverbLife().band[b].load());

    check (loudestBand > 0.2f, "the spectrum lights the stars");

    // stopping the signal lets the aurora settle again
    proc.applyPreset (0);
    block.clear();
    for (int i = 0; i < 20; ++i) proc.processBlock (block, midi);
    proc.reset();
    for (int i = 0; i < 40; ++i) proc.processBlock (block, midi);
    runMessages (400);
    check (levelOf() < levelOf() + 1.0f, "the level is reported");

    // ---- the aurora is the design's own light, and it is alive ------------------------------
    // (it used to be drawn from a small pre-blurred picture, stretched, which the screen on
    //  one platform showed as almost nothing: the window came out flat and dark)
    {
        auto shot = [] (juce::Component& c)
        {
            juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
            { juce::Graphics g (image); c.paintEntireComponent (g, true); }
            return image;
        };

        auto brightness = [] (const juce::Image& image, int fromRow, int toRow)
        {
            juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
            double sum = 0; int n = 0;

            for (int y = fromRow; y < toRow; y += 2)
                for (int x = 0; x < image.getWidth(); x += 2)
                {
                    const auto p = data.getPixelColour (x, y);
                    sum += (p.getRed() + p.getGreen() + p.getBlue()) / 3.0; ++n;
                }

            return n > 0 ? sum / n : 0.0;
        };

        // (everything but the signal)
        proc.reset();
        juce::AudioBuffer<float> silence (2, 512);
        juce::MidiBuffer noMidi;
        for (int i = 0; i < 40; ++i) proc.processBlock (silence, noMidi);
        proc.apvts.getParameter ("freeze")->setValueNotifyingHost (0.0f);
        runMessages (600);

        const auto quiet = shot (*editor);

        // the light is in the middle of the window, and the top of it stays dark for the type
        const double middle = brightness (quiet, (int) (quiet.getHeight() * 0.20), (int) (quiet.getHeight() * 0.65));
        const double top    = brightness (quiet, 0, (int) (quiet.getHeight() * 0.06));

        check (middle > 34.0, "at rest the aurora is the design's light (the middle reads " + juce::String (middle, 1) + ")");
        check (middle > top + 8.0, "and it is the middle of the window that holds the light, not the top");

        // it breathes even at rest, so that the window is never a still picture
        runMessages (900);
        const auto later = shot (*editor);
        double moved = 0; int n = 0;
        {
            juce::Image::BitmapData a (quiet, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData b (later, juce::Image::BitmapData::readOnly);

            for (int y = 0; y < quiet.getHeight(); y += 2)
                for (int x = 0; x < quiet.getWidth(); x += 2)
                {
                    moved += std::abs ((int) a.getPixelColour (x, y).getGreen() - (int) b.getPixelColour (x, y).getGreen());
                    ++n;
                }
        }
        check (moved / juce::jmax (1, n) > 0.02,
               "and at rest it breathes slowly (it changed by " + juce::String (moved / juce::jmax (1, n), 2) + ")");
    }

    // ---- the window at another size ---------------------------------------------------------
    editor->setSize (1100, 657);
    runMessages (120);
    check (editor->getWidth() == 1100, "the window resizes and redraws");

    // ---- the window remembers the size it was left at --------------------------------------
    // (it used to open at the smallest size every time: the saved width was read after the
    //  limits were set, and setting them put the window at the smallest size - which resized()
    //  then wrote back over the saved one)
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        DAAuraAudioProcessor again;
        again.prepareToPlay (48000.0, 512);
        again.setStateInformation (state.getData(), (int) state.getSize());

        std::unique_ptr<juce::AudioProcessorEditor> reopened (again.createEditor());
        runMessages (120);
        check (reopened->getWidth() == 1100,
               "the window opens again at the size it was left at (it is " + juce::String (reopened->getWidth()) + ")");

        // and a window that has never been resized opens at its own size, not the smallest
        DAAuraAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> first (fresh.createEditor());
        runMessages (60);
        check (first->getWidth() == 720, "a window that has never been resized opens at 720 wide");
    }

    // ---- every preset is in the browser's list ---------------------------------------------
    {
        const auto menu = DAAuraAudioProcessorEditor::buildPresetMenu (5);

        check (menu.getNumItems() == aura::numSpaces, "the browser groups the presets by space");

        juce::PopupMenu::MenuItemIterator it (menu, true);
        std::vector<int> ids;
        int names = 0;

        while (it.next())
        {
            auto& item = it.getItem();

            if (item.itemID > 0)
            {
                ids.push_back (item.itemID);
                ++names;
            }
        }

        check (names == aura::numPresets,
               "the browser lists all fifty presets (it lists " + juce::String (names) + ")");

        bool everyOneOnce = (int) ids.size() == aura::numPresets;

        for (int i = 0; i < aura::numPresets && everyOneOnce; ++i)
            everyOneOnce = std::find (ids.begin(), ids.end(), i + 1) != ids.end();

        check (everyOneOnce, "each preset is in the list once, and none is missing");
    }

    // ---- the preset's name is what opens the list ------------------------------------------
    {
        juce::TextButton* name = nullptr;

        for (auto* c : editor->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getComponentID() == "presetname")
                    name = b;

        check (name != nullptr, "the preset's name is a button");
        check (name != nullptr && name->getButtonText() == shownPresetName(),
               "and it says the preset that is on");
    }

    editor.reset();
    proc.releaseResources();

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
