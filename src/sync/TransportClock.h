#pragma once

#include <atomic>
#include <cstdint>

namespace picturelock
{

/** The host's song position, published by the audio thread and read by the UI.

    The audio thread calls publish() once per block. Readers extrapolate between
    blocks using the wall clock so the picture advances smoothly at the display's
    refresh rate rather than in block-sized steps.
*/
class TransportClock
{
public:
    struct Position
    {
        double songSeconds = 0.0;
        bool isPlaying = false;
    };

    /** Audio thread. Wait-free. */
    void publish (double songSeconds, bool isPlaying, double wallMilliseconds);

    /** Any thread. The extrapolated song position at a wall-clock time. */
    Position positionAt (double wallMilliseconds) const;

    bool hasReceivedPosition() const { return publishCount.load (std::memory_order_acquire) > 0; }

private:
    struct Sample { double songSeconds, wallMilliseconds; bool isPlaying; };
    Sample readConsistentSample() const;

    // Sequence lock: odd while the writer is mid-update.
    std::atomic<uint32_t> sequence { 0 };
    std::atomic<uint32_t> publishCount { 0 };
    std::atomic<double> songSeconds { 0.0 };
    std::atomic<double> wallMilliseconds { 0.0 };
    std::atomic<bool> isPlaying { false };
};

} // namespace picturelock
