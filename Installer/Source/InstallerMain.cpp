// The installer program: what it does when started.
//
//   "D.A AURA Installer"                   shows the window and waits for the user
//
// For unattended use and for the build's own checks:
//
//   --silent                               installs into the standard folders without a window;
//                                          exit code 0 = installed, 1 = failed
//   --self-test <folder>                   shows the window and goes through it the way a person
//                                          would (see SelfTest below), saving a picture of each
//                                          page into <folder>; exit code 0 = all as expected
//   --vst3-dir <folder>, --au-dir <folder> install there instead of the standard folders
//   --log <file>                           also write what happens to <file>
//   --page-seconds <n>                     (self-test) how long each page stays up; default 0.6

#include <juce_gui_basics/juce_gui_basics.h>
#include "InstallerConfig.h"
#include "InstallerWindow.h"
#include "InstallerPicture.h"
#include <cstdio>
#include <deque>

extern "C" int daaura_postMouse (int action, int x, int y);             // in SystemMouse.cpp
extern "C" void daaura_plainWindowEdge (void* nativeWindowHandle);      // in SystemWindow.cpp

namespace daaura
{

namespace
{
juce::File logFile;

void note (const juce::String& line)
{
    std::printf ("%s\n", line.toRawUTF8());
    std::fflush (stdout);

    if (logFile != juce::File())
        logFile.appendText (line + "\n", false, false, "\n");
}

const char* nameOf (InstallerView::Page page)
{
    switch (page)
    {
        case InstallerView::Page::welcome:      return "welcome";
        case InstallerView::Page::location:     return "location";
        case InstallerView::Page::installing:   return "installing";
        case InstallerView::Page::finished:     return "finished";
        case InstallerView::Page::failed:       return "failed";
    }

    return "?";
}

struct Options
{
    bool silent = false, selfTest = false;
    juce::File selfTestFolder, vst3Folder, auFolder;
    double pageSeconds = 0.6;

    static Options parse (const juce::StringArray& args)
    {
        Options o;
        const auto here = juce::File::getCurrentWorkingDirectory();

        for (int i = 0; i < args.size(); ++i)
        {
            const auto& arg = args[i];
            const auto value = i + 1 < args.size() ? args[i + 1].unquoted() : juce::String();
            const auto file = value.isNotEmpty() ? here.getChildFile (value) : juce::File();

            if (arg == "--silent")                                   { o.silent = true; }
            else if (arg == "--self-test" && value.isNotEmpty())     { o.selfTest = true; o.selfTestFolder = file; ++i; }
            else if (arg == "--vst3-dir" && value.isNotEmpty())      { o.vst3Folder = file; ++i; }
            else if (arg == "--au-dir" && value.isNotEmpty())        { o.auFolder = file; ++i; }
            else if (arg == "--log" && value.isNotEmpty())           { logFile = file; ++i; }
            else if (arg == "--page-seconds" && value.isNotEmpty())  { o.pageSeconds = juce::jlimit (0.1, 30.0, value.getDoubleValue()); ++i; }
        }

        return o;
    }

    std::vector<InstallTarget> getTargets() const
    {
        auto targets = getStandardTargets();

        for (auto& t : targets)
        {
            const auto& folder = t.format == "VST3" ? vst3Folder : (t.format == "AU" ? auFolder : juce::File());

            if (folder != juce::File())
            {
                t.directory = folder;
                t.shownPath = folder.getFullPathName();
            }
        }

        return targets;
    }
};
}

//==============================================================================
/** --self-test: goes through the installer the way a person would, and looks at what comes
    of it. It takes the window by its frame (at a corner and on two of its sides) and drags it
    to other sizes, drags it to another place, and then clicks Next and Next, puts the window
    away with its minimise button, brings it back, and clicks Finish.

    Meanwhile there is a plain, light sheet behind the window. Around its frame the window is
    see-through: a photograph of the screen then shows the sheet there, and nothing of the
    window.

    Where the system lets this program move the mouse, that is done with the real mouse
    pointer, through the system. Where it does not, the test says so, leaves the dragging out,
    and tells the buttons that they were clicked.

    It is written as a list of steps; the timer tries the first one again and again until that
    step says it is done. */
class SelfTest final : private juce::Timer
{
public:
    SelfTest (InstallerWindow& w, const Assets& assets, const Options& o, std::function<void()> closeTheInstaller)
        : window (w), options (o), closeInstaller (std::move (closeTheInstaller)),
          started (now()), stepBegan (started)
    {
        options.selfTestFolder.createDirectory();

        note ("self-test: " + juce::SystemStats::getOperatingSystemName()
              + (juce::SystemStats::isOperatingSystem64Bit() ? ", 64-bit" : ""));

        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            note ("self-test: screen " + display->logicalBounds.toNearestInt().toString() + ", scale " + juce::String (display->scale, 2));
        else
            note ("self-test: there is no screen here");

        if (window.isOnDesktop())
        {
            // (nothing else is to cover the window while it is tried and photographed)
            window.setAlwaysOnTop (true);

            sheet = std::make_unique<Sheet>();
            sheet->setBounds (window.getBounds().expanded (Sheet::around));
            sheet->addToDesktop (0);
            sheet->setVisible (true);
            sheet->toBehind (&window);
            window.toFront (true);

            note ("self-test: behind the window, " + juce::String (Sheet::around) + " further out on every side, there is a plain sheet coloured "
                  + juce::Colour (Sheet::colour).toDisplayString (false).toLowerCase());
        }

        lookAtTheWindow (assets);
        planTheDrags();
        planThePages();
        startTimer (40);
    }

    /** The installer is about to quit. Says how the test went; returns the exit code. */
    int concluded()
    {
        if (ending == Ending::none || ending == Ending::givenUp)
        {
            stopTimer();

            if (ending == Ending::none)
                expect (finishWasClicked, finishWasClicked ? "Finish closes the installer"
                                                           : "the installer stays open until it has been clicked through");

            ending = Ending::reported;
            note (juce::String ("self-test: ") + (failures == 0 ? "ALL PASSED" : "FAILED"));
        }

        return failures == 0 ? 0 : 1;
    }

private:
    using Page = InstallerView::Page;

    /** What is behind the window during the test: a light grey that the design has nowhere
        along its frame. Whatever a photograph of the screen shows of this colour inside the
        window's rectangle, the window let through. (And a click that the window let through
        would land on this, not on something of the machine's.) */
    struct Sheet final : public juce::Component
    {
        static constexpr juce::uint32 colour = 0xffd2d2d2;
        static constexpr int around = 40;

        Sheet()                                     { setOpaque (true); }
        void paint (juce::Graphics& g) override     { g.fillAll (juce::Colour (colour)); }
    };

    static double now()     { return juce::Time::getMillisecondCounterHiRes(); }

    void expect (bool ok, const juce::String& what)
    {
        note (juce::String ("self-test: [") + (ok ? "PASS" : "FAIL") + "] " + what);

        if (! ok)
            ++failures;
    }

    static bool proportionsKept (juce::Rectangle<int> bounds)
    {
        const double ratio  = (double) bounds.getWidth() / (double) juce::jmax (1, bounds.getHeight());
        const double wanted = (double) InstallerView::designWidth / (double) InstallerView::designHeight;
        return std::abs (ratio - wanted) < 0.02;
    }

    //==============================================================================
    // The steps

    void then (std::function<bool()> step)      { steps.push_back (std::move (step)); }
    void thenDo (std::function<void()> action)  { then ([a = std::move (action)] { a(); return true; }); }
    void thenWait (double milliseconds)         { then ([this, milliseconds] { return now() - stepBegan >= milliseconds; }); }

    void timerCallback() override
    {
        if (now() - started > 120000.0)
        {
            expect (false, juce::String ("it gets through in two minutes  (stuck on the page ") + nameOf (window.getView().getPage()) + ")");
            giveUp();
            return;
        }

        while (! steps.empty())
        {
            const bool done = steps.front()();

            // (a step may have ended the test, or the click it made the program)
            if (ending != Ending::none || ! done)
                return;

            steps.pop_front();
            stepBegan = now();
        }

        // Finish was clicked, time has passed, and the installer is still here
        expect (false, "Finish closes the installer");
        giveUp();
    }

    void giveUp()
    {
        stopTimer();
        ending = Ending::givenUp;

        const auto closeIt = closeInstaller;
        closeIt();              // (leads to concluded(), and then to the end of the program)
    }

    //==============================================================================
    // The mouse, through the system

    bool mouse (int action, juce::Point<int> where) const
    {
       #if JUCE_WINDOWS
        // (the system counts in pixels; the window, in pixels divided by the screen's scale)
        const auto native = juce::Desktop::getInstance().getDisplays().logicalToPhysical (where);
       #else
        const auto native = where;
       #endif

        return daaura_postMouse (action, native.x, native.y) != 0;
    }

    /** Away from every button, so that none of them is lit up in the pictures. */
    void parkTheMouse() const
    {
        if (mouseWorks)
        {
            const auto area = window.getScreenBounds();
            mouse (0, { area.getX() + area.getWidth() / 4, area.getCentreY() });
        }
    }

    juce::Button* findButton (const char* buttonID) const
    {
        return dynamic_cast<juce::Button*> (window.getView().findChildWithID (buttonID));
    }

    /** Clicks a button: with the mouse where that can be done, otherwise by telling it. */
    void thenClick (const char* buttonID)
    {
        thenDo ([this, buttonID]
        {
            auto* button = findButton (buttonID);

            if (button == nullptr)
                expect (false, juce::String ("there is a button called ") + buttonID);
            else if (mouseWorks)
                mouse (0, button->getScreenBounds().getCentre());
        });

        thenWait (200.0);

        thenDo ([this, buttonID]
        {
            if (auto* button = findButton (buttonID); button != nullptr && mouseWorks)
                mouse (1, button->getScreenBounds().getCentre());
        });

        thenWait (80.0);

        thenDo ([this, buttonID]
        {
            if (auto* button = findButton (buttonID))
            {
                if (mouseWorks)
                    mouse (2, button->getScreenBounds().getCentre());
                else
                    button->triggerClick();
            }
        });
    }

    /** Waits for the click to be answered: the page is no longer `left`. */
    void thenExpectPageLeft (Page left, const char* buttonID, const juce::String& what)
    {
        then ([this, left, buttonID, what, toldTheButton = false]() mutable
        {
            if (window.getView().getPage() != left)
            {
                if (! toldTheButton)
                    expect (true, what);

                return true;
            }

            if (now() - stepBegan < 4000.0)
                return false;

            if (mouseWorks && ! toldTheButton)
            {
                // The click with the mouse did nothing. That is a failure; the rest is still
                // gone through, by telling the buttons.
                expect (false, what + "  (the click with the mouse was not answered)");
                mouseWorks = false;
                toldTheButton = true;
                stepBegan = now();

                if (auto* button = findButton (buttonID))
                    button->triggerClick();

                return false;
            }

            expect (false, what);
            giveUp();
            return true;
        });

        thenDo ([this] { parkTheMouse(); });
        thenWait (200.0);
    }

    /** Leaves the page up for a while and takes its picture. */
    void thenShow (const juce::String& pictureName)
    {
        thenDo ([this]
        {
            // (the build's script photographs the screen when it reads this line)
            note (juce::String ("self-test: showing ") + nameOf (window.getView().getPage()));
        });

        thenWait (1000.0 * options.pageSeconds);

        thenDo ([this, pictureName]
        {
            picture (window.getView().getPage() == Page::failed ? juce::String ("4-failed") : pictureName);
        });
    }

    void picture (const juce::String& name)
    {
        auto& shell = window.getShell();
        const auto image = shell.createComponentSnapshot (shell.getLocalBounds(), true, 2.0f);
        const auto file = options.selfTestFolder.getChildFile (name + ".png");
        file.deleteFile();

        juce::FileOutputStream stream (file);

        if (! (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream)))
            expect (false, "a picture of the page could be saved to " + file.getFullPathName());
    }

    //==============================================================================
    void lookAtTheWindow (const Assets& assets)
    {
        const auto bounds = window.getBounds();
        const auto* peer = window.getPeer();
        const int style = peer != nullptr ? peer->getStyleFlags() : 0;

        expect (window.isOnDesktop() && peer != nullptr && window.isShowing(), "the window is on the screen");
        expect (peer != nullptr && (style & juce::ComponentPeer::windowHasTitleBar) == 0 && window.isResizable(),
                "it has no title bar of the system's, and it can be resized");
        expect (proportionsKept (bounds), "it opens in its own proportions  (" + bounds.toString() + ")");

        const auto missing = assets.getMissing();
        expect (missing.isEmpty(), "the picture and the typefaces are built in"
                                       + (missing.isEmpty() ? juce::String() : "  (missing: " + missing.joinIntoString (", ") + ")"));

        lookAtTheFirstPage (assets);

        // Asked for a smaller size of another shape, it becomes smaller in its own shape
        const auto request = bounds.withSizeKeepingCentre (juce::roundToInt (0.8 * bounds.getWidth()), juce::roundToInt (0.6 * bounds.getHeight()));
        window.setBoundsConstrained (request);
        const auto smaller = window.getBounds();
        expect (smaller.getWidth() < bounds.getWidth() && smaller.getHeight() < bounds.getHeight() && proportionsKept (smaller),
                "given another size by the program, it keeps its proportions  (asked for " + juce::String (request.getWidth()) + " x "
                    + juce::String (request.getHeight()) + ", it becomes " + juce::String (smaller.getWidth()) + " x " + juce::String (smaller.getHeight()) + ")");
        picture ("0-smaller");
        window.setBoundsConstrained (bounds);

        for (const auto& t : window.getView().getTargets())
            note ("self-test: " + t.format + " -> " + t.directory.getFullPathName());
    }

    /** The first page is the design itself. Drawn with as many pixels to the unit as the
        design has, it has to be that picture, pixel for pixel, inside the design's frame, and
        nothing at all outside it. */
    void lookAtTheFirstPage (const Assets& assets)
    {
        const auto& design = assets.getPicture (Assets::Picture::design);

        if (! design.isValid() || window.getView().getPage() != Page::welcome)
        {
            expect (false, "the first page is there to be compared with the design");
            return;
        }

        const int width = design.getWidth(), height = design.getHeight();
        juce::Image drawn (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
        {
            juce::Graphics g (drawn);

            // (As a see-through window is handed over to be drawn on Windows: with nothing as
            // the colour to draw with. Whatever is drawn has to say how solid it is itself.)
            g.setColour (juce::Colours::transparentBlack);

            g.addTransform (juce::AffineTransform::scale (Assets::pictureScale));
            window.getView().paintEntireComponent (g, true);
        }

        // (all rows but the last: the window is a fifth of a pixel short of that one)
        const auto likeness = compare (drawn, design, {}, Assets::outline(), height - 1);

        expect (likeness.inside > 1000000 && likeness.differing == 0,
                "the first page, drawn at the design's own size, is the design: inside its frame not one pixel differs  ("
                    + juce::String (likeness.differing) + " of " + juce::String (likeness.inside) + " differ)");
        expect (likeness.outside > 10000 && likeness.spilt == 0,
                "and outside that frame nothing is drawn: there the window is not there  ("
                    + juce::String (likeness.spilt) + " of " + juce::String (likeness.outside) + " pixels drawn on)");

        // (a sum over the design's pixels, to be compared from one system to the next)
        const juce::Image::BitmapData pixels (design, juce::Image::BitmapData::readOnly);
        juce::uint32 sum = 2166136261u;

        for (int y = 0; y < height - 1; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const auto colour = pixels.getPixelColour (x, y);

                for (const auto part : { colour.getRed(), colour.getGreen(), colour.getBlue() })
                    sum = (sum ^ part) * 16777619u;
            }
        }

        note ("self-test: the design's pixels sum to " + juce::String::toHexString ((juce::int64) sum));

        // The window itself: see-through, and without a shadow of the system's
        if (const auto* peer = window.getPeer())
        {
            const int style = peer->getStyleFlags();

            expect (! window.isOpaque() && (style & juce::ComponentPeer::windowIsSemiTransparent) != 0
                        && (style & juce::ComponentPeer::windowHasDropShadow) == 0,
                    "the window is see-through around the frame, and has no shadow or border of the system's");
        }
    }

    //==============================================================================
    // Dragging the window

    /** Where the window is taken hold of: on the frame that is drawn (at its lower right
        corner, on its left side, at its top), just outside that frame, or in the middle. */
    enum class Hold { lowerRightCorner, leftSide, top, outsideTheFrame, middle };

    juce::Point<int> gripOf (Hold hold) const
    {
        using Part = InstallerWindow::FramePart;
        const auto onScreen = window.getScreenBounds();

        switch (hold)
        {
            case Hold::lowerRightCorner:    return onScreen.getPosition() + window.getPointOnFrame (Part::lowerRightCorner);
            case Hold::leftSide:            return onScreen.getPosition() + window.getPointOnFrame (Part::leftSide);
            case Hold::top:                 return onScreen.getPosition() + window.getPointOnFrame (Part::top);

            // (below the frame, where the window is see-through: three pixels inside its own, unseen edge)
            case Hold::outsideTheFrame:     return { onScreen.getCentreX(), onScreen.getBottom() - 3 };
            case Hold::middle:              break;
        }

        // (on the picture, away from every button and every edge)
        return { onScreen.getX() + onScreen.getWidth() / 4, onScreen.getCentreY() };
    }

    /** Which way it is then pulled: the frame inwards (there is always room for that on the
        screen), the window as a whole a little to the right and down. */
    static juce::Point<int> pullOf (Hold hold)
    {
        switch (hold)
        {
            case Hold::lowerRightCorner:    return { -100, -40 };
            case Hold::leftSide:            return { 100, 0 };
            case Hold::top:                 return { 0, 40 };
            case Hold::outsideTheFrame:     return { 0, -40 };
            case Hold::middle:              break;
        }

        return { 30, 20 };
    }

    static bool turnedOutRight (Hold hold, juce::Rectangle<int> from, juce::Rectangle<int> to)
    {
        const int wider  = to.getWidth()  - from.getWidth();
        const int taller = to.getHeight() - from.getHeight();

        switch (hold)
        {
            case Hold::lowerRightCorner:
                return wider <= -40 && proportionsKept (to) && to.getPosition().getDistanceFrom (from.getPosition()) <= 1;

            case Hold::leftSide:
                return wider <= -40 && proportionsKept (to) && std::abs (to.getRight() - from.getRight()) <= 1;

            case Hold::top:
                return taller <= -16 && proportionsKept (to) && std::abs (to.getBottom() - from.getBottom()) <= 1;

            case Hold::outsideTheFrame:
                return taller <= -16 && proportionsKept (to) && std::abs (to.getY() - from.getY()) <= 1;

            case Hold::middle:
                break;
        }

        return std::abs (wider) <= 1 && std::abs (taller) <= 1
                && (to.getPosition() - from.getPosition()).getDistanceFrom (pullOf (hold)) <= 3;
    }

    static const char* describe (Hold hold)
    {
        switch (hold)
        {
            case Hold::lowerRightCorner:    return "taken with the mouse by the lower right corner of its frame and dragged, it resizes and keeps its proportions";
            case Hold::leftSide:            return "taken by the left side of its frame, it resizes as well, and its right edge stays where it is";
            case Hold::top:                 return "taken by the top of its frame, it resizes as well, and its bottom edge stays where it is";
            case Hold::outsideTheFrame:     return "taken just outside its frame, where nothing of it is seen, it resizes as well, as a window of the system's does";
            case Hold::middle:              break;
        }

        return "taken anywhere else and dragged, it moves and keeps its size";
    }

    void planTheDrags()
    {
        thenWait (500.0);       // (the window has only just appeared)

        for (const auto hold : { Hold::lowerRightCorner, Hold::leftSide, Hold::top, Hold::outsideTheFrame, Hold::middle })
            planDrag (hold);

        thenDo ([this]
        {
            // Where the window is, in the units the system's own screenshot tools count in
            // (Windows: pixels, macOS: points)
            const auto area = window.getScreenBounds();

           #if JUCE_WINDOWS
            const auto native = juce::Desktop::getInstance().getDisplays().logicalToPhysical (area);
           #else
            const auto native = area;
           #endif

            note ("self-test: window at " + juce::String (native.getX()) + " " + juce::String (native.getY())
                   + " " + juce::String (native.getWidth()) + " " + juce::String (native.getHeight()));

            // ...and in the same units, the frame that is drawn in it (left, top, width, height),
            // and how much of each of its sides is cut off at a corner
            const double unit = (double) native.getWidth() / (double) InstallerView::designWidth;
            const auto frame = InstallerView::getFrame();

            note ("self-test: frame at " + juce::String (native.getX() + unit * frame.getX(), 2) + " " + juce::String (native.getY() + unit * frame.getY(), 2)
                   + " " + juce::String (unit * frame.getWidth(), 2) + " " + juce::String (unit * frame.getHeight(), 2)
                   + " " + juce::String (unit * InstallerView::cornerCut, 2));
        });
    }

    void planDrag (Hold hold)
    {
        thenDo ([this, hold]
        {
            before = window.getBounds();
            grip = gripOf (hold);
            pull = pullOf (hold);

            // On a screen so small that the window opened at its smallest size, there is
            // nothing to pull an edge inwards to.
            const int smallest = juce::roundToInt (InstallerWindow::smallestScale * InstallerView::designWidth);
            roomToDrag = hold == Hold::middle || before.getWidth() - 110 >= smallest;

            if (! mouseTried)
                mouseWorks = window.getPeer() != nullptr && mouse (0, grip);
            else if (mouseWorks && roomToDrag)
                mouse (0, grip);
        });

        thenWait (300.0);

        thenDo ([this]
        {
            if (! mouseTried)
            {
                mouseTried = true;

                // (did the pointer really go there?)
                if (mouseWorks && juce::Desktop::getMousePosition().getDistanceFrom (grip) > 3)
                    mouseWorks = false;

                if (! mouseWorks)
                    note ("self-test: [SKIP] this program may not move the mouse on this system: the window is not dragged, "
                          "and the buttons are told that they were clicked");
            }

            if (mouseWorks && ! roomToDrag)
                note ("self-test: [SKIP] the window is as small as it gets on this screen, so no edge of it is dragged inwards");

            if (mouseWorks && roomToDrag)
                mouse (1, grip);
        });

        thenWait (250.0);

        for (int i = 1; i <= 10; ++i)
        {
            thenDo ([this, i]
            {
                if (mouseWorks && roomToDrag)
                    mouse (3, grip + juce::Point<int> (pull.x * i / 10, pull.y * i / 10));
            });

            thenWait (60.0);
        }

        thenDo ([this]
        {
            if (mouseWorks && roomToDrag)
                mouse (2, grip + pull);
        });

        thenWait (300.0);

        thenDo ([this, hold]
        {
            if (mouseWorks && roomToDrag)
            {
                const auto after = window.getBounds();
                const auto change = "  (" + before.toString() + " -> " + after.toString() + ")";

                // The frame is what the window is taken by. That it can also be taken just
                // outside the frame is a nicety, and up to the system: it is only noted.
                if (hold != Hold::outsideTheFrame || turnedOutRight (hold, before, after))
                    expect (turnedOutRight (hold, before, after), juce::String (describe (hold)) + change);
                else
                    note ("self-test: [NOTE] just outside its frame, where nothing of it is seen, this system does not let the window be taken hold of: "
                          "it is resized by the frame itself" + change);
            }

            window.setBoundsConstrained (before);
            parkTheMouse();
        });

        thenWait (300.0);
    }

    void planThePages()
    {
        thenDo ([this]
        {
            expect (window.getView().getPage() == Page::welcome, "it opens on the welcome page");
        });

        thenShow ("1-welcome");
        thenClick ("next");
        thenExpectPageLeft (Page::welcome, "next", "Next on the welcome page leads on");

        thenDo ([this]
        {
            auto& view = window.getView();
            bool allTicked = ! view.getTargets().empty();

            for (const auto& t : view.getTargets())
                allTicked = allTicked && t.selected;

            expect (view.getPage() == Page::location && allTicked,
                    "to the install location, where every folder is ticked to begin with");
        });

        thenShow ("2-location");
        thenClick ("next");
        thenExpectPageLeft (Page::location, "next", "Next on the location page starts the installation");

        // the bar, about half way
        then ([this]
        {
            auto& view = window.getView();

            if (view.getPage() == Page::installing && view.getShownProgress() < 0.5)
                return false;

            if (view.getPage() == Page::installing)
                picture ("3-installing");

            return true;
        });

        then ([this] { return window.getView().getPage() != Page::installing; });

        thenDo ([this]
        {
            auto& view = window.getView();
            const auto& report = view.getReport();

            if (view.getPage() != Page::finished)
            {
                expect (false, "the installation succeeds  (" + report.error.replace ("\n", " ") + ")");
                return;
            }

            expect (report.succeeded && report.installed.size() == (int) view.getTargets().size(),
                    "the installation succeeds, and everything that was ticked is reported as installed");

            for (const auto& path : report.installed)
                expect (juce::File (path).exists(), "it is there: " + path);
        });

        thenShow ("4-finished");
        planMinimising();

        // Finish (or, had it failed, Close): the last click, which ends the program
        thenDo ([this] { finishWasClicked = true; });
        thenClick ("next");
        thenWait (4000.0);
    }

    /** The minimise button: the window goes away, and when it is brought back it is where it
        was. (Not where there is no screen: there the window is nowhere to begin with.) */
    void planMinimising()
    {
        thenDo ([this] { before = window.getBounds(); });
        thenClick ("minimise");

        // (the systems take their time over it: the window flies into the taskbar or the Dock)
        then ([this] { return window.getPeer() == nullptr || window.isMinimised() || now() - stepBegan >= 8000.0; });

        thenDo ([this]
        {
            if (window.getPeer() != nullptr)
                expect (window.isMinimised(), "the minimise button puts the window away");
        });

        thenWait (1500.0);

        thenDo ([this]
        {
            if (window.getPeer() != nullptr)
                window.setMinimised (false);
        });

        then ([this] { return window.getPeer() == nullptr || ! window.isMinimised() || now() - stepBegan >= 8000.0; });
        thenWait (1500.0);

        thenDo ([this]
        {
            if (window.getPeer() != nullptr)
            {
                window.toFront (true);

                const auto after = window.getBounds();
                const bool asItWas = std::abs (after.getX() - before.getX()) <= 1 && std::abs (after.getY() - before.getY()) <= 1
                                      && std::abs (after.getWidth() - before.getWidth()) <= 1 && std::abs (after.getHeight() - before.getHeight()) <= 1;

                expect (! window.isMinimised() && window.isShowing() && asItWas,
                        "brought back, it is where it was and as large as it was  (" + before.toString() + " -> " + after.toString() + ")");
            }

            parkTheMouse();
        });

        thenWait (500.0);
    }

    //==============================================================================
    enum class Ending { none, givenUp, reported };

    InstallerWindow& window;
    std::unique_ptr<Sheet> sheet;
    const Options options;
    std::function<void()> closeInstaller;
    std::deque<std::function<bool()>> steps;
    const double started;
    double stepBegan;

    juce::Rectangle<int> before;
    juce::Point<int> grip, pull;
    bool mouseTried = false, mouseWorks = false, roomToDrag = false;
    bool finishWasClicked = false;
    Ending ending = Ending::none;
    int failures = 0;
};

//==============================================================================
class InstallerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override        { return juce::String (config::productName) + " Installer"; }
    const juce::String getApplicationVersion() override     { return config::version; }
    bool moreThanOneInstanceAllowed() override              { return true; }

    void initialise (const juce::String&) override
    {
        const auto options = Options::parse (getCommandLineParameterArray());

        note (getApplicationName() + " " + getApplicationVersion());

        juce::String error;
        auto payload = Payload::openBuiltIn (error);
        const auto targets = options.getTargets();

        if (options.silent)
        {
            setApplicationReturnValue (installSilently (payload.get(), targets, error));
            quit();
            return;
        }

        const bool hasScreen = ! juce::Desktop::getInstance().getDisplays().displays.isEmpty();

        if (! hasScreen && ! options.selfTest)
        {
            note ("failed: there is no screen to show the installer on. (With --silent it installs without one.)");
            setApplicationReturnValue (1);
            quit();
            return;
        }

        assets = std::make_unique<Assets>();

        auto view = std::make_unique<InstallerView> (*assets, std::move (payload), targets, error);
        view->onMinimise = [this] { if (window != nullptr && window->isOnDesktop()) window->setMinimised (true); };
        view->onDone     = [this] { leave(); };

        window = std::make_unique<InstallerWindow> (getApplicationName(), std::make_unique<InstallerShell> (std::move (view)));

        if (hasScreen)
        {
            window->showOnScreen();

            // (nothing of the system's around the picture)
            if (auto* peer = window->getPeer())
                daaura_plainWindowEdge (peer->getNativeHandle());
        }

        if (options.selfTest)
            selfTest = std::make_unique<SelfTest> (*window, *assets, options, [this] { leave(); });
    }

    void shutdown() override
    {
        selfTest.reset();
        window.reset();
        assets.reset();
    }

    void systemRequestedQuit() override
    {
        // (Cmd+Q, the Dock, logging out. Not while files are being copied.)
        if (window != nullptr)
            window->getView().dismiss();
        else
            quit();
    }

private:
    static int installSilently (Payload* payload, std::vector<InstallTarget> targets, const juce::String& payloadError)
    {
        if (payload == nullptr)
        {
            note ("failed: " + payloadError.replace ("\n", " "));
            return 1;
        }

        // only what this installer carries
        targets.erase (std::remove_if (targets.begin(), targets.end(),
                                       [payload] (const InstallTarget& t) { return payload->getBundleName (t.format).isEmpty(); }),
                       targets.end());

        const auto report = install (*payload, targets);

        for (const auto& path : report.installed)
            note ("installed: " + path);

        if (! report.succeeded)
            note ("failed: " + report.error.replace ("\n", " "));

        return report.succeeded ? 0 : 1;
    }

    void leave()
    {
        int result = 0;

        if (window != nullptr && window->getView().getPage() == InstallerView::Page::failed)
            result = 1;

        if (selfTest != nullptr)
            result = juce::jmax (result, selfTest->concluded());

        setApplicationReturnValue (result);
        quit();
    }

    std::unique_ptr<Assets> assets;
    std::unique_ptr<InstallerWindow> window;
    std::unique_ptr<SelfTest> selfTest;
};

} // namespace daaura

START_JUCE_APPLICATION (daaura::InstallerApplication)
