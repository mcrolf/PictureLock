#pragma once

#include "FrameRate.h"

#include <cstdint>
#include <optional>

namespace picturelock
{

/** A SMPTE timecode label, stored as a signed count of frames from 00:00:00:00.
    Negative values represent positions before midnight (shown with a leading '-').
*/
class Timecode
{
public:
    Timecode (int64_t frameCount, FrameRate rate) : frames (frameCount), frameRate (rate) {}

    static Timecode fromFields (int hours, int minutes, int seconds, int frames, FrameRate);
    static Timecode fromSeconds (double seconds, FrameRate);

    /** Accepts "HH:MM:SS:FF", "HH:MM:SS;FF", "HH.MM.SS.FF", or "HHMMSSFF". */
    static std::optional<Timecode> parse (const juce::String& text, FrameRate);

    int64_t frameCount() const { return frames; }
    FrameRate rate() const     { return frameRate; }
    double seconds() const;

    /** Formats as HH:MM:SS:FF, using ';' before the frames field for drop-frame rates. */
    juce::String toString() const;

private:
    struct Fields { int hours, minutes, seconds, frames; };
    Fields fields() const;

    int64_t frames;
    FrameRate frameRate;
};

} // namespace picturelock
