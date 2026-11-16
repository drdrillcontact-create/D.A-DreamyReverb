#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "InstallerAssets.h"
#include "InstallerEngine.h"

namespace daaura
{

/** Everything the installer shows: the pages
    Welcome -> Install Location -> Installing -> Finished (or Failed).

    The first page is the design itself (Installer/Design/Installer.png), shown pixel for
    pixel; the pages after it are the same picture with their own words on it.

    It has the shape of the design's frame, cut corners and all: outside that outline it draws
    nothing, and whatever is behind the window shows.

    It is laid out once, for a size of designWidth x designHeight; the window scales it.
    Clicks that land on nothing fall through to the window, which is how the window is dragged.
*/
class InstallerView : public juce::Component,
                      private juce::Timer
{
public:
    enum class Page { welcome, location, installing, finished, failed };

    static constexpr int designWidth  = 980;
    static constexpr int designHeight = 386;

    /** `payloadError` is what went wrong if there is no payload. */
    InstallerView (const Assets&, std::unique_ptr<Payload>, std::vector<InstallTarget>,
                   const juce::String& payloadError);
    ~InstallerView() override;

    std::function<void()> onMinimise;           // the minimise button
    std::function<void()> onDone;               // Cancel, Finish, Close: the installer is to quit
    std::function<void (Page)> onPageChanged;

    Page getPage() const noexcept                               { return page; }
    const std::vector<InstallTarget>& getTargets() const        { return targets; }
    const InstallReport& getReport() const                      { return report; }
    double getShownProgress() const noexcept                    { return shownProgress; }

    /** What the main button does on the current page: Next, Next (install), Finish, Close. */
    void advance();

    /** Cancel, the close button, Escape. Does nothing while files are being copied. */
    void dismiss();

    void setTargetSelected (int index, bool shouldBeSelected);

    /** To be called while the window is being dragged to another size: the picture is then
        scaled the quick way, and carefully once the dragging has stopped. */
    void sizeIsChanging();

    /** The picture behind the current page. */
    Assets::Picture getPicture() const noexcept;

    /** The outline of what is drawn (the design's frame), in the view's units. */
    static juce::Path getOutline();

    /** The rectangle that outline fits into, in the view's units. */
    static juce::Rectangle<float> getFrame() noexcept;

    /** How much of each side the outline's cut corners take off, in the view's units. */
    static constexpr float cornerCut = 10.0f;

    /** For comparing the type with the design: has the first page set its words in type, the
        way the later pages set theirs, instead of showing the design's own. */
    void setFirstPageInType (bool shouldBeInType);

    // Component IDs, for tests: "next", "cancel", "minimise", "close", "target-VST3", "target-AU"

    void paint (juce::Graphics&) override;

private:
    class PillButton;
    class ChromeButton;
    class TargetRow;
    class Job;

    void showPage (Page);
    void startInstalling();
    void timerCallback() override;

    void paintPicture (juce::Graphics&) const;
    void paintHeadline (juce::Graphics&, const juce::String& first,
                        const juce::String& white, const juce::String& red) const;
    void paintProgress (juce::Graphics&) const;

    /** The picture in the size a screen shows it in: kept, since making it takes a moment. */
    struct Sized
    {
        Assets::Picture picture {};
        float pixelsPerUnit = 0.0f;
        juce::Image image;
    };

    const juce::Image& sized (Assets::Picture, float pixelsPerUnit) const;

    const Assets& assets;
    std::unique_ptr<Payload> payload;
    std::vector<InstallTarget> targets;
    InstallReport report;
    Page page = Page::welcome;

    std::unique_ptr<PillButton> nextButton, cancelButton;
    std::unique_ptr<ChromeButton> minimiseButton, closeButton;
    juce::OwnedArray<TargetRow> rows;

    std::unique_ptr<Job> job;
    double shownProgress = 0.0;

    mutable std::array<Sized, 3> kept;
    mutable size_t nextToReplace = 0;
    std::unique_ptr<juce::Timer> settling;
    bool firstPageInType = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerView)
};

} // namespace daaura
