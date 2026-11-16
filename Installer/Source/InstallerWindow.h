#pragma once

#include "InstallerShell.h"

namespace daaura
{

/** The installer's window. Nothing of it is the system's: no title bar, no border, no shadow.
    What is seen of it is the view, which has the shape of the design's frame; around that
    frame the window is see-through, so that on the screen there is the design's window and
    nothing else.

    The view draws its own minimise and close buttons, and the window is moved by dragging it
    anywhere that is not a button. It is resized by dragging its frame, on any side or at any
    corner, and keeps its proportions while that is done. */
class InstallerWindow final : public juce::ResizableWindow
{
public:
    /** How far inside its frame the window can still be taken hold of to resize it, in the
        view's units: far enough to take in the frame's line all the way round, also where the
        frame's corners are cut off. (Outside the frame it can be taken hold of as well, all the
        way to the window's own, unseen edge: as with any window of the system's, whose edge
        reaches a little further than what is drawn of it.) */
    static constexpr float edgeReach = 7.0f;

    /** What the window puts where the view draws nothing: as good as nothing (black, one part
        in 255, which no screen shows). Windows lets the mouse through wherever a window is
        entirely see-through; with this trace there, the frame can be taken hold of from
        just outside it. */
    static constexpr juce::uint32 traceAroundTheFrame = 0x01000000;

    // From 70 % to 250 % of the size the view was drawn for
    static constexpr double smallestScale = 0.7, largestScale = 2.5;

    InstallerWindow (const juce::String& title, std::unique_ptr<InstallerShell> shellToShow)
        : juce::ResizableWindow (title, juce::Colours::transparentBlack, false),
          shell (*shellToShow)
    {
        setLookAndFeel (&look);
        setUsingNativeTitleBar (false);
        setOpaque (false);
        setDropShadowEnabled (false);       // (a shadow would be the shadow of a rectangle)
        setContentOwned (shellToShow.release(), false);
        setResizable (true, false);

        const int w = InstallerView::designWidth, h = InstallerView::designHeight;

        setResizeLimits (juce::roundToInt (smallestScale * w), juce::roundToInt (smallestScale * h),
                         juce::roundToInt (largestScale * w),  juce::roundToInt (largestScale * h));

        if (auto* limits = getConstrainer())
            limits->setFixedAspectRatio ((double) w / (double) h);

        setSize (w, h);
    }

    ~InstallerWindow() override
    {
        setLookAndFeel (nullptr);
    }

    /** Puts the window in the middle of the main screen and shows it: at 100 %, or smaller on
        a screen that could not hold that. */
    void showOnScreen()
    {
        const int w = InstallerView::designWidth, h = InstallerView::designHeight;
        juce::Rectangle<int> screen (0, 0, w, h);
        double scale = 1.0;

        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            screen = display->userBounds.toNearestInt();
            scale = juce::jlimit (smallestScale, 1.0, juce::jmin (0.92 * (double) screen.getWidth()  / (double) w,
                                                                  0.92 * (double) screen.getHeight() / (double) h));
        }

        // (the height follows from the width the way it does when the window is resized)
        const int width = juce::roundToInt (scale * w);

        setBounds (screen.withSizeKeepingCentre (width, juce::roundToInt ((double) width * (double) h / (double) w)));
        addToDesktop();
        setVisible (true);
        toFront (true);
    }

    InstallerView& getView() noexcept       { return shell.getView(); }
    InstallerShell& getShell() noexcept     { return shell; }

    int getDesktopWindowStyleFlags() const override
    {
        // (So that the system can still minimise and close it: the taskbar, Alt+F4, the Dock.)
        int flags = juce::ResizableWindow::getDesktopWindowStyleFlags()
                  | juce::ComponentPeer::windowHasMinimiseButton
                  | juce::ComponentPeer::windowHasCloseButton;

       #if JUCE_WINDOWS
        // Windows itself resizes only a window that has a frame of the system's, and this one
        // has none. Were Windows told that the window is resizable all the same, it would take
        // the outermost pixels for an edge of its own, show the resizing pointer there, and
        // then do nothing. So it is not told: the window resizes itself, by the edges that
        // lie under the view (see getBorderThickness).
        flags &= ~juce::ComponentPeer::windowIsResizable;
       #endif

        return flags;
    }

    // The edges that resize the window lie underneath the view, which fills the whole window
    // and lets the mouse through wherever it has no button. They reach from the window's own
    // edge to a little way inside the frame that is drawn.
    juce::BorderSize<int> getBorderThickness() const override
    {
        const float scale = getScale();
        const auto frame = InstallerView::getFrame();
        const auto reach = [scale] (float units) { return juce::roundToInt (scale * (units + edgeReach)); };

        return { reach (frame.getY()), reach (frame.getX()),
                 reach ((float) InstallerView::designHeight - frame.getBottom()),
                 reach ((float) InstallerView::designWidth  - frame.getRight()) };
    }

    /** A place on the frame that is drawn, where a hand would take hold of it. */
    enum class FramePart { leftSide, top, lowerRightCorner };

    /** That place, in the window: on the frame's line, a pixel or two inside its outer edge.
        (The lower right corner is cut off: there it is half way along the cut.) */
    juce::Point<int> getPointOnFrame (FramePart part) const
    {
        const float scale = getScale();
        const auto frame = InstallerView::getFrame() * scale;
        const float inside = 1.5f;

        switch (part)
        {
            case FramePart::leftSide:           return juce::Point<float> (frame.getX() + inside, frame.getCentreY()).roundToInt();
            case FramePart::top:                return juce::Point<float> (frame.getCentreX(), frame.getY() + inside).roundToInt();
            case FramePart::lowerRightCorner:   break;
        }

        const float halfCut = 0.5f * InstallerView::cornerCut * scale;
        return juce::Point<float> (frame.getRight() - halfCut - inside, frame.getBottom() - halfCut - inside).roundToInt();
    }

    /** How many of the screen's units there are to one of the view's. */
    float getScale() const noexcept     { return (float) getWidth() / (float) InstallerView::designWidth; }

    juce::BorderSize<int> getContentComponentBorder() const override    { return {}; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (traceAroundTheFrame));
    }

    void userTriedToCloseWindow() override
    {
        getView().dismiss();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)   { getView().advance(); return true; }
        if (key == juce::KeyPress::escapeKey)   { getView().dismiss(); return true; }

        return false;
    }

private:
    /** The edges that resize the window lie around the frame that is drawn, where nothing of
        the window is to be seen: they draw nothing. (Left to itself, an edge draws a line.) */
    struct Look final : public juce::LookAndFeel_V4
    {
        void drawResizableFrame (juce::Graphics&, int, int, const juce::BorderSize<int>&) override {}
    };

    Look look;
    InstallerShell& shell;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerWindow)
};

} // namespace daaura
