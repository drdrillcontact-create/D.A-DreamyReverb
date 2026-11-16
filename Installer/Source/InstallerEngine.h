#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// The part of the installer that has nothing to do with its window: what it carries, where
// that goes, and putting it there.

namespace daaura
{

/** One thing the installer can put somewhere: a plug-in format and the folder it belongs in. */
struct InstallTarget
{
    juce::String format;        // the folder in the payload: "VST3" or "AU"
    juce::String title;         // what the page calls it
    juce::File   directory;     // the folder the plug-in goes into
    juce::String shownPath;     // how the page writes that folder
    bool         selected = true;
};

/** The targets of the system the installer runs on, with their standard folders:
    Windows  VST3  ->  C:\Program Files\Common Files\VST3
    macOS    AU    ->  ~/Library/Audio/Plug-Ins/Components
             VST3  ->  ~/Library/Audio/Plug-Ins/VST3
*/
std::vector<InstallTarget> getStandardTargets();

//==============================================================================
/** The plug-in files the installer carries: a zip with one folder per format, and in each of
    those the plug-in as it should arrive in the target folder.

        VST3/D.A AURA.vst3/...
        AU/D.A AURA.component/...
*/
class Payload
{
public:
    /** Opens the payload that was built into this installer.
        Returns nullptr, and says why in `error`, if there is none or it cannot be read. */
    static std::unique_ptr<Payload> openBuiltIn (juce::String& error);

    /** Reads a payload from a stream (the tests use this). */
    static std::unique_ptr<Payload> open (std::unique_ptr<juce::InputStream>, juce::String& error);

    /** The formats in the payload. */
    juce::StringArray getFormats() const;

    /** The name of the plug-in for a format, e.g. "D.A AURA.vst3". Empty if there is none. */
    juce::String getBundleName (const juce::String& format) const;

    /** The size of a format's files once they are unpacked. */
    juce::int64 getSize (const juce::String& format) const;

    /** Unpacks a format's plug-in into `folder`, which becomes the plug-in itself
        (so `folder` is where "D.A AURA.vst3" is to be, under whatever name).
        `bytesDone` is called with the number of bytes written so far. */
    juce::Result unpack (const juce::String& format, const juce::File& folder,
                         const std::function<void (juce::int64)>& bytesDone);

private:
    Payload() = default;

    struct Bundle
    {
        juce::String format, name;
        juce::int64 size = 0;
        juce::Array<int> entries;       // indices into the zip
    };

    const Bundle* find (const juce::String& format) const;
    juce::String scan();

    std::unique_ptr<juce::ZipFile> zip;
    std::vector<Bundle> bundles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Payload)
};

//==============================================================================
/** What an installation did. */
struct InstallReport
{
    bool succeeded = false;
    juce::String error;                 // for the user, when it did not succeed
    juce::StringArray installed;        // the full path of every plug-in that was put in place
};

/** Puts the selected targets' plug-ins in place. An older copy of the same plug-in is replaced.

    Each plug-in is first unpacked next to its final place and only then swapped in, so a
    failure half-way never leaves a broken plug-in under the final name, and a copy that a
    running host still has open is left as it is (the installation then fails and says so).

    `progress` (0..1) may be called from the calling thread at any time. */
InstallReport install (Payload&, const std::vector<InstallTarget>&,
                       const std::function<void (double)>& progress = {});

} // namespace daaura
