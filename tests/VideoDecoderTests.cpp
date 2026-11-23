#include "audio/GuideAudioExporter.h"
#include "video/VideoDecoder.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace picturelock
{

namespace
{
    juce::File fixture (const char* name)
    {
        return juce::File (PICTURELOCK_FIXTURES_DIR).getChildFile (name);
    }

    juce::int64 hashPixels (const juce::Image& image)
    {
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
        juce::MemoryBlock block;

        for (int y = 0; y < pixels.height; ++y)
            block.append (pixels.getLinePointer (y), (size_t) (pixels.width * pixels.pixelStride));

        return block.toBase64Encoding().hashCode64();
    }
}

class VideoDecoderTests : public juce::UnitTest
{
public:
    VideoDecoderTests() : juce::UnitTest ("VideoDecoder", "PictureLock") {}

    void runTest() override
    {
        testMovieInfo();
        testSeekingMatchesSequentialDecode ("h264_24fps_tc01.mov");
        testSeekingMatchesSequentialDecode ("prores_23976.mov");
        testColourChannelsSurviveConversion();
        testOutOfRangeFrames();
        testUnreadableFile();
        testGuideAudioExport();
    }

private:
    void testMovieInfo()
    {
        beginTest ("Movie info comes from the file");
        juce::String error;
        auto decoder = VideoDecoder::open (fixture ("h264_24fps_tc01.mov"), error);
        expect (decoder != nullptr, error);

        const auto& info = decoder->info();
        expectEquals (info.width, 320);
        expectEquals (info.height, 180);
        expectWithinAbsoluteError (info.framesPerSecond, 24.0, 1.0e-6);
        expectEquals (info.frameCount, (int64_t) 48);
        expect (info.embeddedTimecode == juce::String ("01:00:00:00"));
        expect (info.hasAudio);
    }

    void testSeekingMatchesSequentialDecode (const char* name)
    {
        beginTest (juce::String ("Seeking lands on the exact frame: ") + name);
        juce::String error;
        auto sequential = VideoDecoder::open (fixture (name), error);
        auto seeking = VideoDecoder::open (fixture (name), error);
        expect (sequential != nullptr && seeking != nullptr, error);

        std::vector<juce::int64> sequentialHashes;
        for (int64_t i = 0; i < sequential->info().frameCount; ++i)
        {
            auto frame = sequential->decodeFrame (i);
            expect (frame.has_value(), "sequential frame " + juce::String (i));
            sequentialHashes.push_back (frame ? hashPixels (frame->image) : 0);
        }

        expect (sequentialHashes.front() != sequentialHashes.back(), "frames differ");

        for (int64_t index : { 17, 3, 23, 22, 0, 13, 5, 11, 12, 1 })
        {
            if (index >= (int64_t) sequentialHashes.size())
                continue;

            auto frame = seeking->decodeFrame (index);
            expect (frame.has_value() && frame->index == index, "seek to " + juce::String (index));
            expect (frame && hashPixels (frame->image) == sequentialHashes[(size_t) index], "pixels of frame " + juce::String (index));
        }
    }

    void testColourChannelsSurviveConversion()
    {
        beginTest ("Decoded frames keep red as red when converted for drawing");
        juce::String error;
        auto decoder = VideoDecoder::open (fixture ("red_h264.mp4"), error);
        expect (decoder != nullptr, error);

        auto frame = decoder->decodeFrame (0);
        expect (frame.has_value());

        if (frame)
        {
            const auto pixel = frame->image.getPixelAt (frame->image.getWidth() / 2, frame->image.getHeight() / 2);
            expectGreaterThan ((int) pixel.getRed(), 240);
            expectLessThan ((int) pixel.getGreen(), 16);
            expectLessThan ((int) pixel.getBlue(), 16);
            expectEquals ((int) pixel.getAlpha(), 255);
        }
    }

    void testOutOfRangeFrames()
    {
        beginTest ("Frames outside the movie are not decoded");
        juce::String error;
        auto decoder = VideoDecoder::open (fixture ("h264_24fps_tc01.mov"), error);
        expect (! decoder->decodeFrame (-1).has_value());
        expect (! decoder->decodeFrame (48).has_value());
        expect (decoder->decodeFrame (47).has_value());
    }

    void testUnreadableFile()
    {
        beginTest ("Opening a missing file explains why it failed");
        juce::String error;
        expect (VideoDecoder::open (fixture ("does-not-exist.mov"), error) == nullptr);
        expect (error.isNotEmpty());
    }

    void testGuideAudioExport()
    {
        beginTest ("Guide audio export writes the movie's audio at the session rate");
        const auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("picturelock-guide-test.wav");
        const auto result = GuideAudioExporter::exportToWav (fixture ("h264_24fps_tc01.mov"), wav, 44100.0, {});
        expect (result.wasOk(), result.getErrorMessage());

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (new juce::FileInputStream (wav), true));
        expect (reader != nullptr);

        if (reader != nullptr)
        {
            expectEquals (reader->sampleRate, 44100.0);
            expectEquals ((int) reader->numChannels, 2);
            expectWithinAbsoluteError ((double) reader->lengthInSamples / 44100.0, 2.0, 0.05);
        }

        reader.reset();
        wav.deleteFile();

        beginTest ("Guide audio export fails clearly for a movie without audio");
        expect (GuideAudioExporter::exportToWav (fixture ("prores_23976.mov"), wav, 48000.0, {}).failed());
    }
};

static VideoDecoderTests videoDecoderTests;

} // namespace picturelock
