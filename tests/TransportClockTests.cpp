#include "sync/TransportClock.h"

#include <juce_core/juce_core.h>

namespace picturelock
{

class TransportClockTests : public juce::UnitTest
{
public:
    TransportClockTests() : juce::UnitTest ("TransportClock", "PictureLock") {}

    void runTest() override
    {
        beginTest ("No position until the host publishes one");
        TransportClock clock;
        expect (! clock.hasReceivedPosition());

        beginTest ("Playing positions advance with the wall clock between blocks");
        clock.publish (10.0, true, 1000.0);
        expect (clock.hasReceivedPosition());
        expectWithinAbsoluteError (clock.positionAt (1020.0).songSeconds, 10.02, 1.0e-9);
        expect (clock.positionAt (1020.0).isPlaying);

        beginTest ("Extrapolation stops if the host stops delivering blocks");
        expectWithinAbsoluteError (clock.positionAt (5000.0).songSeconds, 10.25, 1.0e-9);

        beginTest ("Extrapolation never runs backwards before the block");
        expectWithinAbsoluteError (clock.positionAt (900.0).songSeconds, 10.0, 1.0e-9);

        beginTest ("Stopped positions hold still");
        clock.publish (3.0, false, 2000.0);
        expectWithinAbsoluteError (clock.positionAt (2500.0).songSeconds, 3.0, 1.0e-9);
        expect (! clock.positionAt (2500.0).isPlaying);
    }
};

static TransportClockTests transportClockTests;

} // namespace picturelock
