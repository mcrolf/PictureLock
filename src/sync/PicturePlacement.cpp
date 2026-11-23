#include "PicturePlacement.h"

#include <cmath>

namespace picturelock
{

namespace
{
    constexpr double frameBoundaryTolerance = 1.0e-6;
}

Timecode PicturePlacement::sessionTimecodeAt (double songSeconds) const
{
    const auto framesIntoSession = std::floor ((songSeconds - syncOffsetSeconds) * timecodeRate.framesPerSecond()
                                               + frameBoundaryTolerance);
    return { sessionStartFrame + (int64_t) framesIntoSession, timecodeRate };
}

double PicturePlacement::pictureSecondsAt (double songSeconds) const
{
    return songSeconds - syncOffsetSeconds - songSecondsAtPictureStart();
}

double PicturePlacement::songSecondsAtPictureStart() const
{
    return (double) (pictureStartFrame - sessionStartFrame) / timecodeRate.framesPerSecond();
}

juce::String formatSongPosition (double seconds)
{
    const auto sign = seconds < 0 ? "-" : "";
    const auto totalMilliseconds = (int64_t) std::llround (std::abs (seconds) * 1000.0);
    return sign + juce::String (totalMilliseconds / 60000) + ":"
         + juce::String ((totalMilliseconds / 1000) % 60).paddedLeft ('0', 2) + "."
         + juce::String (totalMilliseconds % 1000).paddedLeft ('0', 3);
}

int64_t pictureFrameIndexAt (double pictureSeconds, double fileFramesPerSecond, int64_t fileFrameCount)
{
    if (pictureSeconds < -frameBoundaryTolerance)
        return -1;

    const auto index = (int64_t) std::floor (pictureSeconds * fileFramesPerSecond + frameBoundaryTolerance);
    return index < fileFrameCount ? index : -1;
}

} // namespace picturelock
