#include "sync/PicturePlacement.h"

namespace picturelock
{

class PicturePlacementTests : public juce::UnitTest
{
public:
    PicturePlacementTests() : juce::UnitTest ("PicturePlacement", "PictureLock") {}

    void runTest() override
    {
        const auto fps24 = FrameRate::fromId (FrameRate::Id::fps24);
        const auto fps23_976 = FrameRate::fromId (FrameRate::Id::fps23_976);

        beginTest ("Song position 0 shows the session start timecode");
        PicturePlacement placement { fps24, Timecode::fromFields (0, 59, 50, 0, fps24).frameCount(),
                                     Timecode::fromFields (1, 0, 0, 0, fps24).frameCount(), 0.0 };
        expectEquals (placement.sessionTimecodeAt (0.0).toString(), juce::String ("00:59:50:00"));
        expectEquals (placement.sessionTimecodeAt (10.0).toString(), juce::String ("01:00:00:00"));

        beginTest ("The picture starts at its start timecode");
        expectWithinAbsoluteError (placement.songSecondsAtPictureStart(), 10.0, 1.0e-9);
        expectWithinAbsoluteError (placement.pictureSecondsAt (10.0), 0.0, 1.0e-9);
        expectEquals (pictureFrameIndexAt (placement.pictureSecondsAt (9.99), 24.0, 1000), (int64_t) -1);
        expectEquals (pictureFrameIndexAt (placement.pictureSecondsAt (10.0), 24.0, 1000), (int64_t) 0);
        expectEquals (pictureFrameIndexAt (placement.pictureSecondsAt (11.0), 24.0, 1000), (int64_t) 24);

        beginTest ("Frame boundaries do not flicker on floating point error");
        expectEquals (pictureFrameIndexAt (1.0 / 24.0 * 7.0, 24.0, 1000), (int64_t) 7);
        expectEquals (pictureFrameIndexAt (1.0 / 24.0 * 7.0 - 1.0e-4, 24.0, 1000), (int64_t) 6);

        beginTest ("Frames after the end of the movie are outside the picture");
        expectEquals (pictureFrameIndexAt (999.0 / 24.0, 24.0, 1000), (int64_t) 999);
        expectEquals (pictureFrameIndexAt (1000.0 / 24.0, 24.0, 1000), (int64_t) -1);

        beginTest ("A positive sync offset delays picture and timecode");
        placement.syncOffsetSeconds = 0.5;
        expectEquals (placement.sessionTimecodeAt (10.5).toString(), juce::String ("01:00:00:00"));
        expectWithinAbsoluteError (placement.pictureSecondsAt (10.5), 0.0, 1.0e-9);

        beginTest ("Song positions read like Live's time ruler");
        expectEquals (formatSongPosition (22.0), juce::String ("0:22.000"));
        expectEquals (formatSongPosition (83.5004), juce::String ("1:23.500"));
        expectEquals (formatSongPosition (-1.25), juce::String ("-0:01.250"));

        beginTest ("23.976 timecode runs at the real 24000/1001 rate");
        PicturePlacement pulledDown { fps23_976, Timecode::fromFields (1, 0, 0, 0, fps23_976).frameCount(),
                                      Timecode::fromFields (1, 0, 0, 0, fps23_976).frameCount(), 0.0 };
        expectEquals (pulledDown.sessionTimecodeAt (1001.0 / 24000.0 * 48).toString(), juce::String ("01:00:02:00"));
        expectEquals (pictureFrameIndexAt (pulledDown.pictureSecondsAt (1.0), 24000.0 / 1001.0, 1000), (int64_t) 23);
    }
};

static PicturePlacementTests picturePlacementTests;

} // namespace picturelock
