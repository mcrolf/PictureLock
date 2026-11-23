#pragma once

#include "PictureView.h"

namespace picturelock
{

/** The floating picture window. It never takes keyboard focus, so Live keeps the
    spacebar and other shortcuts while the window is in front.
*/
class VideoWindow : public juce::DocumentWindow
{
public:
    explicit VideoWindow (PictureLockProcessor&);
    ~VideoWindow() override;

    void closeButtonPressed() override;
    void moved() override;
    void resized() override;
    int getDesktopWindowStyleFlags() const override;

private:
    void showOptionsMenu();
    void toggleFullScreen();
    void rememberBounds();
    juce::Rectangle<int> defaultBounds() const;

    PictureLockProcessor& processor;
    PictureView* pictureView = nullptr;
};

} // namespace picturelock
