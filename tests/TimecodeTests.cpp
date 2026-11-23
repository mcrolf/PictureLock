#include "timecode/Timecode.h"

namespace picturelock
{

class TimecodeTests : public juce::UnitTest
{
public:
    TimecodeTests() : juce::UnitTest ("Timecode", "PictureLock") {}

    void runTest() override
    {
        const auto fps24 = FrameRate::fromId (FrameRate::Id::fps24);
        const auto fps23_976 = FrameRate::fromId (FrameRate::Id::fps23_976);
        const auto fps25 = FrameRate::fromId (FrameRate::Id::fps25);
        const auto df29_97 = FrameRate::fromId (FrameRate::Id::fps29_97DropFrame);
        const auto df59_94 = FrameRate::fromId (FrameRate::Id::fps59_94DropFrame);

        beginTest ("Non-drop-frame labels count every frame");
        expectEquals (Timecode::fromFields (1, 0, 0, 0, fps24).frameCount(), (int64_t) 86400);
        expectEquals (Timecode (86400 + 23, fps24).toString(), juce::String ("01:00:00:23"));
        expectEquals (Timecode (90000, fps25).toString(), juce::String ("01:00:00:00"));

        beginTest ("23.976 counts 24 labels per second but runs slower than real time");
        expectWithinAbsoluteError (Timecode::fromFields (1, 0, 0, 0, fps23_976).seconds(), 3603.6, 1.0e-9);

        beginTest ("29.97 drop-frame skips labels 00 and 01 except every tenth minute");
        expectEquals (Timecode (1799, df29_97).toString(), juce::String ("00:00:59;29"));
        expectEquals (Timecode (1800, df29_97).toString(), juce::String ("00:01:00;02"));
        expectEquals (Timecode (17982, df29_97).toString(), juce::String ("00:10:00;00"));
        expectEquals (Timecode::fromFields (1, 0, 0, 0, df29_97).frameCount(), (int64_t) 107892);
        expectEquals (Timecode::fromFields (0, 1, 0, 2, df29_97).frameCount(), (int64_t) 1800);

        beginTest ("59.94 drop-frame skips four labels per minute");
        expectEquals (Timecode (3600, df59_94).toString(), juce::String ("00:01:00;04"));

        beginTest ("Labels round-trip through frame counts");
        for (int64_t frames : { 0, 1, 1799, 1800, 17981, 17982, 107891, 107892, 2589407 })
        {
            const auto label = Timecode (frames, df29_97).toString();
            expectEquals (Timecode::parse (label, df29_97)->frameCount(), frames, label);
        }

        beginTest ("Parsing accepts common separators and rejects invalid labels");
        expectEquals (Timecode::parse ("01:00:00:00", fps24)->frameCount(), (int64_t) 86400);
        expectEquals (Timecode::parse ("01.00.00.00", fps24)->frameCount(), (int64_t) 86400);
        expectEquals (Timecode::parse ("1000000", fps24)->frameCount(), (int64_t) 86400);
        expectEquals (Timecode::parse ("-00:00:01:00", fps24)->frameCount(), (int64_t) -24);
        expect (! Timecode::parse ("00:00:00:24", fps24).has_value(), "frames field beyond the rate");
        expect (! Timecode::parse ("00:61:00:00", fps24).has_value(), "minutes beyond 59");
        expect (! Timecode::parse ("00:01:00;00", df29_97).has_value(), "dropped label");
        expect (! Timecode::parse ("01:00", fps24).has_value(), "too few fields");
        expect (! Timecode::parse ("", fps24).has_value(), "empty");

        beginTest ("Negative timecode is shown before midnight with a sign");
        expectEquals (Timecode (-24, fps24).toString(), juce::String ("-00:00:01:00"));

        beginTest ("Measured file rates map to standard rates");
        expect (FrameRate::closestStandard (23.976023)->id == FrameRate::Id::fps23_976);
        expect (FrameRate::closestStandard (29.97003)->id == FrameRate::Id::fps29_97);
        expect (FrameRate::closestStandard (25.0)->id == FrameRate::Id::fps25);
        expect (FrameRate::closestStandard (24.0)->id == FrameRate::Id::fps24, "24 is not 23.976");
        expect (FrameRate::closestStandard (30.0)->id == FrameRate::Id::fps30, "30 is not 29.97");
        expect (FrameRate::closestStandard (23.976)->id == FrameRate::Id::fps23_976, "rounded 23.976 in file headers");
        expect (! FrameRate::closestStandard (17.0).has_value());
    }
};

static TimecodeTests timecodeTests;

} // namespace picturelock
