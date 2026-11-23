#pragma once

#include "FrameCache.h"
#include "VideoDecoder.h"

#include <juce_core/juce_core.h>
#include <atomic>

namespace picturelock
{

/** Owns the movie decoder and a background thread that keeps the frames around the
    requested position decoded, reading ahead while the transport plays.
*/
class PictureEngine : private juce::Thread
{
public:
    PictureEngine();
    ~PictureEngine() override;

    /** Message thread. Replaces the current movie. */
    juce::Result load (const juce::File&);
    void unload();

    std::optional<VideoInfo> info() const;

    /** Any thread. Tells the engine which frame is needed now. */
    void request (int64_t frameIndex, bool isPlaying);

    /** The decoded frame for `frameIndex`, or the nearest one available while it decodes. */
    std::optional<DecodedFrame> frameNearest (int64_t frameIndex) const;

    static constexpr int framesToReadAhead = 8;
    static constexpr int framesToKeepBehind = 2;

private:
    void run() override;
    bool decodeMissingFrameNear (int64_t target, bool isPlaying);

    std::unique_ptr<VideoDecoder> decoder; // only touched by the decode thread while it runs
    std::optional<VideoInfo> loadedInfo;
    mutable juce::CriticalSection infoLock;
    FrameCache cache;
    juce::WaitableEvent requestArrived;
    std::atomic<int64_t> requestedIndex { 0 };
    std::atomic<bool> requestedWhilePlaying { false };
    int64_t lastUndecodableIndex = -1;
};

} // namespace picturelock
