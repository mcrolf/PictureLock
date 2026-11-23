#include "PictureEngine.h"

namespace picturelock
{

namespace
{
    constexpr int idleWaitMilliseconds = 20;
    constexpr int stopTimeoutMilliseconds = 2000;
}

PictureEngine::PictureEngine() : juce::Thread ("PictureLock decoder") {}

PictureEngine::~PictureEngine()
{
    unload();
}

juce::Result PictureEngine::load (const juce::File& file)
{
    juce::String error;
    auto newDecoder = VideoDecoder::open (file, error);

    if (newDecoder == nullptr)
        return juce::Result::fail (error);

    unload();

    {
        const juce::ScopedLock lock (infoLock);
        loadedInfo = newDecoder->info();
    }

    decoder = std::move (newDecoder);
    lastUndecodableIndex = -1;
    startThread (juce::Thread::Priority::high);
    return juce::Result::ok();
}

void PictureEngine::unload()
{
    signalThreadShouldExit();
    requestArrived.signal();
    stopThread (stopTimeoutMilliseconds);

    decoder.reset();
    cache.clear();

    const juce::ScopedLock lock (infoLock);
    loadedInfo.reset();
}

std::optional<VideoInfo> PictureEngine::info() const
{
    const juce::ScopedLock lock (infoLock);
    return loadedInfo;
}

void PictureEngine::request (int64_t frameIndex, bool isPlaying)
{
    const auto previous = requestedIndex.exchange (frameIndex);
    requestedWhilePlaying = isPlaying;

    if (previous != frameIndex)
        requestArrived.signal();
}

std::optional<DecodedFrame> PictureEngine::frameNearest (int64_t frameIndex) const
{
    return cache.nearest (frameIndex);
}

void PictureEngine::run()
{
    while (! threadShouldExit())
    {
        const auto target = requestedIndex.load();
        const auto isPlaying = requestedWhilePlaying.load();

        if (! decodeMissingFrameNear (target, isPlaying))
            requestArrived.wait (idleWaitMilliseconds);
    }
}

bool PictureEngine::decodeMissingFrameNear (int64_t target, bool isPlaying)
{
    const auto lastWanted = target + (isPlaying ? framesToReadAhead : 0);

    for (auto index = target; index <= lastWanted; ++index)
    {
        if (cache.contains (index) || index == lastUndecodableIndex)
            continue;

        if (auto frame = decoder->decodeFrame (index))
            cache.store (std::move (*frame));
        else
            lastUndecodableIndex = index;

        cache.keepOnly (requestedIndex.load() - framesToKeepBehind, requestedIndex.load() + framesToReadAhead);
        return true;
    }

    return false;
}

} // namespace picturelock
