// Runs the installer's own code without a display (Linux, local JUCE build): the routine that
// puts the plug-in in place, against temporary folders, and the pages, by clicking through them.
// It can also write a picture of every page, exactly as JUCE paints it.
//
//   Tests/run-installer-harness.sh [folder for the pictures]
//   Tests/run-installer-harness.sh --icon <file.png>              draws the installer's icon
//   Tests/run-installer-harness.sh --payload <zip> <folder>       installs a payload zip as the build
//                                                                 makes it into <folder>/<format>
#include "../Installer/Source/InstallerArt.h"
#include "../Installer/Source/InstallerConfig.h"
#include "../Installer/Source/InstallerWindow.h"
#include "../Installer/Source/InstallerPicture.h"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
using namespace daaura;

int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Entry
{
    juce::String path, content;
};

// A zip as the build makes it: one folder per format, the plug-in inside
juce::MemoryBlock makeZip (const std::vector<Entry>& entries)
{
    juce::ZipFile::Builder builder;

    for (const auto& e : entries)
        builder.addEntry (new juce::MemoryInputStream (e.content.toRawUTF8(), e.content.getNumBytesAsUTF8(), true),
                          6, e.path, juce::Time::getCurrentTime());

    juce::MemoryOutputStream out;
    builder.writeToStream (out, nullptr);
    return out.getMemoryBlock();
}

std::unique_ptr<Payload> openZip (const juce::MemoryBlock& zip, juce::String& error)
{
    return Payload::open (std::make_unique<juce::MemoryInputStream> (zip, true), error);
}

const juce::String binaryText = juce::String::repeatedString ("the plug-in's code, version two. ", 40000);     // 1.3 MB

std::vector<Entry> pluginEntries()
{
    return { { "VST3/D.A AURA.vst3/Contents/x86_64-win/D.A AURA.vst3",   binaryText },
             { "VST3/D.A AURA.vst3/Contents/Resources/moduleinfo.json",  "{ \"Name\": \"D.A AURA\" }" },
             { "AU/D.A AURA.component/Contents/MacOS/D.A AURA",          binaryText },
             { "AU/D.A AURA.component/Contents/Info.plist",              "<plist/>" },
             { "AU/D.A AURA.component/Contents/Resources/D.A AURA.rsrc", "" } };
}

juce::StringArray listing (const juce::File& folder)
{
    juce::StringArray names;

    for (const auto& f : folder.findChildFiles (juce::File::findFilesAndDirectories, true))
        names.add (f.getRelativePathFrom (folder) + (f.isDirectory() ? "/" : ""));

    names.sort (false);
    return names;
}

void letRun (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

bool writePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream (file);
    return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
}

// The installer's icon: the brand mark on a dark tile
juce::Image drawIcon (int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);
    const float s = (float) size;
    const auto tile = juce::Rectangle<float> (s, s).reduced (0.085f * s);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1113), 0.5f * s, 0.2f * s,
                                             juce::Colour (0xff050404), 0.5f * s, 0.95f * s, false));
    g.fillRoundedRectangle (tile, 0.19f * s);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0x55ea0612), 0.5f * s, 0.58f * s,
                                             juce::Colour (0x00ea0612), 0.5f * s, 0.98f * s, true));
    g.fillRoundedRectangle (tile, 0.19f * s);

    g.setColour (juce::Colour (0xff3a2a2c));
    g.drawRoundedRectangle (tile.reduced (0.006f * s), 0.185f * s, 0.012f * s);

    const float markWidth = 0.56f * s;
    drawBrandMark (g, juce::Rectangle<float> (markWidth, markWidth * 43.7f / 56.5f).withCentre ({ 0.5f * s, 0.505f * s }),
                   juce::Colour (0xffea0612));
    return image;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 2 && juce::String (argv[1]) == "--icon")
    {
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        const bool written = writePng (drawIcon (1024), file);
        std::printf ("%s %s\n", written ? "wrote" : "could not write", file.getFullPathName().toRawUTF8());
        return written ? 0 : 1;
    }

    if (argc > 3 && juce::String (argv[1]) == "--payload")
    {
        const auto zip = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);

        juce::String error;
        auto payload = Payload::open (zip.createInputStream(), error);

        if (payload == nullptr)
        {
            std::printf ("refused: %s\n", error.replace ("\n", " ").toRawUTF8());
            return 1;
        }

        std::vector<InstallTarget> targets;

        for (const auto& format : payload->getFormats())
        {
            std::printf ("%s: %s, %lld bytes\n", format.toRawUTF8(), payload->getBundleName (format).toRawUTF8(), (long long) payload->getSize (format));
            targets.push_back ({ format, format, folder.getChildFile (format), {}, true });
        }

        const auto report = install (*payload, targets);

        for (const auto& path : report.installed)
            std::printf ("installed: %s\n", path.toRawUTF8());

        if (! report.succeeded)
            std::printf ("failed: %s\n", report.error.replace ("\n", " ").toRawUTF8());

        return report.succeeded ? 0 : 1;
    }

    const auto pictures = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("daaura-installer-harness-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    scratch.createDirectory();

    const auto targetsIn = [] (const juce::File& root)
    {
        return std::vector<InstallTarget> { { "AU",   "Audio Unit", root.getChildFile ("Components"), "~/Library/Audio/Plug-Ins/Components", true },
                                            { "VST3", "VST3",       root.getChildFile ("VST3"),       "~/Library/Audio/Plug-Ins/VST3",       true } };
    };

    //==============================================================================
    std::printf ("what the installer carries\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);
        check (payload != nullptr && error.isEmpty(), "a zip with one plug-in per format is accepted");

        if (payload == nullptr)
            return 1;

        check (payload->getFormats() == juce::StringArray { "VST3", "AU" }
                && payload->getBundleName ("VST3") == "D.A AURA.vst3" && payload->getBundleName ("AU") == "D.A AURA.component",
               "it lists the formats and what each plug-in is called");
        check (payload->getSize ("VST3") == (juce::int64) binaryText.length() + 22 && payload->getBundleName ("AAX").isEmpty(),
               "it knows how large a plug-in is, and that a format it does not carry is not there");

        juce::MemoryBlock garbage ("this is not a zip at all", 24);
        const bool refused = openZip (garbage, error) == nullptr && error.containsIgnoreCase ("damaged");
        check (refused, "something that is not a zip is refused  (\"" + error.replace ("\n", " ") + "\")");

        check (openZip (makeZip ({ { "VST3/../../outside.txt", "x" } }), error) == nullptr, "a zip with a path that leads out of the target folder is refused");
        check (openZip (makeZip ({ { "VST3/A.vst3/x", "x" }, { "VST3/B.vst3/x", "x" } }), error) == nullptr, "a zip with two plug-ins in one format is refused");
    }

    //==============================================================================
    std::printf ("installing\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);
        const auto root = scratch.getChildFile ("first");
        auto targets = targetsIn (root);

        std::vector<double> progress;
        auto report = install (*payload, targets, [&] (double p) { progress.push_back (p); });

        const auto vst3 = root.getChildFile ("VST3/D.A AURA.vst3");
        const auto au   = root.getChildFile ("Components/D.A AURA.component");

        check (report.succeeded && report.error.isEmpty()
                && report.installed == juce::StringArray { au.getFullPathName(), vst3.getFullPathName() },
               "both plug-ins are reported as installed, with their full paths");
        check (vst3.getChildFile ("Contents/x86_64-win/D.A AURA.vst3").loadFileAsString() == binaryText
                && vst3.getChildFile ("Contents/Resources/moduleinfo.json").loadFileAsString() == "{ \"Name\": \"D.A AURA\" }"
                && au.getChildFile ("Contents/MacOS/D.A AURA").loadFileAsString() == binaryText
                && au.getChildFile ("Contents/Resources/D.A AURA.rsrc").existsAsFile(),
               "every file arrives, byte for byte (an empty one too)");
        check (listing (root) == juce::StringArray { "Components/", "Components/D.A AURA.component/", "Components/D.A AURA.component/Contents/",
                                                     "Components/D.A AURA.component/Contents/Info.plist", "Components/D.A AURA.component/Contents/MacOS/",
                                                     "Components/D.A AURA.component/Contents/MacOS/D.A AURA", "Components/D.A AURA.component/Contents/Resources/",
                                                     "Components/D.A AURA.component/Contents/Resources/D.A AURA.rsrc",
                                                     "VST3/", "VST3/D.A AURA.vst3/", "VST3/D.A AURA.vst3/Contents/", "VST3/D.A AURA.vst3/Contents/Resources/",
                                                     "VST3/D.A AURA.vst3/Contents/Resources/moduleinfo.json", "VST3/D.A AURA.vst3/Contents/x86_64-win/",
                                                     "VST3/D.A AURA.vst3/Contents/x86_64-win/D.A AURA.vst3" },
               "and nothing else is left in the folders");
        check (::access (au.getChildFile ("Contents/MacOS/D.A AURA").getFullPathName().toRawUTF8(), X_OK) == 0,
               "the Audio Unit's binary is marked as a program");
        check (! progress.empty() && std::is_sorted (progress.begin(), progress.end()) && progress.front() >= 0.0 && progress.back() == 1.0,
               "progress only ever goes up, and ends at 100 %  (" + juce::String ((int) progress.size()) + " reports)");

        // an older version is already there
        vst3.getChildFile ("Contents/x86_64-win/D.A AURA.vst3").replaceWithText ("the old code");
        vst3.getChildFile ("Contents/Resources/left-over-from-version-one.txt").replaceWithText ("x");
        root.getChildFile ("VST3/Some Other Plug-in.vst3/Contents").createDirectory();
        report = install (*payload, targets);
        check (report.succeeded && vst3.getChildFile ("Contents/x86_64-win/D.A AURA.vst3").loadFileAsString() == binaryText
                && ! vst3.getChildFile ("Contents/Resources/left-over-from-version-one.txt").exists(),
               "an older copy is replaced as a whole: nothing of it stays behind");
        check (root.getChildFile ("VST3").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2
                && root.getChildFile ("VST3/Some Other Plug-in.vst3/Contents").isDirectory(),
               "other plug-ins in the folder are not touched, and no working copies remain");

        // what an interrupted installation would leave behind
        root.getChildFile ("VST3/D.A AURA.vst3.installing/Contents/half").create();
        root.getChildFile ("VST3/D.A AURA.vst3.previous/Contents/old").create();
        report = install (*payload, targets);
        check (report.succeeded && root.getChildFile ("VST3").getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2,
               "the left-overs of an interrupted installation are cleared away");

        // only what is ticked
        const auto second = scratch.getChildFile ("second");
        auto onlyVst3 = targetsIn (second);
        onlyVst3[0].selected = false;
        report = install (*payload, onlyVst3);
        check (report.succeeded && report.installed.size() == 1 && second.getChildFile ("VST3/D.A AURA.vst3").isDirectory()
                && ! second.getChildFile ("Components").exists(),
               "a format that is not ticked is not installed, and its folder is not even created");

        onlyVst3[1].selected = false;
        report = install (*payload, onlyVst3);
        check (! report.succeeded && report.error.isNotEmpty(), "with nothing ticked there is nothing to install  (\"" + report.error + "\")");
    }

    std::printf ("when it cannot be done\n");
    {
        juce::String error;
        auto payload = openZip (makeZip (pluginEntries()), error);

        // the target folder cannot be created: something else has its name
        const auto blocked = scratch.getChildFile ("blocked");
        blocked.createDirectory();
        blocked.getChildFile ("VST3").replaceWithText ("a file where the folder should be");
        auto targets = targetsIn (blocked);
        targets[0].selected = false;
        auto report = install (*payload, targets);
        check (! report.succeeded && report.error.contains (blocked.getChildFile ("VST3").getFullPathName()) && report.installed.isEmpty(),
               "a folder that cannot be created is named in the message  (\"" + report.error.replace ("\n", " ") + "\")");

        // the folder cannot be written to (as the VST3 folder on Windows without administrator rights)
        if (::geteuid() != 0)
        {
            const auto locked = scratch.getChildFile ("locked");
            const auto folder = locked.getChildFile ("VST3");
            folder.getChildFile ("D.A AURA.vst3/Contents").createDirectory();
            folder.getChildFile ("D.A AURA.vst3/Contents/version-one").replaceWithText ("still here");
            ::chmod (folder.getFullPathName().toRawUTF8(), 0555);

            targets = targetsIn (locked);
            targets[0].selected = false;
            report = install (*payload, targets);
            ::chmod (folder.getFullPathName().toRawUTF8(), 0755);

            check (! report.succeeded && report.error.isNotEmpty()
                    && folder.getChildFile ("D.A AURA.vst3/Contents/version-one").loadFileAsString() == "still here"
                    && folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 1,
                   "without the right to write there it fails, and the copy that was there stays whole  (\"" + report.error.replace ("\n", " ") + "\")");
        }

        // a payload without the format that was asked for
        auto onlyAu = openZip (makeZip ({ { "AU/D.A AURA.component/Contents/Info.plist", "<plist/>" } }), error);
        targets = targetsIn (scratch.getChildFile ("partial"));
        report = install (*onlyAu, targets);
        check (! report.succeeded && report.error.contains ("VST3") && report.installed.size() == 1,
               "a format the installer does not carry is reported, after what could be installed was");
    }

    //==============================================================================
    std::printf ("the pages\n");
    {
        const Assets assets;
        check (assets.getMissing().isEmpty() && assets.text (20.0f).getTypefaceName() == "Red Hat Display",
               "the pictures and the typeface are built in  (" + assets.text (20.0f).getTypefaceName() + ")");

        // A picture of a component as a screen with `scale` pixels to the unit shows it: drawn
        // on nothing, so that where the component draws nothing the picture is see-through
        const auto drawnAt = [] (juce::Component& c, float scale)
        {
            juce::Image image (juce::Image::ARGB, juce::roundToInt (scale * (float) c.getWidth()),
                               juce::roundToInt (scale * (float) c.getHeight()), true, juce::SoftwareImageType());
            juce::Graphics g (image);

            // (As a see-through window is handed over to be drawn on Windows: with nothing as
            // the colour to draw with. Whatever is drawn has to say how solid it is itself.)
            g.setColour (juce::Colours::transparentBlack);

            g.addTransform (juce::AffineTransform::scale (scale));
            c.paintEntireComponent (g, true);
            return image;
        };

        const auto snapshot = [&] (juce::Component& c, const juce::String& name, float scale = 2.0f)
        {
            if (pictures != juce::File())
            {
                pictures.createDirectory();
                writePng (drawnAt (c, scale), pictures.getChildFile (name + ".png"));
            }
        };

        juce::String error;
        const auto root = scratch.getChildFile ("pages");
        auto targets = targetsIn (root);
        int doneCalls = 0;
        std::vector<InstallerView::Page> pagesSeen;

        InstallerShell shell (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), targets, juce::String()));
        auto& view = shell.getView();
        view.onDone = [&] { ++doneCalls; };
        view.onPageChanged = [&] (InstallerView::Page p) { pagesSeen.push_back (p); };

        auto* next     = dynamic_cast<juce::Button*> (view.findChildWithID ("next"));
        auto* cancel   = dynamic_cast<juce::Button*> (view.findChildWithID ("cancel"));
        auto* close    = dynamic_cast<juce::Button*> (view.findChildWithID ("close"));
        auto* minimise = dynamic_cast<juce::Button*> (view.findChildWithID ("minimise"));
        auto* tickAu   = dynamic_cast<juce::Button*> (view.findChildWithID ("target-AU"));
        auto* tickVst3 = dynamic_cast<juce::Button*> (view.findChildWithID ("target-VST3"));

        check (next != nullptr && cancel != nullptr && close != nullptr && minimise != nullptr && tickAu != nullptr && tickVst3 != nullptr,
               "there are Next and Cancel, minimise and close, and a tick box per format");

        if (next == nullptr || cancel == nullptr || close == nullptr || minimise == nullptr || tickAu == nullptr || tickVst3 == nullptr)
            return 1;

        // (A button that was clicked shows itself pressed until it has been painted once and a
        // tenth of a second has passed. Both are seen to here, so that the pictures show the
        // buttons at rest.)
        const auto click = [] (juce::Button& b)
        {
            b.triggerClick();
            letRun (20);
            (void) b.createComponentSnapshot (b.getLocalBounds());
            letRun (130);
        };

        check (view.getPage() == InstallerView::Page::welcome && next->isVisible() && cancel->isVisible()
                && next->getButtonText() == "Next" && cancel->getButtonText() == "Cancel" && ! tickAu->isVisible(),
               "it opens on the welcome page, with Next and Cancel");
        snapshot (shell, "1-welcome");
        snapshot (shell, "1-welcome-at-100-percent", 1.0f);

        //==============================================================================
        // The first page is the design itself
        {
            const auto design = juce::ImageFileFormat::loadFrom (juce::File::getCurrentWorkingDirectory().getChildFile ("Installer/Design/Installer.png"));
            const auto window = juce::Rectangle<int> (58, 86, 1764, 695);       // the part of the design that is the window

            check (design.isValid() && design.getBounds().contains (window),
                   "the design is there to compare with  (Installer/Design/Installer.png)");

            // Drawn with as many pixels to the unit as the design has, every pixel inside the
            // design's frame has to be the design's, and outside that frame there has to be
            // nothing. (All but the last row: the window is a fifth of a pixel short of it.)
            const float own = Assets::pictureScale;
            const auto drawn = drawnAt (shell, own);
            const auto likeness = design.isValid() ? compare (drawn, design, window.getPosition(), Assets::outline(), window.getHeight() - 1)
                                                   : Likeness();

            check (likeness.inside > 1000000 && likeness.differing == 0,
                   "the first page, drawn at the design's own size, is the design: inside its frame not one pixel differs  ("
                       + juce::String (likeness.differing) + " of " + juce::String (likeness.inside) + " differ, by "
                       + juce::String (likeness.worst) + " at most)");
            check (likeness.outside > 10000 && likeness.spilt == 0 && likeness.onTheOutline < 400,
                   "and outside that frame nothing is drawn: the window has the frame's shape, cut corners and all  ("
                       + juce::String (likeness.spilt) + " of " + juce::String (likeness.outside) + " pixels drawn on; the outline runs through "
                       + juce::String (likeness.onTheOutline) + ")");

            // That outline is the design's frame: outside it the design shows nothing but the
            // dark sheet it was presented on, and the frame's own line runs just inside it
            {
                const auto outline = Assets::outline();
                const auto frame = Assets::frame().toNearestInt();

                juce::Image cover (juce::Image::SingleChannel, window.getWidth(), window.getHeight(), true, juce::SoftwareImageType());
                {
                    juce::Graphics g (cover);
                    g.setColour (juce::Colours::white);
                    g.fillPath (outline);
                }

                const auto light = [&] (int x, int y)
                {
                    const auto c = design.getPixelAt (window.getX() + x, window.getY() + y);
                    return juce::jmax ((int) c.getRed(), (int) c.getGreen(), (int) c.getBlue());
                };

                int brightestOutside = 0;

                if (design.isValid())
                    for (int y = 0; y < window.getHeight(); ++y)
                        for (int x = 0; x < window.getWidth(); ++x)
                            if (cover.getPixelAt (x, y).getAlpha() == 0)
                                brightestOutside = juce::jmax (brightestOutside, light (x, y));

                // how light the design is on average along a line (the corners left out)
                const auto along = [&] (bool upright, int at)
                {
                    long long sum = 0;
                    int count = 0;

                    if (upright)
                        for (int y = frame.getY() + 40; y < frame.getBottom() - 40; ++y, ++count)
                            sum += light (at, y);
                    else
                        for (int x = frame.getX() + 40; x < frame.getRight() - 40; ++x, ++count)
                            sum += light (x, at);

                    return count > 0 ? (double) sum / (double) count : 0.0;
                };

                // (the two pixels just inside the outline against the two just outside it)
                const double leftIn   = juce::jmax (along (true, frame.getX()), along (true, frame.getX() + 1)),            leftOut   = juce::jmax (along (true, frame.getX() - 1), along (true, frame.getX() - 2));
                const double rightIn  = juce::jmax (along (true, frame.getRight() - 1), along (true, frame.getRight() - 2)),  rightOut  = juce::jmax (along (true, frame.getRight()), along (true, frame.getRight() + 1));
                const double topIn    = juce::jmax (along (false, frame.getY()), along (false, frame.getY() + 1)),            topOut    = juce::jmax (along (false, frame.getY() - 1), along (false, frame.getY() - 2));
                const double bottomIn = juce::jmax (along (false, frame.getBottom() - 1), along (false, frame.getBottom() - 2)), bottomOut = juce::jmax (along (false, frame.getBottom()), along (false, frame.getBottom() + 1));

                check (design.isValid() && brightestOutside < 40,
                       "outside the window's outline the design shows only the dark sheet it lies on  (brightest there: "
                           + juce::String (brightestOutside) + " of 255)");
                // (the sheet is not quite black on the right, where the design lets a red glow fall on it)
                const auto isTheLine = [] (double in, double out) { return in > 25.0 && out < 16.0 && in > 3.0 * out; };
                const auto both = [] (double in, double out) { return juce::String (juce::roundToInt (in)) + " | " + juce::String (juce::roundToInt (out)); };

                check (design.isValid() && isTheLine (leftIn, leftOut) && isTheLine (rightIn, rightOut) && isTheLine (topIn, topOut) && isTheLine (bottomIn, bottomOut),
                       "and the line of the design's frame runs just inside it, on all four sides  (how light, inside | outside: left "
                           + both (leftIn, leftOut) + ", right " + both (rightIn, rightOut) + ", top " + both (topIn, topOut) + ", bottom " + both (bottomIn, bottomOut) + ")");
            }

            snapshot (shell, "1-welcome-at-the-designs-size", own);

            // The pictures behind the later pages are the design with something painted out,
            // and nothing else about it changed
            const auto& noText    = assets.getPicture (Assets::Picture::noText);
            const auto& noButtons = assets.getPicture (Assets::Picture::noButtons);
            const auto& finish    = assets.getPicture (Assets::Picture::finish);
            const juce::Rectangle<int> text (846, 224, 830, 252), buttons (834, 524, 832, 126);

            const auto sameOutside = [&] (const juce::Image& picture, juce::Rectangle<int> patch, juce::Rectangle<int> otherPatch)
            {
                if (! (picture.isValid() && design.isValid()))
                    return false;

                for (int y = 0; y < window.getHeight(); ++y)
                    for (int x = 0; x < window.getWidth(); ++x)
                        if (! patch.contains (x, y) && ! otherPatch.contains (x, y)
                            && picture.getPixelAt (x, y) != design.getPixelAt (window.getX() + x, window.getY() + y).withAlpha (1.0f))
                            return false;

                return true;
            };

            const auto brightest = [] (const juce::Image& picture, juce::Rectangle<int> part)
            {
                int most = 0;

                for (int y = part.getY(); y < part.getBottom(); ++y)
                    for (int x = part.getX(); x < part.getRight(); ++x)
                        most = juce::jmax (most, (int) picture.getPixelAt (x, y).getRed(), (int) picture.getPixelAt (x, y).getGreen());

                return most;
            };

            check (sameOutside (noText, text, {}) && sameOutside (noButtons, text, buttons) && sameOutside (finish, text, buttons),
                   "the pictures behind the later pages are the design everywhere but where its words and buttons were");

            const juce::Rectangle<int> words (850, 228, 820, 244), cancelButton (1300, 556, 354, 62), redButton (880, 560, 360, 54);

            check (brightest (noText, words) < 30 && brightest (noButtons, words) < 30 && brightest (finish, words) < 30
                    && brightest (noText, cancelButton) > 200 && brightest (noButtons, cancelButton) < 30 && brightest (finish, cancelButton) < 30,
                   "there the words are gone, and the Cancel button where a page has none  (brightest left of the words: "
                       + juce::String (brightest (noText, words)) + ")");

            // the red button without its label is all red
            int darkest = 255;

            for (int y = redButton.getY(); y < redButton.getBottom(); ++y)
                for (int x = redButton.getX(); x < redButton.getRight(); ++x)
                    darkest = juce::jmin (darkest, (int) finish.getPixelAt (x, y).getRed());

            check (darkest > 220 && brightest (noButtons, redButton) < 30,
                   "the red button is bare where Finish and Close are set on it, and gone where a page has no button  (its darkest red: "
                       + juce::String (darkest) + ")");

            // the first page's words set in type, to be looked at next to the design's own
            view.setFirstPageInType (true);
            snapshot (shell, "1-welcome-in-type", own);
            view.setFirstPageInType (false);
        }

        click (*next);
        check (view.getPage() == InstallerView::Page::location && tickAu->isVisible() && tickVst3->isVisible()
                && tickAu->getToggleState() && tickVst3->getToggleState() && next->isEnabled() && cancel->isVisible(),
               "Next leads to the install location, where every folder is ticked to begin with");
        snapshot (shell, "2-location");
        snapshot (shell, "2-location-at-100-percent", 1.0f);

        click (*tickAu);
        click (*tickVst3);
        check (! tickAu->getToggleState() && ! tickVst3->getToggleState() && ! next->isEnabled(), "with every tick removed, Next is switched off");
        snapshot (shell, "2b-location-nothing-ticked");
        click (*next);
        check (view.getPage() == InstallerView::Page::location && ! root.exists(), "and clicking it all the same does nothing");

        click (*tickVst3);
        check (tickVst3->getToggleState() && next->isEnabled() && view.getTargets()[1].selected && ! view.getTargets()[0].selected,
               "ticking one again switches Next back on");
        click (*tickAu);

        click (*next);
        check (view.getPage() == InstallerView::Page::installing && ! next->isVisible() && ! cancel->isVisible() && ! close->isEnabled(),
               "the second Next starts the installation; while it runs there is nothing to click");

        bool pictured = false;
        double shownBefore = -1.0;
        bool monotonic = true;
        const auto started = juce::Time::getMillisecondCounter();

        while (view.getPage() == InstallerView::Page::installing && juce::Time::getMillisecondCounter() - started < 20000)
        {
            view.dismiss();                 // what the close button and Escape do (somebody impatient)
            letRun (15);
            monotonic = monotonic && view.getShownProgress() >= shownBefore;
            shownBefore = view.getShownProgress();

            if (! pictured && view.getShownProgress() > 0.55)
            {
                snapshot (shell, "3-installing");
                pictured = true;
            }
        }

        const auto took = juce::Time::getMillisecondCounter() - started;
        check (view.getPage() == InstallerView::Page::finished && view.getReport().succeeded && doneCalls == 0,
               "it ends on the finished page; asking it to close meanwhile did not interrupt it");
        check (monotonic && took >= 700 && took < 5000, "the bar fills steadily and takes long enough to be seen  (" + juce::String ((int) took) + " ms)");
        check (root.getChildFile ("VST3/D.A AURA.vst3/Contents/x86_64-win/D.A AURA.vst3").loadFileAsString() == binaryText
                && root.getChildFile ("Components/D.A AURA.component/Contents/MacOS/D.A AURA").loadFileAsString() == binaryText,
               "and the plug-ins are where the page said they would go");
        check (next->isVisible() && next->getButtonText() == "Finish" && ! cancel->isVisible() && close->isEnabled(),
               "the finished page has one button: Finish");
        snapshot (shell, "4-finished");
        snapshot (shell, "4-finished-at-100-percent", 1.0f);

        click (*next);
        check (doneCalls == 1, "Finish closes the installer");
        check (pagesSeen == std::vector<InstallerView::Page> { InstallerView::Page::location, InstallerView::Page::installing, InstallerView::Page::finished },
               "the pages came in order: location, installing, finished");

        // other sizes of the window
        shell.setSize (686, 270);
        snapshot (shell, "5-smallest", 1.0f);
        shell.setSize (1470, 579);
        snapshot (shell, "6-larger", 1.0f);
        shell.setSize (InstallerView::designWidth, InstallerView::designHeight);
        snapshot (shell, "7-at-100-percent", 1.0f);

        // the buttons under the pointer, and held down
        {
            int ignored = 0;
            InstallerShell other (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error),
                                                                   targetsIn (scratch.getChildFile ("pointer")), juce::String()));
            other.getView().onDone = [&] { ++ignored; };

            const auto button = [&] (const char* buttonID) { return dynamic_cast<juce::Button*> (other.getView().findChildWithID (buttonID)); };

            button ("next")->setState (juce::Button::buttonOver);
            button ("close")->setState (juce::Button::buttonOver);
            snapshot (other, "9a-pointer-on-next-and-close");

            button ("next")->setState (juce::Button::buttonDown);
            button ("close")->setState (juce::Button::buttonNormal);
            button ("cancel")->setState (juce::Button::buttonOver);
            button ("minimise")->setState (juce::Button::buttonOver);
            snapshot (other, "9b-next-held-down-pointer-on-cancel-and-minimise");
        }

        // Cancel and the close button
        {
            int done = 0, putAway = 0;
            InstallerView other (assets, openZip (makeZip (pluginEntries()), error), targetsIn (scratch.getChildFile ("cancelled")), juce::String());
            other.onDone = [&] { ++done; };
            other.onMinimise = [&] { ++putAway; };
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("minimise")));
            check (putAway == 1 && done == 0 && other.getPage() == InstallerView::Page::welcome,
                   "the minimise button asks for the window to be put away, and does nothing else");
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("cancel")));
            check (done == 1 && ! scratch.getChildFile ("cancelled").exists(), "Cancel on the welcome page closes the installer and installs nothing");
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("next")));
            click (*dynamic_cast<juce::Button*> (other.findChildWithID ("close")));
            check (done == 2 && ! scratch.getChildFile ("cancelled").exists(), "so does the close button on the location page");
        }

        // the page as it is on Windows: one folder
        {
            std::vector<InstallTarget> windows { { "VST3", "VST3", scratch.getChildFile ("windows/VST3"), "C:\\Program Files\\Common Files\\VST3", true } };
            InstallerShell windowsShell (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), windows, juce::String()));
            windowsShell.getView().advance();
            check (windowsShell.getView().findChildWithID ("target-VST3") != nullptr && windowsShell.getView().findChildWithID ("target-AU") == nullptr,
                   "with one target there is one tick box");
            snapshot (windowsShell, "2-location-windows");
        }

        // when the installation fails
        {
            const auto blocked = scratch.getChildFile ("page-blocked");
            blocked.createDirectory();
            blocked.getChildFile ("VST3").replaceWithText ("a file where the folder should be");
            std::vector<InstallTarget> one { { "VST3", "VST3", blocked.getChildFile ("VST3"), "C:\\Program Files\\Common Files\\VST3", true } };

            int done = 0;
            InstallerShell failing (std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error), one, juce::String()));
            failing.getView().onDone = [&] { ++done; };
            failing.getView().advance();
            failing.getView().advance();

            for (int i = 0; i < 200 && failing.getView().getPage() == InstallerView::Page::installing; ++i)
                letRun (15);

            auto* button = dynamic_cast<juce::Button*> (failing.getView().findChildWithID ("next"));
            check (failing.getView().getPage() == InstallerView::Page::failed && ! failing.getView().getReport().succeeded
                    && button->getButtonText() == "Close" && button->isVisible(),
                   "a failed installation ends on a page that says so, with a Close button");
            snapshot (failing, "4-failed");
            click (*button);
            check (done == 1, "which closes the installer");
        }

        // the window around it (not put on a screen here: there is none)
        {
            int done = 0;
            auto windowView = std::make_unique<InstallerView> (assets, openZip (makeZip (pluginEntries()), error),
                                                               targetsIn (scratch.getChildFile ("window")), juce::String());
            windowView->onDone = [&] { ++done; };
            InstallerWindow window ("D.A AURA Installer", std::make_unique<InstallerShell> (std::move (windowView)));
            window.setVisible (true);

            const int w = window.getWidth(), h = window.getHeight();
            const auto at = [&] (int x, int y) { return window.getComponentAt (x, y); };
            const auto isEdge = [&] (int x, int y) { return dynamic_cast<juce::ResizableBorderComponent*> (at (x, y)) != nullptr; };
            const auto idAt = [&] (juce::Component* c) { return at (window.getLocalArea (c, c->getLocalBounds()).getCentreX(),
                                                                     window.getLocalArea (c, c->getLocalBounds()).getCentreY()); };

            auto* windowNext  = window.getView().findChildWithID ("next");
            auto* windowClose = window.getView().findChildWithID ("close");

            check (w == InstallerView::designWidth && h == InstallerView::designHeight && window.getShell().getBounds() == window.getLocalBounds(),
                   "the window is made for the view's size, and the view fills all of it");
            check (! window.isOpaque() && ! window.getShell().isOpaque() && ! window.getView().isOpaque() && ! window.isDropShadowEnabled(),
                   "the window is see-through wherever the view draws nothing, and asks for no shadow");

            // The whole window as it would be put on a screen, edges and all: outside the
            // frame's outline there is nothing of it but its trace, inside it is solid
            {
                const auto whole = drawnAt (window, 1.0f);

                juce::Image cover (juce::Image::SingleChannel, w, h, true, juce::SoftwareImageType());
                {
                    juce::Graphics g (cover);
                    g.setColour (juce::Colours::white);
                    g.fillPath (InstallerView::getOutline());
                }

                const int trace = (int) (InstallerWindow::traceAroundTheFrame >> 24);
                int most = 0, least = 255, around = 0;

                for (int y = 0; y < h; ++y)
                {
                    for (int x = 0; x < w; ++x)
                    {
                        const int covered = cover.getPixelAt (x, y).getAlpha();
                        const int alpha = whole.getPixelAt (x, y).getAlpha();

                        if (covered == 0)           { most = juce::jmax (most, alpha); ++around; }
                        else if (covered == 255)    { least = juce::jmin (least, alpha); }
                    }
                }

                check (around > 5000 && most <= trace && trace <= 1 && least == 255,
                       "the whole window, its edges included, shows nothing around the frame  (of 255: at most " + juce::String (most)
                           + " on " + juce::String (around) + " pixels around the frame, at least " + juce::String (least) + " within it)");
            }

            const int flags = window.getDesktopWindowStyleFlags();
            check ((flags & juce::ComponentPeer::windowHasTitleBar) == 0 && (flags & juce::ComponentPeer::windowIsResizable) != 0
                    && (flags & juce::ComponentPeer::windowHasMinimiseButton) != 0 && (flags & juce::ComponentPeer::windowHasCloseButton) != 0
                    && (flags & juce::ComponentPeer::windowHasDropShadow) == 0,
                   "it asks the system for no title bar and no shadow, and to be resizable, minimisable and closable");

            // (where the frame's line is drawn, in the window: the outline's sides)
            const auto frame = InstallerView::getFrame().toNearestInt();
            const int fl = frame.getX(), ft = frame.getY(), fr = frame.getRight() - 1, fb = frame.getBottom() - 1;

            check (isEdge (2, h / 2) && isEdge (w - 3, h / 2) && isEdge (w / 2, 2) && isEdge (w / 2, h - 3)
                    && isEdge (2, 2) && isEdge (w - 3, 2) && isEdge (2, h - 3) && isEdge (w - 3, h - 3),
                   "the mouse finds an edge to resize it by on all four sides and at all four corners");
            check (isEdge (fl, h / 2) && isEdge (fr, h / 2) && isEdge (w / 2, ft) && isEdge (w / 2, fb)
                    && isEdge (fl + 2, h / 2) && isEdge (fr - 2, h / 2) && isEdge (w / 2, ft + 2) && isEdge (w / 2, fb - 2),
                   "that edge reaches from the window's own edge to the frame that is drawn, and the frame's line is part of it");

            // The places the self-test takes the frame by: each is on the frame (inside the
            // outline, and no further inside than its line is wide), and an edge to the mouse
            using Part = InstallerWindow::FramePart;
            const auto outline = InstallerView::getOutline();

            const auto isOnTheFrame = [&] (juce::Point<int> place, float scale)
            {
                const auto inUnits = place.toFloat() / scale;
                juce::Point<float> nearest;
                outline.getNearestPoint (inUnits, nearest);
                const float distance = scale * nearest.getDistanceFrom (inUnits);

                return outline.contains (inUnits) && distance >= 0.5f && distance <= 3.0f && isEdge (place.x, place.y);
            };

            const auto corner = window.getPointOnFrame (Part::lowerRightCorner);

            check (isOnTheFrame (window.getPointOnFrame (Part::leftSide), 1.0f) && isOnTheFrame (window.getPointOnFrame (Part::top), 1.0f)
                    && isOnTheFrame (corner, 1.0f) && corner.x < fr - 3 && corner.y < fb - 3,
                   "the frame can be taken hold of on its line: on its sides, and at a corner where it is cut off  (the lower right one at "
                       + corner.toString() + " of " + juce::String (w) + " x " + juce::String (h) + ")");

            const int reach = juce::roundToInt (InstallerWindow::edgeReach);
            check (at (w / 4, h / 2) == &window && at (w / 2 + 40, 120) == &window
                    && ! isEdge (fl + reach + 2, h / 2) && ! isEdge (fr - reach - 2, h / 2)
                    && ! isEdge (w / 2, ft + reach + 2) && ! isEdge (w / 2, fb - reach - 2),
                   "everywhere else that is not a button, the mouse takes hold of the window itself (to move it)");
            check (windowNext != nullptr && windowClose != nullptr && idAt (windowNext) == windowNext && idAt (windowClose) == windowClose,
                   "and on a button, the button");

            // other sizes: as the mouse would ask for them
            const auto proportional = [] (juce::Rectangle<int> b)
            {
                return std::abs ((double) b.getWidth() / (double) b.getHeight() - (double) InstallerView::designWidth / (double) InstallerView::designHeight) < 0.02;
            };

            auto* limits = window.getConstrainer();
            const auto asked = [&] (int width, int height)
            {
                juce::Rectangle<int> b (0, 0, width, height);
                limits->checkBounds (b, window.getBounds(), { 0, 0, 10000, 10000 }, false, false, true, true);
                return b;
            };

            const auto wide = asked (1400, 300), tiny = asked (100, 100), huge = asked (5000, 5000);
            check (limits != nullptr && proportional (wide) && proportional (tiny) && proportional (huge) && wide.getWidth() > w,
                   "resized, it keeps its proportions  (asked for 1400 x 300, it becomes " + juce::String (wide.getWidth()) + " x " + juce::String (wide.getHeight()) + ")");
            check (tiny.getWidth() == 686 && tiny.getHeight() == 270 && huge.getWidth() == 2450 && huge.getHeight() == 965,
                   "from 70 % to 250 % of its size  (" + juce::String (tiny.getWidth()) + " x " + juce::String (tiny.getHeight())
                       + " to " + juce::String (huge.getWidth()) + " x " + juce::String (huge.getHeight()) + ")");

            // other sizes: the smallest, the one the build machines' screens give it, and a large one
            bool frameFollows = true;

            for (const int width : { 686, 942, 1470, 2450 })
            {
                window.setSize (width, juce::roundToInt (width * (double) h / (double) w));
                const float scale = window.getScale();

                frameFollows = frameFollows && isOnTheFrame (window.getPointOnFrame (Part::leftSide), scale)
                                            && isOnTheFrame (window.getPointOnFrame (Part::top), scale)
                                            && isOnTheFrame (window.getPointOnFrame (Part::lowerRightCorner), scale)
                                            && isEdge (2, window.getHeight() / 2) && isEdge (window.getWidth() / 2, window.getHeight() - 3);
            }

            check (frameFollows, "at other sizes (70 %, 96 %, 150 %, 250 %) the frame is taken hold of in the same places");

            window.setSize (1470, 579);
            check (window.getShell().getBounds() == window.getLocalBounds() && isEdge (1468, 577) && idAt (windowNext) == windowNext,
                   "at another size the view still fills it, and edges and buttons are where they are drawn");
            snapshot (window, "8-window-at-150-percent", 1.0f);

            // the keyboard
            check (window.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)) && window.getView().getPage() == InstallerView::Page::location,
                   "Return does what the main button does");
            check (window.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)) && done == 1 && ! scratch.getChildFile ("window").exists(),
                   "Escape closes the installer");
            window.userTriedToCloseWindow();
            check (done == 2, "and so does closing the window from outside (the taskbar, the Dock)");
        }

        // an installer without its payload
        {
            InstallerShell broken (std::make_unique<InstallerView> (assets, nullptr, targets,
                                                                     "This installer is incomplete: the plug-in files are missing from it.\nPlease download it again."));
            check (broken.getView().getPage() == InstallerView::Page::failed
                    && dynamic_cast<juce::Button*> (broken.getView().findChildWithID ("next"))->getButtonText() == "Close",
                   "an installer whose plug-in files are missing says so straight away");
            snapshot (broken, "0-no-payload");
        }
    }

    scratch.deleteRecursively();

    std::printf ("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
