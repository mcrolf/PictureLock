#include "video/VideoDecoder.h"

namespace picturelock
{

/** Measures sequential and seek decode speed for a real movie.
    Opt-in: set PICTURELOCK_BENCHMARK_MOVIE=/path/to/movie.mov before running the tests. */
class DecodeBenchmark : public juce::UnitTest
{
public:
    DecodeBenchmark() : juce::UnitTest ("DecodeBenchmark", "Benchmark") {}

    void runTest() override
    {
        const auto path = juce::SystemStats::getEnvironmentVariable ("PICTURELOCK_BENCHMARK_MOVIE", {});

        if (path.isEmpty())
            return;

        beginTest ("Decode speed: " + juce::File (path).getFileName());
        juce::String error;
        auto decoder = VideoDecoder::open (juce::File (path), error);
        expect (decoder != nullptr, error);

        if (decoder == nullptr)
            return;

        const auto& info = decoder->info();
        const auto frames = std::min<int64_t> (info.frameCount, 240);

        const auto sequentialStart = juce::Time::getMillisecondCounterHiRes();
        for (int64_t i = 0; i < frames; ++i)
            decoder->decodeFrame (i);
        const auto sequentialFramesPerSecond = frames / ((juce::Time::getMillisecondCounterHiRes() - sequentialStart) / 1000.0);

        juce::Random random (1);
        constexpr int seeks = 20;
        const auto seekStart = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < seeks; ++i)
            decoder->decodeFrame ((int64_t) (random.nextDouble() * (double) info.frameCount));
        const auto millisecondsPerSeek = (juce::Time::getMillisecondCounterHiRes() - seekStart) / seeks;

        logMessage (info.codecName + " " + juce::String (info.width) + "x" + juce::String (info.height)
                    + " @ " + juce::String (info.framesPerSecond, 3) + " fps: sequential "
                    + juce::String (sequentialFramesPerSecond, 1) + " frames/s, random seek "
                    + juce::String (millisecondsPerSeek, 1) + " ms");
    }
};

static DecodeBenchmark decodeBenchmark;

} // namespace picturelock
