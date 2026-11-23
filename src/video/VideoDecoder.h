#pragma once

#include "VideoInfo.h"

#include <memory>

namespace picturelock
{

/** Frame-accurate decoding of one video stream with FFmpeg.

    Not thread-safe: one thread owns a decoder at a time. Sequential requests decode
    straight through; anything else seeks to the preceding keyframe and decodes
    forward to the exact frame.
*/
class VideoDecoder
{
public:
    /** Opens a movie, or returns nullptr and describes the failure in `error`. */
    static std::unique_ptr<VideoDecoder> open (const juce::File& file, juce::String& error);

    ~VideoDecoder();

    const VideoInfo& info() const;

    /** Decodes the frame at `index`. Returns nullopt outside the movie or on a decode error.
        Frames are scaled to fit within `maximumWidth` pixels to bound memory use. */
    std::optional<DecodedFrame> decodeFrame (int64_t index);

    /** Index the next sequential decode will produce; requests here are cheapest. */
    int64_t nextSequentialIndex() const;

    static constexpr int maximumWidth = 1920;

private:
    struct Impl;
    explicit VideoDecoder (std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl;
};

} // namespace picturelock
