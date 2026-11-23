#pragma once

#include "VideoInfo.h"

#include <map>
#include <mutex>

namespace picturelock
{

/** Decoded frames around the playhead, shared between the decode thread and the UI. */
class FrameCache
{
public:
    void store (DecodedFrame);
    bool contains (int64_t index) const;

    /** The frame at `index`, else the closest earlier one, else the closest later one. */
    std::optional<DecodedFrame> nearest (int64_t index) const;

    /** Drops frames outside [first, last]. */
    void keepOnly (int64_t first, int64_t last);
    void clear();

private:
    mutable std::mutex mutex;
    std::map<int64_t, juce::Image> frames;
};

} // namespace picturelock
