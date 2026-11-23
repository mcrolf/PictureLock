#include "VideoDecoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <cmath>

namespace picturelock
{

namespace
{
    // Decoding forward through this many frames is cheaper than a seek for most codecs.
    constexpr int64_t maximumFramesToDecodeInsteadOfSeeking = 48;
    constexpr int maximumSeekAttempts = 4;

    juce::String describeError (int errorCode)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror (errorCode, buffer, sizeof (buffer));
        return buffer;
    }

    struct FormatContextDeleter { void operator() (AVFormatContext* c) const { avformat_close_input (&c); } };
    struct CodecContextDeleter  { void operator() (AVCodecContext* c) const  { avcodec_free_context (&c); } };
    struct FrameDeleter         { void operator() (AVFrame* f) const         { av_frame_free (&f); } };
    struct PacketDeleter        { void operator() (AVPacket* p) const        { av_packet_free (&p); } };
    struct ScalerDeleter        { void operator() (SwsContext* s) const      { sws_freeContext (s); } };

    std::optional<juce::String> findEmbeddedTimecode (const AVFormatContext& format)
    {
        auto lookUp = [] (const AVDictionary* metadata) -> std::optional<juce::String>
        {
            if (auto* entry = av_dict_get (metadata, "timecode", nullptr, 0))
                return juce::String (entry->value);

            return std::nullopt;
        };

        for (unsigned i = 0; i < format.nb_streams; ++i)
            if (auto timecode = lookUp (format.streams[i]->metadata))
                return timecode;

        return lookUp (format.metadata);
    }

    double streamFramesPerSecond (const AVStream& stream)
    {
        for (auto rate : { stream.avg_frame_rate, stream.r_frame_rate })
            if (rate.num > 0 && rate.den > 0)
                return av_q2d (rate);

        return 24.0;
    }
}

struct VideoDecoder::Impl
{
    std::unique_ptr<AVFormatContext, FormatContextDeleter> format;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> codec;
    std::unique_ptr<AVFrame, FrameDeleter> frame { av_frame_alloc() };
    std::unique_ptr<AVPacket, PacketDeleter> packet { av_packet_alloc() };
    std::unique_ptr<SwsContext, ScalerDeleter> scaler;
    AVStream* stream = nullptr;
    int64_t streamStartPts = 0;
    int64_t nextIndex = 0;
    bool hasReachedEndOfFile = false;
    VideoInfo info;

    juce::Result open (const juce::File& file);
    std::optional<DecodedFrame> decodeFrame (int64_t index);

private:
    juce::Result openCodec();
    void describeStream (const juce::File& file);
    void seekBefore (int64_t index, int attempt);
    bool decodeNextFrame();
    int64_t indexOfDecodedFrame() const;
    int64_t ptsForIndex (int64_t index) const;
    juce::Image convertDecodedFrame();
};

juce::Result VideoDecoder::Impl::open (const juce::File& file)
{
    AVFormatContext* rawFormat = nullptr;

    if (auto result = avformat_open_input (&rawFormat, file.getFullPathName().toRawUTF8(), nullptr, nullptr); result < 0)
        return juce::Result::fail ("Could not open " + file.getFileName() + ": " + describeError (result));

    format.reset (rawFormat);

    if (auto result = avformat_find_stream_info (format.get(), nullptr); result < 0)
        return juce::Result::fail ("Could not read stream info: " + describeError (result));

    const auto streamIndex = av_find_best_stream (format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (streamIndex < 0)
        return juce::Result::fail (file.getFileName() + " has no video stream");

    stream = format->streams[streamIndex];

    for (unsigned i = 0; i < format->nb_streams; ++i)
        if ((int) i != streamIndex)
            format->streams[i]->discard = AVDISCARD_ALL;

    if (auto result = openCodec(); result.failed())
        return result;

    describeStream (file);
    return juce::Result::ok();
}

juce::Result VideoDecoder::Impl::openCodec()
{
    const auto* decoder = avcodec_find_decoder (stream->codecpar->codec_id);

    if (decoder == nullptr)
        return juce::Result::fail ("Unsupported video codec: " + juce::String (avcodec_get_name (stream->codecpar->codec_id)));

    codec.reset (avcodec_alloc_context3 (decoder));
    avcodec_parameters_to_context (codec.get(), stream->codecpar);
    codec->thread_count = 0;
    codec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (auto result = avcodec_open2 (codec.get(), decoder, nullptr); result < 0)
        return juce::Result::fail ("Could not open the video decoder: " + describeError (result));

    return juce::Result::ok();
}

void VideoDecoder::Impl::describeStream (const juce::File& file)
{
    streamStartPts = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;

    info.file = file;
    info.codecName = avcodec_get_name (stream->codecpar->codec_id);
    info.width = stream->codecpar->width;
    info.height = stream->codecpar->height;
    info.framesPerSecond = streamFramesPerSecond (*stream);
    info.embeddedTimecode = findEmbeddedTimecode (*format);
    info.hasAudio = av_find_best_stream (format.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) >= 0;

    const auto pixelAspect = av_guess_sample_aspect_ratio (format.get(), stream, nullptr);
    const auto pixelAspectRatio = pixelAspect.num > 0 && pixelAspect.den > 0 ? av_q2d (pixelAspect) : 1.0;
    info.displayAspectRatio = info.height > 0 ? info.width * pixelAspectRatio / info.height : 16.0 / 9.0;

    if (stream->nb_frames > 0)
        info.frameCount = stream->nb_frames;
    else if (stream->duration != AV_NOPTS_VALUE)
        info.frameCount = (int64_t) std::llround ((double) stream->duration * av_q2d (stream->time_base) * info.framesPerSecond);
    else if (format->duration != AV_NOPTS_VALUE)
        info.frameCount = (int64_t) std::llround ((double) format->duration / AV_TIME_BASE * info.framesPerSecond);
}

std::optional<DecodedFrame> VideoDecoder::Impl::decodeFrame (int64_t index)
{
    if (index < 0 || index >= info.frameCount)
        return std::nullopt;

    const bool isNearlySequential = index >= nextIndex && index - nextIndex <= maximumFramesToDecodeInsteadOfSeeking;

    if (! isNearlySequential)
        seekBefore (index, 0);

    for (int attempt = 1; decodeNextFrame(); )
    {
        const auto decodedIndex = indexOfDecodedFrame();

        // A seek that lands after the target means the keyframe index was imprecise.
        if (decodedIndex > index && ! isNearlySequential && attempt < maximumSeekAttempts && nextIndex == -1)
        {
            seekBefore (index, attempt++);
            continue;
        }

        nextIndex = decodedIndex + 1;

        if (decodedIndex >= index)
            return DecodedFrame { index, convertDecodedFrame() };
    }

    return std::nullopt;
}

void VideoDecoder::Impl::seekBefore (int64_t index, int attempt)
{
    // Each retry backs off a further second to find an earlier keyframe.
    const auto backoff = (int64_t) std::llround (attempt * info.framesPerSecond);
    const auto targetPts = ptsForIndex (std::max<int64_t> (0, index - backoff));

    av_seek_frame (format.get(), stream->index, targetPts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers (codec.get());
    hasReachedEndOfFile = false;
    nextIndex = -1;
}

bool VideoDecoder::Impl::decodeNextFrame()
{
    for (;;)
    {
        const auto received = avcodec_receive_frame (codec.get(), frame.get());

        if (received == 0)
            return true;

        if (received == AVERROR_EOF || (received != AVERROR (EAGAIN)))
            return false;

        if (hasReachedEndOfFile)
            return false;

        if (av_read_frame (format.get(), packet.get()) < 0)
        {
            hasReachedEndOfFile = true;
            avcodec_send_packet (codec.get(), nullptr);
            continue;
        }

        if (packet->stream_index == stream->index)
            avcodec_send_packet (codec.get(), packet.get());

        av_packet_unref (packet.get());
    }
}

int64_t VideoDecoder::Impl::indexOfDecodedFrame() const
{
    const auto pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;

    if (pts == AV_NOPTS_VALUE)
        return nextIndex >= 0 ? nextIndex : 0;

    const auto seconds = (double) (pts - streamStartPts) * av_q2d (stream->time_base);
    return (int64_t) std::llround (seconds * info.framesPerSecond);
}

int64_t VideoDecoder::Impl::ptsForIndex (int64_t index) const
{
    const auto seconds = (double) index / info.framesPerSecond;
    return streamStartPts + (int64_t) std::llround (seconds / av_q2d (stream->time_base));
}

juce::Image VideoDecoder::Impl::convertDecodedFrame()
{
    const auto scale = std::min (1.0, (double) maximumWidth / frame->width);
    const auto outputWidth = std::max (2, (int) std::lround (frame->width * scale));
    const auto outputHeight = std::max (2, (int) std::lround (frame->height * scale));

    scaler.reset (sws_getCachedContext (scaler.release(), frame->width, frame->height, (AVPixelFormat) frame->format,
                                        outputWidth, outputHeight, AV_PIX_FMT_BGRA, SWS_BILINEAR,
                                        nullptr, nullptr, nullptr));

    // Software images can be written from this background thread on every platform.
    juce::Image image (juce::Image::ARGB, outputWidth, outputHeight, false, juce::SoftwareImageType());

    if (scaler == nullptr)
        return image;

    juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);
    uint8_t* destination[4] = { pixels.data, nullptr, nullptr, nullptr };
    int destinationStride[4] = { pixels.lineStride, 0, 0, 0 };

    sws_scale (scaler.get(), frame->data, frame->linesize, 0, frame->height, destination, destinationStride);
    return image;
}

std::unique_ptr<VideoDecoder> VideoDecoder::open (const juce::File& file, juce::String& error)
{
    auto impl = std::make_unique<Impl>();

    if (auto result = impl->open (file); result.failed())
    {
        error = result.getErrorMessage();
        return nullptr;
    }

    return std::unique_ptr<VideoDecoder> (new VideoDecoder (std::move (impl)));
}

VideoDecoder::VideoDecoder (std::unique_ptr<Impl> implementation) : impl (std::move (implementation)) {}
VideoDecoder::~VideoDecoder() = default;

const VideoInfo& VideoDecoder::info() const                          { return impl->info; }
std::optional<DecodedFrame> VideoDecoder::decodeFrame (int64_t index) { return impl->decodeFrame (index); }
int64_t VideoDecoder::nextSequentialIndex() const                     { return impl->nextIndex; }

} // namespace picturelock
