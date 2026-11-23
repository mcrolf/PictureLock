#pragma once

#include "plugin/PictureMoment.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace picturelock
{

class PictureLockProcessor;

/** Draws the current movie frame, letterboxed, with an optional timecode burn-in.
    Repaints on every display refresh while the frame or timecode changes.
*/
class PictureView : public juce::Component
{
public:
    explicit PictureView (PictureLockProcessor&);

    void paint (juce::Graphics&) override;

    std::function<void()> onDoubleClick;
    std::function<void (const juce::MouseEvent&)> onRightClick;
    std::function<void (const PictureMoment&)> onMomentChanged;

    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void refresh();
    juce::Rectangle<float> pictureBounds() const;
    void paintTimecodeBurnIn (juce::Graphics&, juce::Rectangle<float> picture);
    void paintEmptyState (juce::Graphics&);

    PictureLockProcessor& processor;
    PictureMoment moment;
    juce::VBlankAttachment vBlank;
};

} // namespace picturelock
