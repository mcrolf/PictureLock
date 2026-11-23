#include "FrameCache.h"

namespace picturelock
{

void FrameCache::store (DecodedFrame frame)
{
    std::scoped_lock lock (mutex);
    frames[frame.index] = std::move (frame.image);
}

bool FrameCache::contains (int64_t index) const
{
    std::scoped_lock lock (mutex);
    return frames.count (index) > 0;
}

std::optional<DecodedFrame> FrameCache::nearest (int64_t index) const
{
    std::scoped_lock lock (mutex);

    if (frames.empty())
        return std::nullopt;

    auto atOrAfter = frames.lower_bound (index);

    if (atOrAfter != frames.end() && atOrAfter->first == index)
        return DecodedFrame { atOrAfter->first, atOrAfter->second };

    if (atOrAfter != frames.begin())
    {
        auto before = std::prev (atOrAfter);
        return DecodedFrame { before->first, before->second };
    }

    return DecodedFrame { atOrAfter->first, atOrAfter->second };
}

void FrameCache::keepOnly (int64_t first, int64_t last)
{
    std::scoped_lock lock (mutex);
    frames.erase (frames.begin(), frames.lower_bound (first));
    frames.erase (frames.upper_bound (last), frames.end());
}

void FrameCache::clear()
{
    std::scoped_lock lock (mutex);
    frames.clear();
}

} // namespace picturelock
