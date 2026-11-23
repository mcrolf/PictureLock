#include "PictureView.h"

#include "LookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace picturelock
{

PictureView::PictureView (PictureLockProcessor& p)
    : processor (p), vBlank (this, [this] { refresh(); })
{
    setOpaque (true);
}

void PictureView::refresh()
{
    auto next = processor.advancePicture();

    const auto frameChanged = (next.frame ? next.frame->index : -1) != (moment.frame ? moment.frame->index : -1)
                           || next.frameIndex != moment.frameIndex;
    const auto timecodeChanged = next.sessionTimecode.frameCount() != moment.sessionTimecode.frameCount();
    const auto transportChanged = next.transport.isPlaying != moment.transport.isPlaying
                               || next.hasHostTransport != moment.hasHostTransport;

    moment = std::move (next);

    if (frameChanged || timecodeChanged || transportChanged)
    {
        repaint();

        if (onMomentChanged)
            onMomentChanged (moment);
    }
}

juce::Rectangle<float> PictureView::pictureBounds() const
{
    const auto info = processor.movieInfo();
    const auto aspect = info ? info->displayAspectRatio : 16.0 / 9.0;
    const auto area = getLocalBounds().toFloat();
    const auto width = std::min (area.getWidth(), area.getHeight() * (float) aspect);
    return area.withSizeKeepingCentre (width, width / (float) aspect);
}

void PictureView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (! processor.movieInfo())
    {
        paintEmptyState (g);
        return;
    }

    const auto picture = pictureBounds();

    if (moment.frameIndex >= 0 && moment.frame)
    {
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (moment.frame->image, picture);
    }

    if (processor.settings().showsTimecodeOverlay())
        paintTimecodeBurnIn (g, picture);
}

void PictureView::paintTimecodeBurnIn (juce::Graphics& g, juce::Rectangle<float> picture)
{
    const auto height = juce::jlimit (14.0f, 56.0f, picture.getHeight() * 0.07f);
    const auto font = PictureLockLookAndFeel::timecodeFont (height);
    const auto text = moment.sessionTimecode.toString();
    const auto width = juce::GlyphArrangement::getStringWidth (font, text) + height;

    auto box = juce::Rectangle<float> (width, height * 1.3f)
                   .withCentre ({ picture.getCentreX(), picture.getBottom() - height * 1.2f });

    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.fillRect (box);
    g.setColour (juce::Colours::white);
    g.setFont (font);
    g.drawText (text, box, juce::Justification::centred, false);
}

void PictureView::paintEmptyState (juce::Graphics& g)
{
    g.setColour (palette::mutedText);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Drop a movie file here, or use Open Movie.", getLocalBounds().reduced (20),
                      juce::Justification::centred, 3);
}

void PictureView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}

void PictureView::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && onRightClick)
        onRightClick (event);
}

} // namespace picturelock
