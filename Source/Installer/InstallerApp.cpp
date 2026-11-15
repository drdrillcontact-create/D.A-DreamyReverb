// D.A AURA Installer
//
// A small standalone GUI app that installs the D.A AURA plugin bundle(s).
//
//   Page 1 (Welcome)  : the supplied welcome image (Installer.png) drawn to fit
//                       the window, with interactive Next (red) and Cancel
//                       (dark) click-areas placed exactly over the button areas
//                       of that image, so the screen looks identical to the
//                       reference screenshot.
//   Page 2 (Install)  : shows the install folder (default
//                       C:\Program Files\Common Files\VST3 on Windows,
//                       /Library/Audio/Plug-Ins on macOS) with a checkbox that
//                       is checked by default, plus Back / Next.
//   Page 3 (Done)     : a Finish button.
//
// The plugin is embedded INSIDE the installer so the app is fully self-contained
// and works when extracted and run from anywhere:
//   - Windows: the VST3 dll is appended to the end of the .exe behind a marker;
//     the installer reads its own executable, extracts the dll to a temp file
//     and copies it into the chosen install folder.
//   - macOS:   the .component / .vst3 bundles live in Contents/plugins inside
//     the .app bundle and are copied from there (moving the .app keeps them).

#include <JuceHeader.h>
#include <cstring>
#include <functional>
#include <memory>

namespace
{
    // fractions of the welcome image (1857 x 847) where the two buttons are drawn
    constexpr float nextX = 0.496f, nextY = 0.753f, nextW = 0.212f, nextH = 0.082f;
    constexpr float cancelX = 0.729f, cancelY = 0.753f, cancelW = 0.194f, cancelH = 0.082f;

    // marker that precedes the embedded plugin zip appended to the .exe
    const char pluginMarker[] = "DAAURAINSTALLER_PLUGIN_ZIP_v1";

    juce::Image loadWelcomeImage()
    {
        return juce::ImageCache::getFromMemory (
            BinaryData::installerwelcome_png,
            (int) BinaryData::installerwelcome_pngSize);
    }

    // rectangle the image is drawn into so it fits fully (no cropping), centred
    juce::Rectangle<int> fitRect (const juce::Rectangle<int>& area, const juce::Image& img)
    {
        if (! img.isValid())
            return area;
        const float s = (float) juce::jmin (area.getWidth()  / (float) img.getWidth(),
                                            area.getHeight() / (float) img.getHeight());
        const int w = (int) (img.getWidth()  * s);
        const int h = (int) (img.getHeight() * s);
        return { area.getX() + (area.getWidth()  - w) / 2,
                 area.getY() + (area.getHeight() - h) / 2, w, h };
    }

    // copy a file or a whole directory tree from src to dest
    bool copyRecursive (const juce::File& src, const juce::File& dest)
    {
        if (src.isDirectory())
        {
            if (! dest.createDirectory())
                return false;

            bool ok = true;
            for (const auto& f : src.findChildFiles (juce::File::findFilesAndDirectories, true))
                ok = copyRecursive (f, dest.getChildFile (f.getFileName())) && ok;
            return ok;
        }

        return src.copyFileTo (dest);
    }

    // Windows: extract the embedded plugin zip from our own executable into a
    // temp directory and return that directory (it contains the .vst3 bundle).
    // Returns an invalid File on failure.
    juce::File extractEmbeddedPlugin()
    {
        const juce::File exe = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::currentExecutableFile);

        if (! exe.existsAsFile())
            return juce::File();

        juce::MemoryBlock data;
        if (! exe.loadFileAsData (data))
            return juce::File();

        const char* bytes = static_cast<const char*> (data.getData());
        const size_t len  = data.getSize();
        const size_t mlen = sizeof (pluginMarker) - 1;

        // find the LAST occurrence of the marker (trailing bytes are the zip)
        size_t pos = (size_t) -1;
        for (size_t i = (len > mlen ? len - mlen : 0); i-- > 0;)
        {
            if (std::memcmp (bytes + i, pluginMarker, mlen) == 0)
            {
                pos = i;
                break;
            }
        }

        if (pos == (size_t) -1)
            return juce::File();

        const size_t zipStart = pos + mlen;
        if (zipStart >= len)
            return juce::File();

        const juce::File tempDir = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::tempDirectory)
            .getChildFile ("DA AURA Installer Extracted");

        tempDir.deleteRecursively();
        if (! tempDir.createDirectory())
            return juce::File();

        // write the zip bytes to a temp file, then uncompress
        const juce::File zipFile = tempDir.getChildFile ("plugin.zip");
        {
            juce::FileOutputStream out (zipFile);
            if (! out.opened() || ! out.write (bytes + zipStart, len - zipStart))
                return juce::File();
        }

        juce::ZipFile zip (zipFile);
        if (zip.getNumEntries() == 0)
            return juce::File();

        const juce::File outDir = tempDir.getChildFile ("bundle");
        if (! outDir.createDirectory())
            return juce::File();

        if (! zip.uncompressTo (outDir, true))
            return juce::File();

        return outDir;
    }

    // macOS: the bundles sit in Contents/plugins inside the .app bundle
    juce::File macPluginsDir()
    {
#if JUCE_MAC
        const juce::File exe = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::currentExecutableFile);
        // exe is <app>/Contents/MacOS/<name> -> Contents/plugins
        return exe.getParentDirectory().getParentDirectory().getChildFile ("plugins");
#else
        return juce::File();
#endif
    }

    // place the plugin bundle(s) into the target folder.
    // On macOS, if the target looks like a Plug-Ins root, bundles are sorted
    // into Components/ and VST3/ so the host finds them.
    juce::String installPlugins (const juce::File& pluginsDir, const juce::File& target)
    {
#if JUCE_MAC
        const bool macPlugInsRoot = target.getFullPathName().contains ("Plug-Ins");
#else
        const bool macPlugInsRoot = false;
#endif

        if (! target.exists() && ! target.createDirectory())
            return "Could not create the install folder.";

        int copied = 0;

        if (pluginsDir.isDirectory())
        {
            for (const auto& f : pluginsDir.findChildFiles (juce::File::findFilesAndDirectories, true))
            {
                juce::File dest = target;

                if (macPlugInsRoot)
                {
                    if (f.getFileName().endsWith (".component"))
                        dest = target.getChildFile ("Components");
                    else if (f.getFileName().endsWith (".vst3"))
                        dest = target.getChildFile ("VST3");
                }

                if (! dest.exists() && ! dest.createDirectory())
                    return "Could not create " + dest.getFullPathName() + ".";

                if (! copyRecursive (f, dest.getChildFile (f.getFileName())))
                    return "Could not copy " + f.getFileName() + ".";

                ++copied;
            }
        }
        else if (pluginsDir.existsAsFile())
        {
            // single file (Windows extracted dll)
            if (! copyRecursive (pluginsDir, target.getChildFile (pluginsDir.getFileName())))
                return "Could not copy the plugin.";
            ++copied;
        }

        return copied == 0 ? "No plugin files found to install."
                           : juce::String();
    }
}

// invisible clickable rectangle used over the welcome image's static buttons
class ClickArea : public juce::Component
{
public:
    ClickArea (std::function<void()> onClick_) : onClick (std::move (onClick_)) {}

    void mouseDown (const juce::MouseEvent&) override { onClick(); }

private:
    std::function<void()> onClick;
};

class InstallerComponent : public juce::Component
{
public:
    InstallerComponent()
    {
        bg = loadWelcomeImage();

        nextArea = std::make_unique<ClickArea> ([this] { onNext(); });
        cancelArea = std::make_unique<ClickArea> ([this] { quit(); });
        addAndMakeVisible (*nextArea);
        addAndMakeVisible (*cancelArea);

        backBtn.setButtonText ("Back");
        styleDarkButton (backBtn);
        addAndMakeVisible (backBtn);

        installBtn.setButtonText ("Next");
        styleRedButton (installBtn);
        addAndMakeVisible (installBtn);

        finishBtn.setButtonText ("Finish");
        styleDarkButton (finishBtn);
        addAndMakeVisible (finishBtn);

        pathEdit.setMultiLine (false);
        pathEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141414));
        pathEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        pathEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff444444));
        addAndMakeVisible (pathEdit);

        check.setButtonText ("Install to this location");
        check.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        check.setToggleState (true, juce::NotificationType::dontSendNotification);
        addAndMakeVisible (check);

        status.setJustificationType (juce::Justification::centred);
        status.setColour (juce::Label::textColourId, juce::Colours::white);
        status.setFont (juce::Font (14.0f, juce::Font::bold));
        addAndMakeVisible (status);

        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        title.setFont (juce::Font (22.0f, juce::Font::bold));
        addAndMakeVisible (title);

        backBtn.onClick   = [this] { page = Welcome; resized(); };
        installBtn.onClick = [this] { doInstall(); };
        finishBtn.onClick  = [this] { quit(); };
    }

    void resized() override
    {
        const auto area = getLocalBounds();

        if (page == Welcome)
        {
            // the welcome image is drawn to fit the window; the click-areas are
            // mapped onto the drawn image rect so they sit exactly on the buttons
            const auto r = fitRect (area, bg);
            nextArea->setBounds  ((int) (r.getX() + nextX   * r.getWidth()),
                                  (int) (r.getY() + nextY   * r.getHeight()),
                                  (int) (nextW   * r.getWidth()),
                                  (int) (nextH   * r.getHeight()));
            cancelArea->setBounds((int) (r.getX() + cancelX * r.getWidth()),
                                  (int) (r.getY() + cancelY * r.getHeight()),
                                  (int) (cancelW * r.getWidth()),
                                  (int) (cancelH * r.getHeight()));

            backBtn.setVisible (false);
            installBtn.setVisible (false);
            finishBtn.setVisible (false);
            pathEdit.setVisible (false);
            check.setVisible (false);
            status.setVisible (false);
            title.setVisible (false);
        }
        else if (page == Install)
        {
            const auto cx = area.getCentreX();
            const int fieldW = 560, fieldH = 34;
            pathEdit.setBounds (cx - fieldW / 2, area.getY() + 200, fieldW, fieldH);
            check.setBounds     (cx - fieldW / 2, area.getY() + 248, 260, 24);
            title.setBounds     (area.getX(), area.getY() + 70, area.getWidth(), 34);
            status.setBounds    (area.getX(), area.getY() + 300, area.getWidth(), 28);

            const int bw = 140, bh = 40;
            backBtn.setBounds   (cx - bw - 20, area.getY() + area.getHeight() - 90, bw, bh);
            installBtn.setBounds(cx + 20,           area.getY() + area.getHeight() - 90, bw, bh);

            nextArea->setVisible (false);
            cancelArea->setVisible (false);
            finishBtn.setVisible (false);
        }
        else // Done
        {
            const auto cx = area.getCentreX();
            title.setBounds     (area.getX(), area.getY() + 70, area.getWidth(), 34);
            status.setBounds    (area.getX(), area.getY() + 140, area.getWidth(), 40);
            finishBtn.setBounds (cx - 75, area.getY() + area.getHeight() - 110, 150, 44);

            nextArea->setVisible (false);
            cancelArea->setVisible (false);
            backBtn.setVisible (false);
            installBtn.setVisible (false);
            pathEdit.setVisible (false);
            check.setVisible (false);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds();

        if (page == Welcome)
        {
            g.fillRect (area);
            if (bg.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.drawImage (bg, fitRect (area, bg).toFloat());
            }
            return;
        }

        paintPanel (g, area);
    }

private:
    enum Page { Welcome, Install, Done };
    Page page = Welcome;

    juce::Image bg;
    std::unique_ptr<ClickArea> nextArea, cancelArea;
    juce::TextButton backBtn, installBtn, finishBtn;
    juce::TextEditor pathEdit;
    juce::ToggleButton check;
    juce::Label status, title;

    static void styleRedButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff8a0306));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd7050c));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    }

    static void styleDarkButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1a1a1a));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a2a2a));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    }

    void paintPanel (juce::Graphics& g, const juce::Rectangle<int>& area)
    {
        g.setColour (juce::Colour (0xff020202));
        g.fillRect (area);

        // thin red top bar for branding
        g.setColour (juce::Colour (0xffd7050c));
        g.fillRect (area.withHeight (4));
    }

    void onNext()
    {
        if (page != Welcome)
            return;

        page = Install;
        pathEdit.setText (juce::String (DREAMY_INSTALLER_DEFAULT_DIR), false);
        resized();
    }

    void doInstall()
    {
        const juce::File target (pathEdit.getText().isNotEmpty() ? pathEdit.getText() : DREAMY_INSTALLER_DEFAULT_DIR);
        juce::File pluginsDir;

#if JUCE_MAC
        pluginsDir = macPluginsDir();
#else
        // Windows: extract the embedded plugin bundle from our own exe
        pluginsDir = extractEmbeddedPlugin();
        if (! pluginsDir.isDirectory())
        {
            page = Done;
            title.setText ("Installation failed", juce::dontSendNotification);
            status.setText ("The embedded plugin could not be read. Re-download the installer.",
                            juce::dontSendNotification);
            resized();
            return;
        }
#endif

        const juce::String err = installPlugins (pluginsDir, target);

        page = Done;
        title.setText ("Installation complete", juce::dontSendNotification);
        status.setText (err.isEmpty()
                            ? "D.A AURA was installed to " + target.getFullPathName()
                            : "Installation failed: " + err,
                        juce::dontSendNotification);
        resized();
    }

    void quit()
    {
        juce::JUCEApplication::getInstance()->quit();
    }
};

class InstallerWindow : public juce::DocumentWindow
{
public:
    InstallerWindow()
        : DocumentWindow ("D.A AURA Installer",
                          juce::Colour (0xff020202),
                          juce::DocumentWindow::allButtons)
    {
        setContentOwned (new InstallerComponent(), true);
        setResizable (true, true);
        setResizeLimits (720, 480, 1600, 1100);
        centreWithSize (760, 520);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class DreamyInstallerApp : public juce::JUCEApplication
{
public:
    DreamyInstallerApp() {}
    ~DreamyInstallerApp() override = default;

    const juce::String getApplicationName() override     { return "D.A AURA Installer"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
        window = std::make_unique<InstallerWindow>();
    }

    void shutdown() override
    {
        window = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
    }

private:
    std::unique_ptr<InstallerWindow> window;
};

START_JUCE_APPLICATION (DreamyInstallerApp)
