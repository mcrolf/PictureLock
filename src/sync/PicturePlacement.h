#pragma once

#include "timecode/Timecode.h"

namespace picturelock
{

/** Where the picture sits on the session timeline.

    Live has no timecode, so the session is given a start timecode: the timecode at
    Live's song position 0 (bar 1). The picture's first frame sits at the picture
    start timecode. A positive sync offset delays the picture relative to the audio.
*/
struct PicturePlacement
{
    FrameRate timecodeRate = FrameRate::fromId (FrameRate::Id::fps24);
    int64_t sessionStartFrame = 0;
    int64_t pictureStartFrame = 0;
    double syncOffsetSeconds = 0.0;

    /** Session timecode for a song position, after the sync offset. */
    Timecode sessionTimecodeAt (double songSeconds) const;

    /** Seconds into the movie file for a song position; negative before the picture starts. */
    double pictureSecondsAt (double songSeconds) const;

    /** Song position at which the picture's first frame is shown. */
    double songSecondsAtPictureStart() const;
};

/** A song position as Live's time ruler shows it, e.g. "1:22.500". */
juce::String formatSongPosition (double seconds);

/** Index of the movie frame on screen at a picture time, or -1 outside the movie. */
int64_t pictureFrameIndexAt (double pictureSeconds, double fileFramesPerSecond, int64_t fileFrameCount);

} // namespace picturelock
