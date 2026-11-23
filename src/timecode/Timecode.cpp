#include "Timecode.h"

#include <cmath>

namespace picturelock
{

namespace
{
    // Drop-frame skips frame labels 0 and 1 (or 0-3 at 59.94) at the start of every
    // minute except each tenth minute.
    int droppedLabelsPerMinute (const FrameRate& rate)
    {
        return rate.isDropFrame ? rate.nominalFramesPerSecond / 15 : 0;
    }

    int64_t labelCountForFrameCount (int64_t frameCount, const FrameRate& rate)
    {
        const auto dropped = droppedLabelsPerMinute (rate);

        if (dropped == 0)
            return frameCount;

        const int64_t framesPerMinute = rate.nominalFramesPerSecond * 60 - dropped;
        const int64_t framesPerTenMinutes = framesPerMinute * 10 + dropped;

        const auto tenMinuteBlocks = frameCount / framesPerTenMinutes;
        const auto remainder = frameCount % framesPerTenMinutes;
        const auto droppedMinutesInBlock = remainder < dropped ? 0 : (remainder - dropped) / framesPerMinute;

        return frameCount + dropped * (9 * tenMinuteBlocks + droppedMinutesInBlock);
    }
}

Timecode Timecode::fromFields (int hours, int minutes, int seconds, int frames, FrameRate rate)
{
    const int64_t nominal = rate.nominalFramesPerSecond;
    const int64_t totalMinutes = hours * 60 + minutes;
    const int64_t labels = ((int64_t) hours * 3600 + minutes * 60 + seconds) * nominal + frames;
    const auto dropped = droppedLabelsPerMinute (rate) * (totalMinutes - totalMinutes / 10);

    return { labels - dropped, rate };
}

Timecode Timecode::fromSeconds (double seconds, FrameRate rate)
{
    constexpr double frameBoundaryTolerance = 1.0e-6;
    return { (int64_t) std::floor (seconds * rate.framesPerSecond() + frameBoundaryTolerance), rate };
}

std::optional<Timecode> Timecode::parse (const juce::String& text, FrameRate rate)
{
    auto digitsAndSeparators = text.trim();
    const bool isNegative = digitsAndSeparators.startsWithChar ('-');

    if (isNegative)
        digitsAndSeparators = digitsAndSeparators.substring (1);

    if (digitsAndSeparators.isEmpty())
        return std::nullopt;

    juce::StringArray parts;

    if (digitsAndSeparators.containsOnly ("0123456789"))
    {
        auto padded = digitsAndSeparators.paddedLeft ('0', 8);

        if (padded.length() != 8)
            return std::nullopt;

        for (int i = 0; i < 8; i += 2)
            parts.add (padded.substring (i, i + 2));
    }
    else
    {
        parts.addTokens (digitsAndSeparators, ":;.", "");
    }

    if (parts.size() != 4)
        return std::nullopt;

    for (const auto& part : parts)
        if (part.isEmpty() || ! part.containsOnly ("0123456789"))
            return std::nullopt;

    const int hours = parts[0].getIntValue(), minutes = parts[1].getIntValue();
    const int seconds = parts[2].getIntValue(), frames = parts[3].getIntValue();

    if (minutes > 59 || seconds > 59 || frames >= rate.nominalFramesPerSecond)
        return std::nullopt;

    if (droppedLabelsPerMinute (rate) > 0 && seconds == 0 && minutes % 10 != 0 && frames < droppedLabelsPerMinute (rate))
        return std::nullopt;

    auto timecode = fromFields (hours, minutes, seconds, frames, rate);
    return isNegative ? Timecode (-timecode.frameCount(), rate) : timecode;
}

double Timecode::seconds() const
{
    return (double) frames / frameRate.framesPerSecond();
}

Timecode::Fields Timecode::fields() const
{
    const int64_t nominal = frameRate.nominalFramesPerSecond;
    const auto labels = labelCountForFrameCount (std::abs (frames), frameRate);

    return { (int) (labels / (nominal * 3600)),
             (int) ((labels / (nominal * 60)) % 60),
             (int) ((labels / nominal) % 60),
             (int) (labels % nominal) };
}

juce::String Timecode::toString() const
{
    const auto [hours, minutes, seconds, frameField] = fields();
    const auto frameSeparator = frameRate.isDropFrame ? ";" : ":";
    auto twoDigits = [] (int value) { return juce::String (value).paddedLeft ('0', 2); };

    return juce::String (frames < 0 ? "-" : "")
         + twoDigits (hours) + ":" + twoDigits (minutes) + ":" + twoDigits (seconds)
         + frameSeparator + twoDigits (frameField);
}

} // namespace picturelock
