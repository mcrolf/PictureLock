#include "TransportClock.h"

#include <algorithm>

namespace picturelock
{

namespace
{
    // If the host stops delivering blocks (device stalled, plugin bypassed), stop
    // extrapolating rather than letting the picture run away from the audio.
    constexpr double maximumExtrapolationMilliseconds = 250.0;
}

void TransportClock::publish (double newSongSeconds, bool newIsPlaying, double newWallMilliseconds)
{
    sequence.fetch_add (1, std::memory_order_acq_rel);
    songSeconds.store (newSongSeconds, std::memory_order_relaxed);
    wallMilliseconds.store (newWallMilliseconds, std::memory_order_relaxed);
    isPlaying.store (newIsPlaying, std::memory_order_relaxed);
    sequence.fetch_add (1, std::memory_order_acq_rel);

    if (publishCount.load (std::memory_order_relaxed) == 0)
        publishCount.store (1, std::memory_order_release);
}

TransportClock::Sample TransportClock::readConsistentSample() const
{
    for (;;)
    {
        const auto before = sequence.load (std::memory_order_acquire);

        if ((before & 1u) != 0)
            continue;

        Sample sample { songSeconds.load (std::memory_order_relaxed),
                        wallMilliseconds.load (std::memory_order_relaxed),
                        isPlaying.load (std::memory_order_relaxed) };

        std::atomic_thread_fence (std::memory_order_acquire);

        if (sequence.load (std::memory_order_relaxed) == before)
            return sample;
    }
}

TransportClock::Position TransportClock::positionAt (double wallMillisecondsNow) const
{
    const auto sample = readConsistentSample();

    if (! sample.isPlaying)
        return { sample.songSeconds, false };

    const auto elapsed = std::clamp (wallMillisecondsNow - sample.wallMilliseconds, 0.0, maximumExtrapolationMilliseconds);
    return { sample.songSeconds + elapsed / 1000.0, true };
}

} // namespace picturelock
