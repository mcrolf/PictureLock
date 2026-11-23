#include "plugin/PluginProcessor.h"

namespace picturelock
{

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    struct FakeHostPlayHead : juce::AudioPlayHead
    {
        double songSeconds = 0.0;
        bool isPlaying = false;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setTimeInSamples ((int64_t) std::llround (songSeconds * sampleRate));
            info.setIsPlaying (isPlaying);
            return info;
        }
    };

    juce::File movieFixture()
    {
        return juce::File (PICTURELOCK_FIXTURES_DIR).getChildFile ("h264_24fps_tc01.mov");
    }

    void runMessageLoopBriefly()
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    }
}

/** Drives the processor like a host does: playhead position, audio block, then the UI
    asking for the picture. */
class ProcessorSyncTests : public juce::UnitTest
{
public:
    ProcessorSyncTests() : juce::UnitTest ("ProcessorSync", "PictureLock") {}

    void runTest() override
    {
        PictureLockProcessor processor;
        FakeHostPlayHead host;
        processor.setPlayHead (&host);
        processor.prepareToPlay (sampleRate, blockSize);

        beginTest ("Without a host position the processor says so");
        expect (! processor.advancePicture().hasHostTransport);

        beginTest ("Loading a movie with start timecode places the picture at song position 0");
        expect (processor.loadMovie (movieFixture()).wasOk());
        runMessageLoopBriefly();
        expectEquals (Timecode (processor.settings().placement().pictureStartFrame, FrameRate::fromId (FrameRate::Id::fps24)).toString(),
                      juce::String ("01:00:00:00"));
        expectWithinAbsoluteError (processor.settings().placement().songSecondsAtPictureStart(), 0.0, 1.0e-9);

        beginTest ("A stopped playhead shows the exact frame for its song position");
        expectEquals (frameShownAt (processor, host, 1.0), (int64_t) 24);
        expectEquals (processor.advancePicture().sessionTimecode.toString(), juce::String ("01:00:01:00"));
        expectEquals (frameShownAt (processor, host, 0.5), (int64_t) 12);
        expectEquals (frameShownAt (processor, host, 1.99), (int64_t) 47);

        beginTest ("Moving the picture start moves the picture, not the session timecode");
        processor.settings().setPictureStart (*Timecode::parse ("01:00:00:12", FrameRate::fromId (FrameRate::Id::fps24)));
        expectEquals (frameShownAt (processor, host, 1.0), (int64_t) 12);
        expectEquals (processor.advancePicture().sessionTimecode.toString(), juce::String ("01:00:01:00"));

        beginTest ("Before the picture start nothing is drawn");
        expectEquals (frameShownAt (processor, host, 0.25), (int64_t) -1);

        beginTest ("A sync offset delays the picture");
        processor.settings().setPictureStart (*Timecode::parse ("01:00:00:00", FrameRate::fromId (FrameRate::Id::fps24)));
        processor.settings().setSyncOffsetMilliseconds (500.0);
        expectEquals (frameShownAt (processor, host, 1.0), (int64_t) 12);
        processor.settings().setSyncOffsetMilliseconds (0.0);

        beginTest ("While playing, the picture advances between audio blocks");
        host.isPlaying = true;
        host.songSeconds = 0.5;
        processBlock (processor);
        const auto startFrame = processor.advancePicture().frameIndex;
        juce::Thread::sleep (100);
        const auto laterFrame = processor.advancePicture().frameIndex;
        expect (startFrame >= 12 && startFrame <= 13, "start frame " + juce::String (startFrame));
        expect (laterFrame >= startFrame + 2 && laterFrame <= startFrame + 4, "later frame " + juce::String (laterFrame));
        host.isPlaying = false;

        beginTest ("Saved state restores the movie and placement");
        processor.settings().setSyncOffsetMilliseconds (40.0);
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        PictureLockProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        runMessageLoopBriefly();
        expect (restored.movieInfo().has_value());
        expect (restored.settings().movieFile() == movieFixture());
        expectWithinAbsoluteError (restored.settings().syncOffsetMilliseconds(), 40.0, 1.0e-9);
    }

private:
    static void processBlock (PictureLockProcessor& processor)
    {
        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);
    }

    /** The decoded frame on screen at a stopped song position, once decoding catches up. */
    static int64_t frameShownAt (PictureLockProcessor& processor, FakeHostPlayHead& host, double songSeconds)
    {
        host.songSeconds = songSeconds;
        processBlock (processor);

        for (int attempt = 0; attempt < 200; ++attempt)
        {
            auto moment = processor.advancePicture();

            if (moment.frameIndex < 0)
                return -1;

            if (moment.frame && moment.frame->index == moment.frameIndex)
                return moment.frame->index;

            juce::Thread::sleep (5);
        }

        return -2;
    }
};

static ProcessorSyncTests processorSyncTests;

} // namespace picturelock
