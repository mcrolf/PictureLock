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

int64_t pictureFrameIndexAt (double pictureSeconds, double fileFramesPerSecond, int64_t fileFrameCount)
{
    if (pictureSeconds < -frameBoundaryTolerance)
        return -1;

    const auto index = (int64_t) std::floor (pictureSeconds * fileFramesPerSecond + frameBoundaryTolerance);
    return index < fileFrameCount ? index : -1;
}

} // namespace picturelock
