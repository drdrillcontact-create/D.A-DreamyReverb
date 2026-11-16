#include "InstallerEngine.h"

#if JUCE_WINDOWS
 // In PayloadWindows.cpp, which keeps <windows.h> away from the JUCE headers
 extern "C" int daaura_findBuiltInPayload (const void** data, size_t* size);
#endif

namespace daaura
{

std::vector<InstallTarget> getStandardTargets()
{
   #if JUCE_WINDOWS
    // (the folder every VST3 host looks in; on an ordinary system C:\Program Files\Common Files\VST3)
    const auto vst3 = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                          .getChildFile ("Common Files").getChildFile ("VST3");

    return { { "VST3", "VST3", vst3, vst3.getFullPathName(), true } };
   #elif JUCE_MAC
    // The user's own plug-in folders: every host looks there too, and writing to them needs no
    // administrator password.
    const auto plugIns = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                             .getChildFile ("Library/Audio/Plug-Ins");

    return { { "AU",   "Audio Unit", plugIns.getChildFile ("Components"), "~/Library/Audio/Plug-Ins/Components", true },
             { "VST3", "VST3",       plugIns.getChildFile ("VST3"),       "~/Library/Audio/Plug-Ins/VST3",       true } };
   #else
    const auto vst3 = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3");
    return { { "VST3", "VST3", vst3, "~/.vst3", true } };
   #endif
}

//==============================================================================
std::unique_ptr<Payload> Payload::openBuiltIn (juce::String& error)
{
    const char* const missing = "This installer is incomplete: the plug-in files are missing from it.\n"
                                "Please download it again.";

   #if JUCE_WINDOWS
    const void* data = nullptr;
    size_t size = 0;

    if (daaura_findBuiltInPayload (&data, &size) == 0 || data == nullptr || size == 0)
    {
        error = missing;
        return nullptr;
    }

    return open (std::make_unique<juce::MemoryInputStream> (data, size, false), error);
   #else
    #if JUCE_MAC
     const auto file = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                           .getChildFile ("Contents/Resources/payload.zip");
    #else
     // (Only for trying the installer out on other systems.)
     const juce::File file (juce::SystemStats::getEnvironmentVariable ("DAAURA_INSTALLER_PAYLOAD", {}));
    #endif

    auto stream = file.existsAsFile() ? file.createInputStream() : nullptr;

    if (stream == nullptr)
    {
        error = missing;
        return nullptr;
    }

    return open (std::move (stream), error);
   #endif
}

std::unique_ptr<Payload> Payload::open (std::unique_ptr<juce::InputStream> stream, juce::String& error)
{
    std::unique_ptr<Payload> payload (new Payload());
    payload->zip = std::make_unique<juce::ZipFile> (std::move (stream));

    error = payload->scan();

    if (error.isNotEmpty())
        return nullptr;

    return payload;
}

// Sorts the zip's entries into plug-ins, and refuses anything that is not laid out as expected.
// Returns what is wrong, or nothing.
juce::String Payload::scan()
{
    const char* const damaged = "This installer is damaged: its plug-in files cannot be read.\n"
                                "Please download it again.";

    if (zip->getNumEntries() == 0)
        return damaged;

    for (int i = 0; i < zip->getNumEntries(); ++i)
    {
        const auto* entry = zip->getEntry (i);
        const auto name = entry->filename.replaceCharacter ('\\', '/');

        juce::StringArray parts;
        parts.addTokens (name.endsWithChar ('/') ? name.dropLastCharacters (1) : name, "/", {});

        // Nothing may point outside the folder it is unpacked into
        for (const auto& part : parts)
            if (part.isEmpty() || part == "." || part == ".." || part.containsChar (':'))
                return damaged;

        if (parts.size() < 2)       // the format's folder itself
            continue;

        if (entry->isSymbolicLink)
            return damaged;

        auto found = std::find_if (bundles.begin(), bundles.end(),
                                   [&] (const Bundle& b) { return b.format == parts[0]; });

        if (found == bundles.end())
        {
            bundles.push_back ({ parts[0], parts[1], 0, {} });
            found = bundles.end() - 1;
        }
        else if (found->name != parts[1])
        {
            return damaged;         // one plug-in per format
        }

        found->entries.add (i);
        found->size += entry->uncompressedSize;
    }

    return bundles.empty() ? juce::String (damaged) : juce::String();
}

const Payload::Bundle* Payload::find (const juce::String& format) const
{
    for (const auto& b : bundles)
        if (b.format == format)
            return &b;

    return nullptr;
}

juce::StringArray Payload::getFormats() const
{
    juce::StringArray formats;

    for (const auto& b : bundles)
        formats.add (b.format);

    return formats;
}

juce::String Payload::getBundleName (const juce::String& format) const
{
    const auto* b = find (format);
    return b != nullptr ? b->name : juce::String();
}

juce::int64 Payload::getSize (const juce::String& format) const
{
    const auto* b = find (format);
    return b != nullptr ? b->size : 0;
}

juce::Result Payload::unpack (const juce::String& format, const juce::File& folder,
                              const std::function<void (juce::int64)>& bytesDone)
{
    const auto* bundle = find (format);

    if (bundle == nullptr)
        return juce::Result::fail ("This installer does not contain the " + format + " plug-in.");

    const auto cannotWrite = [] (const juce::File& f)
    {
        return juce::Result::fail ("Could not write\n" + f.getFullPathName());
    };

    if (folder.createDirectory().failed())
        return cannotWrite (folder);

    const auto prefix = bundle->format + "/" + bundle->name;
    juce::HeapBlock<char> buffer (1 << 16);
    juce::int64 done = 0;

    for (const int index : bundle->entries)
    {
        const auto* entry = zip->getEntry (index);
        const auto name   = entry->filename.replaceCharacter ('\\', '/');
        const auto inside = name.substring (prefix.length()).trimCharactersAtStart ("/");   // path within the plug-in
        const bool isFolder = name.endsWithChar ('/');

        if (inside.isEmpty())           // the plug-in's own folder
            continue;

        const auto target = folder.getChildFile (inside.trimCharactersAtEnd ("/"));

        if (! target.isAChildOf (folder))
            return juce::Result::fail ("This installer is damaged. Please download it again.");

        if (isFolder)
        {
            if (target.createDirectory().failed())
                return cannotWrite (target);

            continue;
        }

        if (target.getParentDirectory().createDirectory().failed())
            return cannotWrite (target.getParentDirectory());

        std::unique_ptr<juce::InputStream> in (zip->createStreamForEntry (index));

        if (in == nullptr)
            return juce::Result::fail ("This installer is damaged. Please download it again.");

        juce::int64 written = 0;

        {
            target.deleteFile();
            juce::FileOutputStream out (target);

            if (out.failedToOpen())
                return cannotWrite (target);

            while (written < entry->uncompressedSize)
            {
                const auto wanted = (int) juce::jmin ((juce::int64) (1 << 16), entry->uncompressedSize - written);
                const auto got = in->read (buffer, wanted);

                if (got <= 0)
                    break;

                if (! out.write (buffer, (size_t) got))
                    return cannotWrite (target);

                written += got;
                done += got;

                if (bytesDone)
                    bytesDone (done);
            }

            out.flush();

            if (out.getStatus().failed())
                return cannotWrite (target);
        }

        if (written != entry->uncompressedSize)
            return juce::Result::fail ("This installer is damaged. Please download it again.");

       #if ! JUCE_WINDOWS
        // The zip records which files are programs. (A plug-in's binary is also marked when
        // the zip does not say, so that it loads whatever tool made the zip.)
        const auto mode = (entry->externalFileAttributes >> 16) & 0777u;

        if ((mode & 0111u) != 0 || inside.startsWith ("Contents/MacOS/"))
            target.setExecutePermission (true);
       #endif
    }

    return juce::Result::ok();
}

//==============================================================================
namespace
{
// A name next to `wanted` that is free, after removing what an interrupted installation left
juce::File freeSibling (const juce::File& wanted)
{
    for (int attempt = 1; attempt <= 20; ++attempt)
    {
        const auto candidate = attempt == 1 ? wanted
                                            : wanted.getSiblingFile (wanted.getFileName() + "-" + juce::String (attempt));
        candidate.deleteRecursively();

        if (! candidate.exists())
            return candidate;
    }

    return {};
}

juce::Result installOne (Payload& payload, const InstallTarget& target,
                         const std::function<void (juce::int64)>& bytesDone)
{
    const auto name = payload.getBundleName (target.format);

    if (name.isEmpty())
        return juce::Result::fail ("This installer does not contain the " + target.title + " plug-in.");

    const auto& directory = target.directory;

    if (directory.createDirectory().failed())
        return juce::Result::fail ("Could not create the folder\n" + directory.getFullPathName());

    const auto place    = directory.getChildFile (name);
    const auto staging  = freeSibling (directory.getChildFile (name + ".installing"));
    const auto previous = freeSibling (directory.getChildFile (name + ".previous"));

    if (staging == juce::File() || previous == juce::File())
        return juce::Result::fail ("Could not write to the folder\n" + directory.getFullPathName());

    // 1. unpack next to the final place (under a name no host takes for a plug-in)
    if (const auto unpacked = payload.unpack (target.format, staging, bytesDone); unpacked.failed())
    {
        staging.deleteRecursively();
        return unpacked;
    }

    // 2. move an older copy out of the way. A host that has it open keeps it from moving.
    const bool replacing = place.exists();

    if (replacing && ! place.moveFileTo (previous))
    {
        staging.deleteRecursively();
        return juce::Result::fail (name + " is in use and could not be replaced.\n"
                                   "Close your DAW, then run the installer again.");
    }

    // 3. the new copy takes the final name
    if (! staging.moveFileTo (place))
    {
        if (replacing)
            previous.moveFileTo (place);

        staging.deleteRecursively();
        return juce::Result::fail ("Could not put " + name + " into the folder\n" + directory.getFullPathName());
    }

    // 4. (If the old copy cannot be removed now, the next installation removes it.)
    previous.deleteRecursively();
    return juce::Result::ok();
}
}

InstallReport install (Payload& payload, const std::vector<InstallTarget>& targets,
                       const std::function<void (double)>& progress)
{
    InstallReport report;
    juce::int64 total = 0;
    int selected = 0;

    for (const auto& t : targets)
    {
        if (t.selected)
        {
            total += payload.getSize (t.format);
            ++selected;
        }
    }

    if (selected == 0)
    {
        report.error = "Nothing was selected to install.";
        return report;
    }

    juce::int64 before = 0;

    for (const auto& t : targets)
    {
        if (! t.selected)
            continue;

        const auto result = installOne (payload, t, [&] (juce::int64 bytes)
        {
            if (progress && total > 0)
                progress (juce::jlimit (0.0, 1.0, (double) (before + bytes) / (double) total));
        });

        if (result.failed())
        {
            report.error = result.getErrorMessage();
            return report;
        }

        report.installed.add (t.directory.getChildFile (payload.getBundleName (t.format)).getFullPathName());
        before += payload.getSize (t.format);
    }

   #if JUCE_MAC
    // macOS keeps a list of the Audio Units it knows. Its keeper starts afresh when asked for,
    // and then finds the new one without the user having to log out first.
    for (const auto& t : targets)
    {
        if (t.selected && t.format == "AU")
        {
            juce::ChildProcess refresh;

            if (refresh.start (juce::StringArray { "/usr/bin/killall", "-9", "AudioComponentRegistrar" }))
                refresh.waitForProcessToFinish (3000);

            break;
        }
    }
   #endif

    if (progress)
        progress (1.0);

    report.succeeded = true;
    return report;
}

} // namespace daaura
