#pragma once

#include <juce_core/juce_core.h>
#include <functional>

namespace picturelock
{

/** Writes a movie's production audio to a stereo WAV aligned to the first picture frame,
    so it can be dropped into Live at the picture start position as a guide track.
*/
struct GuideAudioExporter
{
    /** Returns false from the callback to cancel. Progress is 0 to 1. */
    using ProgressCallback = std::function<bool (double progress)>;

    /** Blocking: call it from a background thread. */
    static juce::Result exportToWav (const juce::File& movie, const juce::File& wav,
                                     double sampleRate, const ProgressCallback&);
};

} // namespace picturelock
