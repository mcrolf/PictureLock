#pragma once

#include "sync/TransportClock.h"
#include "timecode/Timecode.h"
#include "video/VideoInfo.h"

namespace picturelock
{

/** What should be on screen right now: the session timecode and the movie frame. */
struct PictureMoment
{
    TransportClock::Position transport;
    Timecode sessionTimecode { 0, FrameRate::fromId (FrameRate::Id::fps24) };
    int64_t frameIndex = -1;            // -1 when the playhead is outside the picture
    std::optional<DecodedFrame> frame;  // the best decoded frame available for frameIndex
    bool hasHostTransport = false;
};

} // namespace picturelock
