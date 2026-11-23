#include "PictureSettings.h"

namespace picturelock
{

namespace ids
{
    static const juce::Identifier root ("PictureLockSettings");
    static const juce::Identifier movieFile ("movieFile");
    static const juce::Identifier timecodeRate ("timecodeRate");
    static const juce::Identifier sessionStart ("sessionStart");
    static const juce::Identifier pictureStart ("pictureStart");
    static const juce::Identifier syncOffsetMilliseconds ("syncOffsetMilliseconds");
    static const juce::Identifier timecodeOverlay ("timecodeOverlay");
    static const juce::Identifier windowVisible ("videoWindowVisible");
    static const juce::Identifier windowAlwaysOnTop ("videoWindowAlwaysOnTop");
    static const juce::Identifier windowBounds ("videoWindowBounds");
}

void PictureSettings::setMovieFile (const juce::File& file)
{
    movie = file;
    sendChangeMessage();
}

void PictureSettings::setTimecodeRate (FrameRate rate)
{
    auto relabel = [rate] (int64_t frames, FrameRate oldRate)
    {
        const auto label = Timecode (frames, oldRate).toString();
        return Timecode::parse (label, rate).value_or (Timecode::fromSeconds (Timecode (frames, oldRate).seconds(), rate)).frameCount();
    };

    const auto oldRate = picturePlacement.timecodeRate;
    picturePlacement.sessionStartFrame = relabel (picturePlacement.sessionStartFrame, oldRate);
    picturePlacement.pictureStartFrame = relabel (picturePlacement.pictureStartFrame, oldRate);
    picturePlacement.timecodeRate = rate;
    sendChangeMessage();
}

void PictureSettings::setSessionStart (Timecode timecode)
{
    picturePlacement.sessionStartFrame = timecode.frameCount();
    sendChangeMessage();
}

void PictureSettings::setPictureStart (Timecode timecode)
{
    picturePlacement.pictureStartFrame = timecode.frameCount();
    sendChangeMessage();
}

void PictureSettings::setSyncOffsetMilliseconds (double milliseconds)
{
    picturePlacement.syncOffsetSeconds = juce::jlimit (-maximumSyncOffsetMilliseconds, maximumSyncOffsetMilliseconds, milliseconds) / 1000.0;
    sendChangeMessage();
}

void PictureSettings::setShowsTimecodeOverlay (bool shouldShow)
{
    timecodeOverlay = shouldShow;
    sendChangeMessage();
}

void PictureSettings::setVideoWindow (VideoWindowState state)
{
    window = state;
    sendChangeMessage();
}

juce::ValueTree PictureSettings::toValueTree() const
{
    const auto& rate = picturePlacement.timecodeRate;
    juce::ValueTree tree (ids::root);
    tree.setProperty (ids::movieFile, movie.getFullPathName(), nullptr);
    tree.setProperty (ids::timecodeRate, rate.name, nullptr);
    tree.setProperty (ids::sessionStart, Timecode (picturePlacement.sessionStartFrame, rate).toString(), nullptr);
    tree.setProperty (ids::pictureStart, Timecode (picturePlacement.pictureStartFrame, rate).toString(), nullptr);
    tree.setProperty (ids::syncOffsetMilliseconds, syncOffsetMilliseconds(), nullptr);
    tree.setProperty (ids::timecodeOverlay, timecodeOverlay, nullptr);
    tree.setProperty (ids::windowVisible, window.isVisible, nullptr);
    tree.setProperty (ids::windowAlwaysOnTop, window.isAlwaysOnTop, nullptr);
    tree.setProperty (ids::windowBounds, window.bounds.toString(), nullptr);
    return tree;
}

void PictureSettings::restoreFrom (const juce::ValueTree& tree)
{
    if (! tree.hasType (ids::root))
        return;

    const auto rate = FrameRate::fromName (tree[ids::timecodeRate].toString()).value_or (picturePlacement.timecodeRate);
    auto parseFrames = [rate] (const juce::var& text)
    {
        return Timecode::parse (text.toString(), rate).value_or (Timecode (0, rate)).frameCount();
    };

    const auto path = tree[ids::movieFile].toString();
    movie = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    picturePlacement.timecodeRate = rate;
    picturePlacement.sessionStartFrame = parseFrames (tree[ids::sessionStart]);
    picturePlacement.pictureStartFrame = parseFrames (tree[ids::pictureStart]);
    picturePlacement.syncOffsetSeconds = juce::jlimit (-maximumSyncOffsetMilliseconds, maximumSyncOffsetMilliseconds,
                                                       (double) tree.getProperty (ids::syncOffsetMilliseconds, 0.0)) / 1000.0;
    timecodeOverlay = tree.getProperty (ids::timecodeOverlay, true);
    window.isVisible = tree.getProperty (ids::windowVisible, false);
    window.isAlwaysOnTop = tree.getProperty (ids::windowAlwaysOnTop, true);
    window.bounds = juce::Rectangle<int>::fromString (tree[ids::windowBounds].toString());
    sendChangeMessage();
}

} // namespace picturelock
