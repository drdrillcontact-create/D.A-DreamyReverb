// D.A AURA Installer
//
// A small standalone GUI app that installs the D.A AURA plugin bundle(s).
//
//   Page 1 (Welcome)  : the supplied welcome image as the full background, with
//                       interactive Next (red) and Cancel (gray) buttons placed
//                       over the button areas of that image.
//   Page 2 (Install)  : shows the install folder (default
//                       C:\Program Files\Common Files\VST3 on Windows,
//                       /Library/Audio/Plug-Ins on macOS) with a checkbox that
//                       is checked by default, plus Next / Back.
//   Page 3 (Done)     : a Finish button.
//
// The plugin bundles are read from a "plugins" folder that sits next to the
// built application (the workflow places them there before packaging) and are
// copied into the chosen install folder.

#include <JuceHeader.h>
#include <memory>

namespace
{
    // fractions of the welcome image (1857 x 847) where the two buttons are drawn
    constexpr float nextX = 0.496f, nextY = 0.752f, nextW = 0.212f, nextH = 0.084f;
    constexpr float cancelX = 0.729f, cancelY = 0.753f, cancelW = 0.196f, cancelH = 0.081f;

    juce::Image loadWelcomeImage()
    {
        return juce::ImageCache::getFromMemory (
            BinaryData::installerwelcome_png,
            (int) BinaryData::installerwelcome_pngSize);
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

    // place plugin bundles from <exeDir>/plugins into the target folder.
    // On macOS, if the target looks like a Plug-Ins root, bundles are sorted
    // into Components/ and VST3/ so the host finds them.
    juce::String installPlugins (const juce::File& pluginsDir, const juce::File& target)
    {
        if (! pluginsDir.isDirectory())
            return "Plugin files were not found next to the installer.";

#if JUCE_MAC || JUCE_IOS
        const bool macPlugInsRoot = target.getFullPathName().contains ("Plug-Ins");
#else
        const bool macPlugInsRoot = false;
#endif

        if (! target.exists() && ! target.createDirectory())
            return "Could not create the install folder.";

        int copied = 0;
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

        return copied == 0 ? "No plugin files found to install."
                           : juce::String();
    }
}

class InstallerComponent : public juce::Component
{
public:
    InstallerComponent()
    {
        bg = loadWelcomeImage();

        nextBtn.setButtonText ("Next");
        nextBtn.setColour (juce::TextButton::buttonOnColourId,   juce::Colour (0xffd7050c));
        nextBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff8a0306));
        nextBtn.setColour (juce::TextButton::textColourOffId,       juce::Colours::white);
        nextBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (nextBtn);

        cancelBtn.setButtonText ("Cancel");
        cancelBtn.setColour (juce::TextButton::buttonOnColourId,   juce::Colour (0xff3a3a3a));
        cancelBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff222222));
        cancelBtn.setColour (juce::TextButton::textColourOffId,       juce::Colours::white);
        cancelBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (cancelBtn);

        backBtn.setButtonText ("Back");
        backBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff222222));
        backBtn.setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xff3a3a3a));
        backBtn.setColour (juce::TextButton::textColourOffId,   juce::Colours::white);
        addAndMakeVisible (backBtn);

        installBtn.setButtonText ("Next");
        installBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff8a0306));
        installBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd7050c));
        installBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (installBtn);

        finishBtn.setButtonText ("Finish");
        finishBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff8a0306));
        finishBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd7050c));
        finishBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
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

        nextBtn.onClick = [this] { onNext(); };
        cancelBtn.onClick = [this] { quit(); };
        backBtn.onClick   = [this] { page = Welcome; resized(); };
        installBtn.onClick = [this] { doInstall(); };
        finishBtn.onClick  = [this] { quit(); };
    }

    void resized() override
    {
        const auto area = getLocalBounds();

        if (page == Welcome)
        {
            // full-window cover of the welcome image, then overlay the buttons
            const auto r = coverRect (area, bg);
            nextBtn.setBounds  (r.getX() + nextX   * r.getWidth(), r.getY() + nextY   * r.getHeight(),
                               nextW * r.getWidth(), nextH * r.getHeight());
            cancelBtn.setBounds(r.getX() + cancelX * r.getWidth(), r.getY() + cancelY * r.getHeight(),
                               cancelW * r.getWidth(), cancelH * r.getHeight());

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
            pathEdit.setBounds (cx - fieldW / 2, area.y + 200, fieldW, fieldH);
            check.setBounds     (cx - fieldW / 2, area.y + 248, 260, 24);
            title.setBounds     (area.x, area.y + 70, area.getWidth(), 34);
            status.setBounds    (area.x, area.y + 300, area.getWidth(), 28);

            const int bw = 140, bh = 40;
            backBtn.setBounds   (cx - bw - 20, area.y + area.getHeight() - 90, bw, bh);
            installBtn.setBounds(cx + 20,           area.y + area.getHeight() - 90, bw, bh);

            nextBtn.setVisible (false);
            cancelBtn.setVisible (false);
            finishBtn.setVisible (false);
        }
        else // Done
        {
            const auto cx = area.getCentreX();
            title.setBounds     (area.x, area.y + 70, area.getWidth(), 34);
            status.setBounds    (area.x, area.y + 140, area.getWidth(), 40);
            finishBtn.setBounds (cx - 75, area.y + area.getHeight() - 110, 150, 44);

            nextBtn.setVisible (false);
            cancelBtn.setVisible (false);
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
            paintWelcomeBackground (g, area);
            return;
        }

        paintPanel (g, area);
    }

private:
    enum Page { Welcome, Install, Done };
    Page page = Welcome;

    juce::Image bg;
    juce::TextButton nextBtn, cancelBtn, backBtn, installBtn, finishBtn;
    juce::TextEditor pathEdit;
    juce::ToggleButton check;
    juce::Label status, title;

    // image rect that covers the area (may crop), for mapping image fractions
    static juce::Rectangle<int> coverRect (const juce::Rectangle<int>& area, const juce::Image& img)
    {
        if (! img.isValid())
            return area;
        const float s = (float) juce::jmax (area.getWidth() / (float) img.getWidth(),
                                            area.getHeight() / (float) img.getHeight());
        const int w = (int) (img.getWidth()  * s);
        const int h = (int) (img.getHeight() * s);
        return { area.getX() + (area.getWidth() - w) / 2,
                 area.getY() + (area.getHeight() - h) / 2, w, h };
    }

    void paintWelcomeBackground (juce::Graphics& g, const juce::Rectangle<int>& area)
    {
        g.fillRect (area);
        if (bg.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (bg, coverRect (area, bg));
        }
    }

    void paintPanel (juce::Graphics& g, const juce::Rectangle<int>& area)
    {
        g.setColour (juce::Colour (0xff1a0d0e));
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
        const juce::File exeDir = juce::File::getSpecialLocation (juce::File::specialLocationType::currentExecutableDirectory);
        const juce::File pluginsDir = exeDir.getChildFile ("plugins");

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
        juce::DialogWindow::ExitCallbackInfo cbi;
        cbi.window = getTopLevelWindow();
        if (cbi.window != nullptr)
            cbi.window->closeWindow();
    }
};

class InstallerWindow : public juce::DocumentWindow
{
public:
    InstallerWindow()
        : DocumentWindow ("D.A AURA Installer",
                          juce::Colour (0xff141414),
                          juce::DocumentWindow::titleBarButtonsMid)
    {
        setResizable (true, true);
        setResizeLimits (720, 480, 1600, 1100);
        centreOnScreen();
        setVisible (true);
    }

    void resized() override
    {
        setContentComponent (new InstallerComponent());
    }
};

class DreamyInstallerApp : public juce::JUCEApplication
{
public:
    DreamyInstallerApp() {}
    ~DreamyInstallerApp() override = default;

    const juce::String getApplicationName() override     { return "D.A AURA Installer"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    void initialise (const juce::String& commandLine) override { (void) commandLine; }
    void shutdown() override {}
    void saveState (juce::MemoryOutputStream&) override {}
    void restoreState (const juce::MemoryBlock&) override {}

    void createWindow (bool isStandalone) override
    {
        (void) isStandalone;
        window = std::make_unique<InstallerWindow>();
    }

private:
    std::unique_ptr<InstallerWindow> window;
};

int main (int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    return DreamyInstallerApp::getInstance()->run();
}
