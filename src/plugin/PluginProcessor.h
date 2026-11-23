#pragma once

#include "PictureMoment.h"
#include "PictureSettings.h"
#include "audio/GuideAudioExporter.h"
#include "sync/TransportClock.h"
#include "video/PictureEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace picturelock
{

class VideoWindow;

/** A pass-through audio effect that follows the host transport and shows the picture
    that belongs at the current song position.
*/
class PictureLockProcessor : public juce::AudioProcessor,
                             private juce::ChangeListener
{
public:
    PictureLockProcessor();
    ~PictureLockProcessor() override;

    // Picture
    juce::Result loadMovie (const juce::File&);
    std::optional<VideoInfo> movieInfo() const { return pictureEngine.info(); }
    PictureSettings& settings() { return pictureSettings; }

    /** Message thread. Requests the frame for the current song position and returns it. */
    PictureMoment advancePicture();

    /** Moves the picture so its first frame sits at the current song position. */
    void placePictureStartAtPlayhead();

    /** Writes the movie's audio to a WAV on a background thread. `onFinished` runs on the message thread. */
    void exportGuideAudio (const juce::File& wav, std::function<void (juce::Result)> onFinished);
    bool isExportingGuideAudio() const { return guideAudioExportProgress.load() >= 0.0; }
    double guideAudioProgress() const  { return guideAudioExportProgress.load(); }

    void setVideoWindowVisible (bool);
    bool isVideoWindowVisible() const;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void adoptMovieTimecode (const VideoInfo&);
    void reloadSavedMovie();

    TransportClock transportClock;
    PictureEngine pictureEngine;
    PictureSettings pictureSettings;
    std::unique_ptr<VideoWindow> videoWindow;
    std::atomic<double> currentSampleRate { 48000.0 };
    juce::ThreadPool guideAudioPool { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    std::atomic<double> guideAudioExportProgress { -1.0 }; // -1 when no export is running
    std::atomic<bool> shouldCancelGuideAudioExport { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (PictureLockProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PictureLockProcessor)
};

} // namespace picturelock
