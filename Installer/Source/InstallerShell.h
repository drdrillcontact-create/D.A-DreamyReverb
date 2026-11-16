#pragma once

#include "InstallerView.h"

namespace daaura
{

/** What the window holds: the view, scaled to whatever size the window has.
    (The view is laid out for one size only; resizing the window enlarges or shrinks all of it.) */
class InstallerShell : public juce::Component
{
public:
    explicit InstallerShell (std::unique_ptr<InstallerView> viewToShow)
        : view (std::move (viewToShow))
    {
        setOpaque (false);                          // (it draws nothing itself)
        setInterceptsMouseClicks (false, true);     // empty parts belong to the window (dragging)
        addAndMakeVisible (*view);
        setSize (InstallerView::designWidth, InstallerView::designHeight);
    }

    InstallerView& getView() noexcept       { return *view; }

    void resized() override
    {
        const auto w = (float) InstallerView::designWidth;
        const auto h = (float) InstallerView::designHeight;
        const float scale = juce::jmin ((float) getWidth() / w, (float) getHeight() / h);

        view->setTransform (juce::AffineTransform::scale (scale)
                                .translated (0.5f * ((float) getWidth()  - scale * w),
                                             0.5f * ((float) getHeight() - scale * h)));
        view->sizeIsChanging();
    }

private:
    std::unique_ptr<InstallerView> view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerShell)
};

} // namespace daaura
