#pragma once

#include "sync/PicturePlacement.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

namespace picturelock
{

/** Everything the user configures for a PictureLock instance. Saved with the Live Set.
    Broadcasts a change message on the message thread whenever a value changes.
*/
class PictureSettings : public juce::ChangeBroadcaster
{
public:
    struct VideoWindowState
    {
        bool isVisible = false;
        bool isAlwaysOnTop = true;
        juce::Rectangle<int> bounds;
    };

    juce::File movieFile() const { return movie; }
    void setMovieFile (const juce::File&);

    const PicturePlacement& placement() const { return picturePlacement; }

    /** Changes the rate while keeping the session and picture start labels, e.g. 01:00:00:00. */
    void setTimecodeRate (FrameRate);
    void setSessionStart (Timecode);
    void setPictureStart (Timecode);
    void setSyncOffsetMilliseconds (double);
    double syncOffsetMilliseconds() const { return picturePlacement.syncOffsetSeconds * 1000.0; }

    bool showsTimecodeOverlay() const { return timecodeOverlay; }
    void setShowsTimecodeOverlay (bool);

    const VideoWindowState& videoWindow() const { return window; }
    void setVideoWindow (VideoWindowState);

    juce::ValueTree toValueTree() const;
    void restoreFrom (const juce::ValueTree&);

    static constexpr double maximumSyncOffsetMilliseconds = 1000.0;

private:
    juce::File movie;
    PicturePlacement picturePlacement;
    bool timecodeOverlay = true;
    VideoWindowState window;
};

} // namespace picturelock
