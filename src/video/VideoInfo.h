#pragma once

#include <juce_graphics/juce_graphics.h>
#include <optional>

namespace picturelock
{

/** What PictureLock knows about an opened movie file. */
struct VideoInfo
{
    juce::File file;
    juce::String codecName;
    int width = 0;
    int height = 0;
    double displayAspectRatio = 16.0 / 9.0; // accounts for anamorphic pixel aspect
    double framesPerSecond = 24.0;
    int64_t frameCount = 0;
    std::optional<juce::String> embeddedTimecode; // the file's start timecode, e.g. "01:00:00:00"
    bool hasAudio = false;

    double durationSeconds() const { return (double) frameCount / framesPerSecond; }
};

/** One decoded movie frame, ready to draw. */
struct DecodedFrame
{
    int64_t index = -1;
    juce::Image image;
};

} // namespace picturelock
