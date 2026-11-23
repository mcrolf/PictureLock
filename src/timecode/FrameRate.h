#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace picturelock
{

/** A timecode frame rate: the exact playback rate plus how frames are labelled. */
struct FrameRate
{
    enum class Id
    {
        fps23_976, fps24, fps25, fps29_97, fps29_97DropFrame, fps30,
        fps47_952, fps48, fps50, fps59_94, fps59_94DropFrame, fps60
    };

    Id id;
    int nominalFramesPerSecond; // frames counted per timecode second (24, 25, 30, ...)
    int rateNumerator;          // exact rate as a ratio, e.g. 24000 / 1001
    int rateDenominator;
    bool isDropFrame;
    const char* name;

    double framesPerSecond() const { return (double) rateNumerator / rateDenominator; }

    static const std::vector<FrameRate>& all();
    static FrameRate fromId (Id);
    static std::optional<FrameRate> fromName (const juce::String&);

    /** The non-drop-frame standard rate closest to a measured file rate, if one is within 0.02%. */
    static std::optional<FrameRate> closestStandard (double measuredFramesPerSecond);

    bool operator== (const FrameRate& other) const { return id == other.id; }
};

} // namespace picturelock
