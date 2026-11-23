#include "FrameRate.h"

#include <cmath>

namespace picturelock
{

const std::vector<FrameRate>& FrameRate::all()
{
    static const std::vector<FrameRate> rates {
        { Id::fps23_976,         24, 24000, 1001, false, "23.976" },
        { Id::fps24,             24,    24,    1, false, "24" },
        { Id::fps25,             25,    25,    1, false, "25" },
        { Id::fps29_97,          30, 30000, 1001, false, "29.97" },
        { Id::fps29_97DropFrame, 30, 30000, 1001, true,  "29.97 DF" },
        { Id::fps30,             30,    30,    1, false, "30" },
        { Id::fps47_952,         48, 48000, 1001, false, "47.952" },
        { Id::fps48,             48,    48,    1, false, "48" },
        { Id::fps50,             50,    50,    1, false, "50" },
        { Id::fps59_94,          60, 60000, 1001, false, "59.94" },
        { Id::fps59_94DropFrame, 60, 60000, 1001, true,  "59.94 DF" },
        { Id::fps60,             60,    60,    1, false, "60" },
    };
    return rates;
}

FrameRate FrameRate::fromId (Id id)
{
    for (const auto& rate : all())
        if (rate.id == id)
            return rate;

    jassertfalse;
    return all().front();
}

std::optional<FrameRate> FrameRate::fromName (const juce::String& name)
{
    for (const auto& rate : all())
        if (name == rate.name)
            return rate;

    return std::nullopt;
}

std::optional<FrameRate> FrameRate::closestStandard (double measuredFramesPerSecond)
{
    // Must stay well under the 0.1% gap between 24 and 23.976 (and 30 and 29.97).
    constexpr double maximumRelativeError = 0.0002;

    for (const auto& rate : all())
    {
        if (rate.isDropFrame)
            continue;

        const auto relativeError = std::abs (rate.framesPerSecond() - measuredFramesPerSecond) / rate.framesPerSecond();

        if (relativeError < maximumRelativeError)
            return rate;
    }

    return std::nullopt;
}

} // namespace picturelock
