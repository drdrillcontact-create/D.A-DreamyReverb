#include "InstallerView.h"
#include "InstallerConfig.h"
#include "InstallerPicture.h"

namespace daaura
{

namespace
{
// The colours of the design's lettering, for the words the later pages add
const juce::Colour backdropColour { 0xff040303 };
const juce::Colour red            { 0xffea030f };      // the tick, the progress bar
const juce::Colour textRed        { 0xffdd0510 };      // the red word of a headline
const juce::Colour white          { 0xffdedede };      // headlines
const juce::Colour brightWhite    { 0xffeeeeee };
const juce::Colour grey           { 0xffaaa9a9 };      // running text
const juce::Colour dimGrey        { 0xff757575 };
const juce::Colour outlineGrey    { 0xff4a4a4a };

// Where things are in the design, in the view's own units (designWidth x designHeight).
// Measured on the picture, which has 1.8 pixels to the unit.
const juce::Rectangle<float> nextPill   { 479.44f, 306.67f, 218.89f, 38.89f };     // the red button
const juce::Rectangle<float> cancelPill { 719.44f, 306.67f, 202.22f, 38.89f };
constexpr float nextRounding   = 7.8f;
constexpr float cancelRounding = 11.7f;
constexpr float glow = 15.0f;           // how far the red button's light reaches

const juce::Point<float> minimiseCentre { 907.78f, 29.44f };
const juce::Point<float> closeCentre    { 942.22f, 29.72f };
constexpr float glyphReach = 7.0f;      // half the width of the minimise bar and of the cross

constexpr float textLeft  = 479.5f;
constexpr float textRight = 921.7f;     // the right edge of the Cancel button

const juce::Rectangle<float> progressBar { textLeft + 1.0f, 226.0f, textRight - textLeft - 1.0f, 8.0f };

// The width a piece of text takes up in a line. (Measured between two markers, so that
// spaces at its ends count.)
float widthOf (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, "|" + text + "|")
         - juce::GlyphArrangement::getStringWidth (font, "||");
}

struct Run
{
    juce::Font font;
    juce::Colour colour;
    juce::String text;
};

// Draws pieces of text one after the other on a baseline; returns where the line ends
float drawRuns (juce::Graphics& g, float x, float baseline, const std::vector<Run>& runs)
{
    for (const auto& run : runs)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (run.font, run.text, x, baseline);
        g.setColour (run.colour);
        glyphs.draw (g);
        x += widthOf (run.font, run.text);
    }

    return x;
}

juce::String productName()      { return daaura::config::productName; }
juce::String companyName()      { return daaura::config::companyName; }

/** Calls for a repaint once the window has stopped changing its size. */
struct Settling final : public juce::Timer
{
    explicit Settling (juce::Component& c) : owner (c) {}

    void timerCallback() override
    {
        stopTimer();
        owner.repaint();
    }

    juce::Component& owner;
};
}

//==============================================================================
/** Next / Finish / Close (the red button) and Cancel (the outlined one).

    The buttons themselves are in the picture. What is drawn here is what the picture cannot
    show: a label that changes (Finish, Close), and the button answering the mouse. */
class InstallerView::PillButton : public juce::Button
{
public:
    PillButton (const Assets& a, bool isMainButton)
        : juce::Button ({}), assets (a), main (isMainButton),
          pill (isMainButton ? nextPill : cancelPill),
          rounding (isMainButton ? nextRounding : cancelRounding)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);

        // (with room for the light around the red button, which dims with it)
        setBounds (pill.getSmallestIntegerContainer().expanded ((int) glow));
    }

    /** `drawnHere` is false where the picture behind the page already shows this label. */
    void setLabel (const juce::String& label, bool drawnHere)
    {
        setButtonText (label);
        labelDrawnHere = drawnHere;
        repaint();
    }

    bool hitTest (int x, int y) override
    {
        return shape().contains ((float) x, (float) y);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto area = shape();

        if (labelDrawnHere)
        {
            // (as the design sets "Next": black, bold, in the middle of the button)
            const auto font = assets.textBold (labelHeight);
            const float width = widthOf (font, getButtonText());

            drawRuns (g, area.getCentreX() - 0.5f * width, area.getY() + labelBaseline,
                      { { font, main ? juce::Colours::black : brightWhite, getButtonText() } });
        }

        if (! isEnabled())
        {
            // dimmed, and the light around it with it
            const float around = main ? glow : 2.0f;
            g.setColour (juce::Colours::black.withAlpha (0.66f));
            g.fillRoundedRectangle (area.expanded (around), rounding + around);
        }
        else if (down)
        {
            g.setColour (main ? juce::Colours::black.withAlpha (0.20f) : juce::Colours::white.withAlpha (0.13f));
            g.fillRoundedRectangle (area, rounding);
        }
        else if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (main ? 0.11f : 0.07f));
            g.fillRoundedRectangle (area, rounding);

            if (! main)
            {
                g.setColour (juce::Colours::white.withAlpha (0.28f));
                g.drawRoundedRectangle (area.reduced (0.8f), rounding - 0.8f, 1.4f);
            }
        }
    }

    // The size of a button's label and where its baseline lies below the button's top edge:
    // as the design has them for "Next" and "Cancel"
    static constexpr float labelHeight = 22.7f, labelBaseline = 25.0f;

private:
    juce::Rectangle<float> shape() const
    {
        return pill.translated (-(float) getX(), -(float) getY());
    }

    const Assets& assets;
    const bool main;
    const juce::Rectangle<float> pill;
    const float rounding;
    bool labelDrawnHere = false;
};

//==============================================================================
/** The two small controls in the top right corner: minimise and close. They are in the
    picture; drawn here is how they answer the mouse. */
class InstallerView::ChromeButton : public juce::Button
{
public:
    explicit ChromeButton (bool isCloseButton)
        : juce::Button (isCloseButton ? "Close" : "Minimise"), closes (isCloseButton),
          centre (isCloseButton ? closeCentre : minimiseCentre)
    {
        setWantsKeyboardFocus (false);
        setBounds (juce::Rectangle<float> (26.0f, 24.0f).withCentre (centre).getSmallestIntegerContainer());
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto middle = centre - getPosition().toFloat();
        const auto patch = juce::Rectangle<float> (23.0f, 21.0f).withCentre (middle);

        if (! isEnabled())
        {
            g.setColour (juce::Colours::black.withAlpha (0.72f));
            g.fillRect (patch);
            return;
        }

        if (! (over || down))
            return;

        // A patch of colour under the pointer, and the control's sign again on top of it
        g.setColour (closes ? red.withAlpha (down ? 0.95f : 0.8f) : juce::Colours::white.withAlpha (down ? 0.26f : 0.17f));
        g.fillRoundedRectangle (patch, 4.5f);

        juce::Path sign;

        if (closes)
        {
            sign.startNewSubPath (middle.x - glyphReach + 0.5f, middle.y - glyphReach + 0.5f);
            sign.lineTo (middle.x + glyphReach - 0.5f, middle.y + glyphReach - 0.5f);
            sign.startNewSubPath (middle.x + glyphReach - 0.5f, middle.y - glyphReach + 0.5f);
            sign.lineTo (middle.x - glyphReach + 0.5f, middle.y + glyphReach - 0.5f);
        }
        else
        {
            sign.startNewSubPath (middle.x - glyphReach, middle.y);
            sign.lineTo (middle.x + glyphReach, middle.y);
        }

        g.setColour (juce::Colours::white);
        g.strokePath (sign, juce::PathStrokeType (1.7f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

private:
    const bool closes;
    const juce::Point<float> centre;
};

//==============================================================================
/** One line of the Install Location page: a tick box, what is installed, and where to. */
class InstallerView::TargetRow : public juce::Button
{
public:
    TargetRow (const Assets& a, const InstallTarget& target)
        : juce::Button (target.title), assets (a), title (target.title), path (target.shownPath)
    {
        setClickingTogglesState (true);
        setToggleState (target.selected, juce::dontSendNotification);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto box = juce::Rectangle<float> (3.0f, 3.0f, 17.0f, 17.0f);
        const bool ticked = getToggleState();

        if (ticked)
        {
            g.setColour (red.withAlpha (0.16f));
            g.fillRoundedRectangle (box.expanded (2.5f), 6.0f);
            g.setColour (over ? red.brighter (0.12f) : red);
            g.fillRoundedRectangle (box, 4.0f);

            juce::Path tick;
            tick.startNewSubPath (box.getX() + 4.2f, box.getCentreY() + 0.3f);
            tick.lineTo (box.getX() + 7.4f, box.getBottom() - 4.6f);
            tick.lineTo (box.getRight() - 3.8f, box.getY() + 4.8f);
            g.setColour (juce::Colours::black);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        }
        else
        {
            g.setColour (juce::Colour (0xff070707));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (over ? juce::Colour (0xff9a9a9a) : outlineGrey);
            g.drawRoundedRectangle (box, 4.0f, 1.2f);
        }

        const float left = box.getRight() + 10.0f;
        drawRuns (g, left, 16.4f, { { assets.textBold (19.5f), ticked ? white : grey, title } });
        drawRuns (g, left, 32.2f, { { assets.text (16.0f), ticked ? grey : dimGrey, path } });
    }

private:
    const Assets& assets;
    const juce::String title, path;
};

//==============================================================================
/** Copies the files on a thread of its own, so that the window stays alive meanwhile. */
class InstallerView::Job : public juce::Thread
{
public:
    Job (Payload& p, std::vector<InstallTarget> t)
        : juce::Thread ("D.A AURA installation"), payload (p), targets (std::move (t))
    {
    }

    ~Job() override
    {
        stopThread (60000);
    }

    void run() override
    {
        report = install (payload, targets, [this] (double fraction) { progress.store (fraction); });
        done.store (true);
    }

    std::atomic<double> progress { 0.0 };
    std::atomic<bool> done { false };
    InstallReport report;                   // to be read once `done` is set

private:
    Payload& payload;
    const std::vector<InstallTarget> targets;
};

//==============================================================================
InstallerView::InstallerView (const Assets& a, std::unique_ptr<Payload> p, std::vector<InstallTarget> t,
                              const juce::String& payloadError)
    : assets (a), payload (std::move (p)), targets (std::move (t))
{
    setOpaque (false);                          // (outside the frame there is nothing of it)
    setInterceptsMouseClicks (false, true);     // the empty parts belong to the window (dragging)
    setSize (designWidth, designHeight);

    // Only what this installer actually carries is offered
    if (payload != nullptr)
        targets.erase (std::remove_if (targets.begin(), targets.end(),
                                       [this] (const InstallTarget& target) { return payload->getBundleName (target.format).isEmpty(); }),
                       targets.end());

    nextButton     = std::make_unique<PillButton> (assets, true);
    cancelButton   = std::make_unique<PillButton> (assets, false);
    minimiseButton = std::make_unique<ChromeButton> (false);
    closeButton    = std::make_unique<ChromeButton> (true);

    nextButton->setComponentID ("next");
    cancelButton->setComponentID ("cancel");
    minimiseButton->setComponentID ("minimise");
    closeButton->setComponentID ("close");

    cancelButton->setLabel ("Cancel", false);

    settling = std::make_unique<Settling> (*this);

    nextButton->onClick     = [this] { advance(); };
    cancelButton->onClick   = [this] { dismiss(); };
    closeButton->onClick    = [this] { dismiss(); };
    minimiseButton->onClick = [this] { if (onMinimise) onMinimise(); };

    addAndMakeVisible (*nextButton);
    addAndMakeVisible (*cancelButton);
    addAndMakeVisible (*minimiseButton);
    addAndMakeVisible (*closeButton);

    // One row per plug-in format, each ticked to begin with
    const int rowHeight = 36, rowGap = 4;
    const int firstRow = targets.size() > 1 ? 219 : 230;

    for (int i = 0; i < (int) targets.size(); ++i)
    {
        auto* row = rows.add (new TargetRow (assets, targets[(size_t) i]));
        row->setComponentID ("target-" + targets[(size_t) i].format);
        row->setBounds ((int) textLeft - 2, firstRow + i * (rowHeight + rowGap), (int) (textRight - textLeft) + 2, rowHeight);
        row->onClick = [this, i, row] { setTargetSelected (i, row->getToggleState()); };
        addChildComponent (row);
    }

    if (payload == nullptr)
    {
        report.error = payloadError;
        showPage (Page::failed);
    }
    else if (targets.empty())
    {
        report.error = "This installer carries no plug-in for this system.";
        showPage (Page::failed);
    }
    else
    {
        showPage (Page::welcome);
    }
}

InstallerView::~InstallerView()
{
    stopTimer();
    job.reset();        // (waits for the copying to end)
}

//==============================================================================
void InstallerView::setTargetSelected (int index, bool shouldBeSelected)
{
    if (! juce::isPositiveAndBelow (index, (int) targets.size()))
        return;

    targets[(size_t) index].selected = shouldBeSelected;
    rows[index]->setToggleState (shouldBeSelected, juce::dontSendNotification);

    // Next is only offered while something is ticked
    const bool any = std::any_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; });
    nextButton->setEnabled (page != Page::location || any);
}

void InstallerView::showPage (Page newPage)
{
    page = newPage;

    for (auto* row : rows)
        row->setVisible (page == Page::location);

    const bool asking = page == Page::welcome || page == Page::location;

    // ("Next" and its arrow are in the picture; Finish and Close are set on its bare red button)
    nextButton->setVisible (page != Page::installing);
    cancelButton->setVisible (asking);
    nextButton->setLabel (asking ? "Next" : (page == Page::finished ? "Finish" : "Close"), ! asking);

    const bool any = std::any_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; });
    nextButton->setEnabled (page != Page::location || any);
    closeButton->setEnabled (page != Page::installing);

    repaint();

    if (onPageChanged)
        onPageChanged (page);
}

void InstallerView::advance()
{
    switch (page)
    {
        case Page::welcome:     showPage (Page::location); break;
        case Page::location:    startInstalling(); break;
        case Page::installing:  break;

        case Page::finished:
        case Page::failed:
            if (onDone)
                onDone();
            break;
    }
}

void InstallerView::dismiss()
{
    // (Not while files are being copied: that takes a moment and is then over.)
    if (page != Page::installing && onDone)
        onDone();
}

void InstallerView::startInstalling()
{
    if (payload == nullptr || job != nullptr
        || std::none_of (targets.begin(), targets.end(), [] (const InstallTarget& t) { return t.selected; }))
        return;

    shownProgress = 0.0;
    job = std::make_unique<Job> (*payload, targets);
    showPage (Page::installing);
    job->startThread();
    startTimerHz (60);
}

void InstallerView::timerCallback()
{
    if (job == nullptr)
    {
        stopTimer();
        return;
    }

    // The files are copied in a blink. The bar takes its time (about a second for the full
    // length), so that the page is seen rather than flashing by.
    const double step = (double) getTimerInterval() / 900.0;
    shownProgress = juce::jmin (job->progress.load(), shownProgress + step);
    repaint (progressBar.expanded (8.0f, 30.0f).getSmallestIntegerContainer());

    if (job->done.load() && (! job->report.succeeded || shownProgress >= 1.0))
    {
        stopTimer();
        job->stopThread (60000);
        report = job->report;
        job.reset();
        showPage (report.succeeded ? Page::finished : Page::failed);
    }
}

//==============================================================================
Assets::Picture InstallerView::getPicture() const noexcept
{
    switch (page)
    {
        case Page::welcome:     return firstPageInType ? Assets::Picture::noText : Assets::Picture::design;
        case Page::location:    return Assets::Picture::noText;
        case Page::installing:  return Assets::Picture::noButtons;
        case Page::finished:
        case Page::failed:      break;
    }

    return Assets::Picture::finish;
}

void InstallerView::sizeIsChanging()
{
    if (isShowing())
        settling->startTimer (160);
}

void InstallerView::setFirstPageInType (bool shouldBeInType)
{
    firstPageInType = shouldBeInType;
    repaint();
}

const juce::Image& InstallerView::sized (Assets::Picture which, float pixelsPerUnit) const
{
    const auto& original = assets.getPicture (which);

    // At its own size, the picture itself
    if (std::abs (pixelsPerUnit - Assets::pictureScale) < 1.0e-4f)
        return original;

    for (const auto& k : kept)
        if (k.image.isValid() && k.picture == which && std::abs (k.pixelsPerUnit - pixelsPerUnit) < 1.0e-5f)
            return k.image;

    auto& made = kept[nextToReplace];
    nextToReplace = (nextToReplace + 1) % kept.size();

    made.picture = which;
    made.pixelsPerUnit = pixelsPerUnit;
    made.image = resampled (original,
                            (int) std::ceil ((float) designWidth  * pixelsPerUnit - 0.001f),
                            (int) std::ceil ((float) designHeight * pixelsPerUnit - 0.001f),
                            (double) Assets::pictureScale / (double) pixelsPerUnit);

    // Handed to the system in its own kind of picture, which it draws without copying it
    // every time. (The pixels stay what they are.)
    if (const auto native = juce::NativeImageType().convert (made.image); native.isValid())
        made.image = native;

    return made.image;
}

void InstallerView::paintPicture (juce::Graphics& g) const
{
    const auto which = getPicture();
    const auto& original = assets.getPicture (which);

    if (! original.isValid())
    {
        g.fillAll (backdropColour);
        return;
    }

    // How many of the screen's pixels there are to one of the view's units just now
    const float pixelsPerUnit = g.getInternalContext().getPhysicalPixelScaleFactor();

    // A picture is drawn as solid as the colour that is being drawn with. That is said here:
    // a see-through window is handed over, on Windows, with nothing as that colour, and the
    // picture would then not be drawn at all.
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);

    if (pixelsPerUnit <= 0.0f || settling->isTimerRunning())
    {
        // (the window is being dragged to another size: scaled the quick way meanwhile)
        g.drawImage (original, getLocalBounds().toFloat());
        return;
    }

    // The picture in exactly the size it is shown in, set down pixel on pixel
    g.drawImageTransformed (sized (which, pixelsPerUnit), juce::AffineTransform::scale (1.0f / pixelsPerUnit));
}

void InstallerView::paintHeadline (juce::Graphics& g, const juce::String& first,
                                   const juce::String& whitePart, const juce::String& redPart) const
{
    // Sizes, weights, spacing and places are those that make "Welcome to the" and
    // "D.A Production Installer", set in type, cover the design's own two lines.
    drawRuns (g, 478.4f, 155.6f, { { assets.textMedium (46.0f, 0.0083f), white, first } });

    const auto bold = assets.textExtraBold (53.0f, -0.006f);
    drawRuns (g, 478.9f, 197.3f, { { bold, white, whitePart }, { bold, textRed, redPart } });
}

void InstallerView::paintProgress (juce::Graphics& g) const
{
    const auto bar = progressBar;
    const float radius = 0.5f * bar.getHeight();

    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (bar, radius);
    g.setColour (outlineGrey.withAlpha (0.8f));
    g.drawRoundedRectangle (bar, radius, 1.0f);

    const float filled = (float) juce::jlimit (0.0, 1.0, shownProgress) * bar.getWidth();

    if (filled > 1.0f)
    {
        const auto fill = bar.withWidth (juce::jmax (bar.getHeight(), filled));

        g.setColour (red.withAlpha (0.14f));
        g.fillRoundedRectangle (fill.expanded (3.0f), radius + 3.0f);
        g.setColour (red);
        g.fillRoundedRectangle (fill, radius);
    }

    const auto small = assets.textMedium (19.0f);
    const auto percent = juce::String (juce::roundToInt (100.0 * juce::jlimit (0.0, 1.0, shownProgress))) + " %";

    drawRuns (g, textLeft, 258.0f, { { small, grey, "Copying the plug-in files" + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")) } });
    drawRuns (g, textRight - widthOf (small, percent), 258.0f, { { small, white, percent } });
}

juce::Path InstallerView::getOutline()
{
    auto outline = Assets::outline();
    outline.applyTransform (juce::AffineTransform::scale (1.0f / Assets::pictureScale));
    return outline;
}

juce::Rectangle<float> InstallerView::getFrame() noexcept
{
    return Assets::frame() / Assets::pictureScale;
}

void InstallerView::paint (juce::Graphics& g)
{
    // Only what lies inside the design's frame is the window
    g.reduceClipRegion (Assets::outline(), juce::AffineTransform::scale (1.0f / Assets::pictureScale));

    paintPicture (g);

    // (likewise: the design's two lines of running text)
    const auto body   = assets.textSemiBold (20.9f);
    const auto strong = assets.textExtraBold (20.9f);
    const auto name   = productName();
    const float line1 = 235.0f, line2 = 255.9f;

    switch (page)
    {
        case Page::welcome:
            // The design says it all. (Set in type only to see how the type compares with it.)
            if (firstPageInType)
            {
                paintHeadline (g, "Welcome to the", companyName() + " ", "Installer");
                drawRuns (g, textLeft, line1, { { body, grey, "This will install " + companyName() + " on your computer." } });
                drawRuns (g, textLeft, line2, { { body, grey, "Click Next to continue, or Cancel to exit." } });
            }
            break;

        case Page::location:
            paintHeadline (g, "Select the", "Install ", "Location");
            break;

        case Page::installing:
            paintHeadline (g, "Please wait", "Installing ", name);
            paintProgress (g);
            break;

        case Page::finished:
            paintHeadline (g, "Installation complete", name + " is ", "Ready");
            drawRuns (g, textLeft, line1, { { strong, white, name }, { body, grey, " has been installed on your computer." } });
            drawRuns (g, textLeft, line2, { { body, grey, "Click Finish to close the installer." } });
            drawRuns (g, textLeft, 281.0f, { { assets.text (16.5f), dimGrey, "If your DAW does not list it yet, let it rescan its plug-ins." } });
            break;

        case Page::failed:
        {
            paintHeadline (g, "Installation failed", name + " ", "Not Installed");

            juce::AttributedString message;
            message.setJustification (juce::Justification::topLeft);
            message.setWordWrap (juce::AttributedString::byWord);
            message.append (report.error, assets.textMedium (18.5f), grey);

            juce::TextLayout layout;
            layout.createLayout (message, textRight - textLeft + 20.0f);
            layout.draw (g, { textLeft, 217.0f, textRight - textLeft + 20.0f, 84.0f });
            break;
        }
    }
}

} // namespace daaura
