#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "ui/VideoWindow.h"

namespace picturelock
{

PictureLockProcessor::PictureLockProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    pictureSettings.addChangeListener (this);
}

PictureLockProcessor::~PictureLockProcessor()
{
    pictureSettings.removeChangeListener (this);
    shouldCancelGuideAudioExport = true;
    guideAudioPool.removeAllJobs (true, 10000);
    videoWindow.reset();
    pictureEngine.unload();
}

juce::Result PictureLockProcessor::loadMovie (const juce::File& file)
{
    auto result = pictureEngine.load (file);

    if (result.failed())
        return result;

    if (file != pictureSettings.movieFile())
        if (auto info = pictureEngine.info())
            adoptMovieTimecode (*info);

    pictureSettings.setMovieFile (file);
    return result;
}

void PictureLockProcessor::adoptMovieTimecode (const VideoInfo& info)
{
    auto rate = FrameRate::closestStandard (info.framesPerSecond);

    if (! rate)
        return;

    const auto isDropFrameLabel = info.embeddedTimecode && info.embeddedTimecode->containsChar (';');

    if (isDropFrameLabel && rate->id == FrameRate::Id::fps29_97)
        rate = FrameRate::fromId (FrameRate::Id::fps29_97DropFrame);
    else if (isDropFrameLabel && rate->id == FrameRate::Id::fps59_94)
        rate = FrameRate::fromId (FrameRate::Id::fps59_94DropFrame);

    pictureSettings.setTimecodeRate (*rate);

    if (! info.embeddedTimecode)
        return;

    if (auto start = Timecode::parse (*info.embeddedTimecode, *rate))
    {
        // A session still at the default start would put a 01:00:00:00 movie an hour
        // into the Live Set, so start the session where the picture starts instead.
        if (pictureSettings.placement().sessionStartFrame == 0)
            pictureSettings.setSessionStart (*start);

        pictureSettings.setPictureStart (*start);
    }
}

PictureMoment PictureLockProcessor::advancePicture()
{
    PictureMoment moment;
    moment.hasHostTransport = transportClock.hasReceivedPosition();
    moment.transport = transportClock.positionAt (juce::Time::getMillisecondCounterHiRes());

    const auto& placement = pictureSettings.placement();
    moment.sessionTimecode = placement.sessionTimecodeAt (moment.transport.songSeconds);

    if (auto info = pictureEngine.info())
    {
        moment.frameIndex = pictureFrameIndexAt (placement.pictureSecondsAt (moment.transport.songSeconds),
                                                 info->framesPerSecond, info->frameCount);

        if (moment.frameIndex >= 0)
        {
            pictureEngine.request (moment.frameIndex, moment.transport.isPlaying);
            moment.frame = pictureEngine.frameNearest (moment.frameIndex);
        }
    }

    return moment;
}

void PictureLockProcessor::placePictureStartAtPlayhead()
{
    const auto position = transportClock.positionAt (juce::Time::getMillisecondCounterHiRes());
    pictureSettings.setPictureStart (pictureSettings.placement().sessionTimecodeAt (position.songSeconds));
}

void PictureLockProcessor::exportGuideAudio (const juce::File& wav, std::function<void (juce::Result)> onFinished)
{
    if (isExportingGuideAudio())
        return;

    guideAudioExportProgress = 0.0;
    const auto movie = pictureSettings.movieFile();
    const auto sampleRate = currentSampleRate.load();

    guideAudioPool.addJob ([this, movie, wav, sampleRate, whenFinished = std::move (onFinished)]() mutable
    {
        auto result = GuideAudioExporter::exportToWav (movie, wav, sampleRate, [this] (double progress)
        {
            guideAudioExportProgress = progress;
            return ! shouldCancelGuideAudioExport.load();
        });

        guideAudioExportProgress = -1.0;

        juce::MessageManager::callAsync ([weakThis = juce::WeakReference<PictureLockProcessor> (this), outcome = result, whenFinished]
        {
            if (weakThis != nullptr && whenFinished)
                whenFinished (outcome);
        });
    });
}

void PictureLockProcessor::setVideoWindowVisible (bool shouldBeVisible)
{
    auto state = pictureSettings.videoWindow();
    state.isVisible = shouldBeVisible;
    pictureSettings.setVideoWindow (state);
}

bool PictureLockProcessor::isVideoWindowVisible() const
{
    return videoWindow != nullptr && videoWindow->isVisible();
}

void PictureLockProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto& state = pictureSettings.videoWindow();

    if (state.isVisible && videoWindow == nullptr)
        videoWindow = std::make_unique<VideoWindow> (*this);
    else if (! state.isVisible && videoWindow != nullptr)
        videoWindow.reset();

    if (videoWindow != nullptr)
        videoWindow->setAlwaysOnTop (state.isAlwaysOnTop);
}

void PictureLockProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
}

bool PictureLockProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == output;
}

void PictureLockProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Audio passes through untouched; the block only tells us where the song is.
    const auto* playHead = getPlayHead();

    if (playHead == nullptr)
        return;

    const auto position = playHead->getPosition();

    if (! position)
        return;

    double songSeconds = 0.0;

    if (auto samples = position->getTimeInSamples())
        songSeconds = (double) *samples / currentSampleRate.load();
    else if (auto seconds = position->getTimeInSeconds())
        songSeconds = *seconds;
    else
        return;

    transportClock.publish (songSeconds, position->getIsPlaying(), juce::Time::getMillisecondCounterHiRes());
}

juce::AudioProcessorEditor* PictureLockProcessor::createEditor()
{
    return new PictureLockEditor (*this);
}

void PictureLockProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = pictureSettings.toValueTree().createXml())
        copyXmlToBinary (*xml, destination);
}

void PictureLockProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    // Hosts may restore state off the message thread; movie loading and windows need it.
    juce::MessageManager::callAsync ([weakThis = juce::WeakReference<PictureLockProcessor> (this), tree]
    {
        if (auto* processor = weakThis.get())
        {
            processor->pictureSettings.restoreFrom (tree);
            processor->reloadSavedMovie();
        }
    });
}

void PictureLockProcessor::reloadSavedMovie()
{
    const auto movie = pictureSettings.movieFile();

    if (movie.existsAsFile())
        pictureEngine.load (movie);
}

} // namespace picturelock

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new picturelock::PictureLockProcessor();
}
