#include "GuideAudioExporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <cmath>

namespace picturelock
{

namespace
{
    constexpr int outputChannels = 2;
    constexpr int outputBitsPerSample = 24;

    struct FormatContextDeleter { void operator() (AVFormatContext* c) const { avformat_close_input (&c); } };
    struct CodecContextDeleter  { void operator() (AVCodecContext* c) const  { avcodec_free_context (&c); } };
    struct FrameDeleter         { void operator() (AVFrame* f) const         { av_frame_free (&f); } };
    struct PacketDeleter        { void operator() (AVPacket* p) const        { av_packet_free (&p); } };
    struct ResamplerDeleter     { void operator() (SwrContext* s) const      { swr_free (&s); } };

    /** Decodes one audio stream, resamples it to stereo float, and writes it out. */
    class AudioTranscode
    {
    public:
        AudioTranscode (double sampleRateToWrite, juce::AudioFormatWriter& destination)
            : sampleRate (sampleRateToWrite), writer (destination) {}

        juce::Result run (const juce::File& movie, const GuideAudioExporter::ProgressCallback& progress)
        {
            if (auto result = openStreams (movie); result.failed())
                return result;

            while (av_read_frame (format.get(), packet.get()) >= 0)
            {
                if (packet->stream_index == audioStream->index)
                {
                    avcodec_send_packet (codec.get(), packet.get());
                    writeDecodedFrames();
                    reportedSeconds = (double) packet->pts * av_q2d (audioStream->time_base);
                }

                av_packet_unref (packet.get());

                if (progress && ! progress (durationSeconds > 0 ? juce::jlimit (0.0, 1.0, reportedSeconds / durationSeconds) : 0.0))
                    return juce::Result::fail ("Export cancelled");
            }

            avcodec_send_packet (codec.get(), nullptr);
            writeDecodedFrames();
            flushResampler();
            return juce::Result::ok();
        }

    private:
        juce::Result openStreams (const juce::File& movie)
        {
            AVFormatContext* rawFormat = nullptr;

            if (avformat_open_input (&rawFormat, movie.getFullPathName().toRawUTF8(), nullptr, nullptr) < 0)
                return juce::Result::fail ("Could not open " + movie.getFileName());

            format.reset (rawFormat);
            avformat_find_stream_info (format.get(), nullptr);

            const auto audioIndex = av_find_best_stream (format.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

            if (audioIndex < 0)
                return juce::Result::fail (movie.getFileName() + " has no audio");

            audioStream = format->streams[audioIndex];
            const auto videoIndex = av_find_best_stream (format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            pictureStartSeconds = videoIndex >= 0 ? streamStartSeconds (*format->streams[videoIndex]) : 0.0;
            durationSeconds = format->duration != AV_NOPTS_VALUE ? (double) format->duration / AV_TIME_BASE : 0.0;

            for (unsigned i = 0; i < format->nb_streams; ++i)
                if ((int) i != audioIndex)
                    format->streams[i]->discard = AVDISCARD_ALL;

            return openDecoderAndResampler();
        }

        juce::Result openDecoderAndResampler()
        {
            const auto* decoder = avcodec_find_decoder (audioStream->codecpar->codec_id);

            if (decoder == nullptr)
                return juce::Result::fail ("Unsupported audio codec: " + juce::String (avcodec_get_name (audioStream->codecpar->codec_id)));

            codec.reset (avcodec_alloc_context3 (decoder));
            avcodec_parameters_to_context (codec.get(), audioStream->codecpar);

            if (avcodec_open2 (codec.get(), decoder, nullptr) < 0)
                return juce::Result::fail ("Could not open the audio decoder");

            AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
            SwrContext* rawResampler = nullptr;

            if (swr_alloc_set_opts2 (&rawResampler, &stereo, AV_SAMPLE_FMT_FLTP, (int) sampleRate,
                                     &codec->ch_layout, codec->sample_fmt, codec->sample_rate, 0, nullptr) < 0
                || swr_init (rawResampler) < 0)
            {
                swr_free (&rawResampler);
                return juce::Result::fail ("Could not set up audio resampling");
            }

            resampler.reset (rawResampler);
            return juce::Result::ok();
        }

        static double streamStartSeconds (const AVStream& stream)
        {
            return stream.start_time != AV_NOPTS_VALUE ? (double) stream.start_time * av_q2d (stream.time_base) : 0.0;
        }

        void writeDecodedFrames()
        {
            while (avcodec_receive_frame (codec.get(), frame.get()) == 0)
            {
                if (! hasAlignedToPicture)
                    alignToPicture();

                const auto capacity = swr_get_out_samples (resampler.get(), frame->nb_samples);
                buffer.setSize (outputChannels, std::max (capacity, 1), false, false, true);
                const auto produced = swr_convert (resampler.get(), outputPointers(), capacity,
                                                   (const uint8_t* const*) frame->extended_data, frame->nb_samples);
                writeResampled (produced);
            }
        }

        // Pads or trims the start so sample 0 of the WAV lines up with picture frame 0.
        void alignToPicture()
        {
            hasAlignedToPicture = true;
            const auto pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
            const auto audioStartSeconds = pts != AV_NOPTS_VALUE ? (double) pts * av_q2d (audioStream->time_base) : 0.0;
            const auto offsetSamples = (int64_t) std::llround ((audioStartSeconds - pictureStartSeconds) * sampleRate);

            if (offsetSamples > 0)
                writeSilence (offsetSamples);
            else
                samplesToSkip = -offsetSamples;
        }

        void writeSilence (int64_t count)
        {
            buffer.setSize (outputChannels, (int) std::min<int64_t> (count, 65536), false, false, true);
            buffer.clear();

            for (int64_t remaining = count; remaining > 0; remaining -= buffer.getNumSamples())
                writer.writeFromAudioSampleBuffer (buffer, 0, (int) std::min<int64_t> (remaining, buffer.getNumSamples()));
        }

        void flushResampler()
        {
            buffer.setSize (outputChannels, 4096, false, false, true);
            writeResampled (swr_convert (resampler.get(), outputPointers(), buffer.getNumSamples(), nullptr, 0));
        }

        void writeResampled (int produced)
        {
            const auto skipped = (int) std::min<int64_t> (samplesToSkip, std::max (produced, 0));
            samplesToSkip -= skipped;

            if (produced - skipped > 0)
                writer.writeFromAudioSampleBuffer (buffer, skipped, produced - skipped);
        }

        uint8_t* const* outputPointers()
        {
            return reinterpret_cast<uint8_t* const*> (buffer.getArrayOfWritePointers());
        }

        double sampleRate;
        juce::AudioFormatWriter& writer;
        juce::AudioBuffer<float> buffer;
        std::unique_ptr<AVFormatContext, FormatContextDeleter> format;
        std::unique_ptr<AVCodecContext, CodecContextDeleter> codec;
        std::unique_ptr<SwrContext, ResamplerDeleter> resampler;
        std::unique_ptr<AVFrame, FrameDeleter> frame { av_frame_alloc() };
        std::unique_ptr<AVPacket, PacketDeleter> packet { av_packet_alloc() };
        AVStream* audioStream = nullptr;
        double pictureStartSeconds = 0.0, durationSeconds = 0.0, reportedSeconds = 0.0;
        bool hasAlignedToPicture = false;
        int64_t samplesToSkip = 0;
    };
}

juce::Result GuideAudioExporter::exportToWav (const juce::File& movie, const juce::File& wav,
                                              double sampleRate, const ProgressCallback& progress)
{
    wav.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (wav);

    if (static_cast<juce::FileOutputStream*> (stream.get())->failedToOpen())
        return juce::Result::fail ("Could not write " + wav.getFullPathName());

    juce::WavAudioFormat wavFormat;
    auto writer = wavFormat.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                         .withSampleRate (sampleRate)
                                                         .withNumChannels (outputChannels)
                                                         .withBitsPerSample (outputBitsPerSample));
    if (writer == nullptr)
        return juce::Result::fail ("Could not create the WAV writer");

    auto result = AudioTranscode (sampleRate, *writer).run (movie, progress);
    writer.reset();

    if (result.failed())
        wav.deleteFile();

    return result;
}

} // namespace picturelock
