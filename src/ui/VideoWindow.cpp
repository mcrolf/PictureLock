#include "VideoWindow.h"

#include "LookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace picturelock
{

VideoWindow::VideoWindow (PictureLockProcessor& p)
    : juce::DocumentWindow ("PictureLock", juce::Colours::black, juce::DocumentWindow::closeButton, false),
      processor (p)
{
    auto view = std::make_unique<PictureView> (processor);
    view->onDoubleClick = [this] { toggleFullScreen(); };
    view->onRightClick = [this] (const juce::MouseEvent&) { showOptionsMenu(); };
    pictureView = view.get();

    setUsingNativeTitleBar (true);
    setContentOwned (view.release(), false);
    setResizable (true, false);
    setResizeLimits (160, 90, 16384, 16384);

    const auto saved = processor.settings().videoWindow().bounds;
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (saved);
    setBounds (! saved.isEmpty() && display != nullptr ? saved : defaultBounds());

    addToDesktop (getDesktopWindowStyleFlags());
    setAlwaysOnTop (processor.settings().videoWindow().isAlwaysOnTop);
    setVisible (true);
}

VideoWindow::~VideoWindow() = default;

int VideoWindow::getDesktopWindowStyleFlags() const
{
    return juce::DocumentWindow::getDesktopWindowStyleFlags() | juce::ComponentPeer::windowIgnoresKeyPresses;
}

juce::Rectangle<int> VideoWindow::defaultBounds() const
{
    const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds.toNearestInt();
    return area.withSizeKeepingCentre (960, 540 + getTitleBarHeight());
}

void VideoWindow::closeButtonPressed()
{
    // Destroys this window via the settings change.
    processor.setVideoWindowVisible (false);
}

void VideoWindow::moved()
{
    juce::DocumentWindow::moved();
    rememberBounds();
}

void VideoWindow::resized()
{
    juce::DocumentWindow::resized();
    rememberBounds();
}

void VideoWindow::rememberBounds()
{
    if (! isShowing() || isFullScreen() || isMinimised())
        return;

    auto state = processor.settings().videoWindow();

    if (state.bounds != getBounds())
    {
        state.bounds = getBounds();
        processor.settings().setVideoWindow (state);
    }
}

void VideoWindow::toggleFullScreen()
{
    setFullScreen (! isFullScreen());
}

void VideoWindow::showOptionsMenu()
{
    auto& settings = processor.settings();
    auto state = settings.videoWindow();

    juce::PopupMenu menu;
    menu.addItem ("Full Screen", true, isFullScreen(), [this] { toggleFullScreen(); });
    menu.addItem ("Always on Top", true, state.isAlwaysOnTop, [&settings, state]() mutable
    {
        state.isAlwaysOnTop = ! state.isAlwaysOnTop;
        settings.setVideoWindow (state);
    });
    menu.addItem ("Timecode Burn-in", true, settings.showsTimecodeOverlay(), [&settings]
    {
        settings.setShowsTimecodeOverlay (! settings.showsTimecodeOverlay());
    });
    menu.addSeparator();

    if (auto info = processor.movieInfo())
    {
        menu.addItem ("Actual Size (" + juce::String (info->width) + " x " + juce::String (info->height) + ")", [this, info]
        {
            const auto width = std::min (info->width, VideoDecoder::maximumWidth);
            setContentComponentSize (width, (int) std::lround (width / info->displayAspectRatio));
        });
        menu.addItem ("Half Size", [this, info]
        {
            const auto width = info->width / 2;
            setContentComponentSize (width, (int) std::lround (width / info->displayAspectRatio));
        });
        menu.addSeparator();
    }

    menu.addItem ("Close Video Window", [this] { closeButtonPressed(); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (pictureView).withMousePosition());
}

} // namespace picturelock
