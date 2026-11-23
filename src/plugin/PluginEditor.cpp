#include "PluginEditor.h"

namespace picturelock
{

namespace
{
    const juce::String movieFilePatterns = "*.mov;*.mp4;*.m4v;*.mxf;*.mkv;*.avi;*.mts;*.ts;*.mpg";
    constexpr int settingsPanelWidth = 280;
    constexpr int headerHeight = 44;
    constexpr int readoutHeight = 56;
    constexpr int statusHeight = 26;

    juce::String formatSongPosition (double seconds)
    {
        const auto sign = seconds < 0 ? "-" : "";
        const auto totalMilliseconds = (int64_t) std::llround (std::abs (seconds) * 1000.0);
        return sign + juce::String (totalMilliseconds / 60000) + ":"
             + juce::String ((totalMilliseconds / 1000) % 60).paddedLeft ('0', 2) + "."
             + juce::String (totalMilliseconds % 1000).paddedLeft ('0', 3);
    }
}

PictureLockEditor::PictureLockEditor (PictureLockProcessor& p)
    : AudioProcessorEditor (p), pictureLock (p), preview (p), settingsPanel (p)
{
    setLookAndFeel (&lookAndFeel);

    openMovieButton.onClick = [this] { chooseMovie(); };
    videoWindowButton.setClickingTogglesState (false);
    videoWindowButton.setTooltip ("Show the picture in its own window, for a second display or full screen");
    videoWindowButton.onClick = [this] { pictureLock.setVideoWindowVisible (! pictureLock.settings().videoWindow().isVisible); };
    exportGuideAudioButton.setTooltip ("Write the movie's audio to a WAV aligned to the first picture frame");
    exportGuideAudioButton.onClick = [this] { chooseGuideAudioDestination(); };

    movieDescription.setColour (juce::Label::textColourId, palette::mutedText);
    movieDescription.setFont (juce::FontOptions (13.0f));

    timecodeReadout.setFont (PictureLockLookAndFeel::timecodeFont (40.0f));
    timecodeReadout.setColour (juce::Label::textColourId, palette::timecode);
    timecodeReadout.setJustificationType (juce::Justification::centredLeft);
    timecodeReadout.setTitle ("Session timecode");

    transportStatus.setFont (juce::FontOptions (13.0f));
    transportStatus.setJustificationType (juce::Justification::centredRight);
    statusMessage.setFont (juce::FontOptions (13.0f));

    preview.onMomentChanged = [this] (const PictureMoment& moment) { showMoment (moment); };
    preview.onDoubleClick = [this] { pictureLock.setVideoWindowVisible (true); };

    for (auto* child : std::initializer_list<juce::Component*> { &openMovieButton, &movieDescription, &videoWindowButton,
                                                                 &exportGuideAudioButton, &timecodeReadout, &transportStatus,
                                                                 &statusMessage, &preview, &settingsPanel })
        addAndMakeVisible (child);

    pictureLock.settings().addChangeListener (this);
    showMovieDescription();
    changeListenerCallback (nullptr);

    if (const auto missing = pictureLock.settings().movieFile(); missing != juce::File() && ! missing.existsAsFile())
        showStatus ("Movie not found: " + missing.getFullPathName(), palette::warning);

    setResizable (true, true);
    setResizeLimits (640, 420, 2400, 1600);
    setSize (900, 560);
}

PictureLockEditor::~PictureLockEditor()
{
    pictureLock.settings().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void PictureLockEditor::chooseMovie()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Open a movie", pictureLock.settings().movieFile(), movieFilePatterns);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  if (auto file = chooser.getResult(); file.existsAsFile())
                                      openMovie (file);
                              });
}

void PictureLockEditor::openMovie (const juce::File& file)
{
    if (auto result = pictureLock.loadMovie (file); result.failed())
        showStatus (result.getErrorMessage(), palette::warning);
    else
        showStatus ("Opened " + file.getFileName(), palette::mutedText);

    showMovieDescription();
}

void PictureLockEditor::chooseGuideAudioDestination()
{
    const auto movie = pictureLock.settings().movieFile();
    const auto suggested = movie.getSiblingFile (movie.getFileNameWithoutExtension() + " guide.wav");

    fileChooser = std::make_unique<juce::FileChooser> ("Save guide audio", suggested, "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto wav = chooser.getResult().withFileExtension ("wav");

        if (wav == juce::File())
            return;

        const auto songStart = pictureLock.settings().placement().songSecondsAtPictureStart();
        pictureLock.exportGuideAudio (wav, [safeThis = juce::Component::SafePointer<PictureLockEditor> (this), wav, songStart] (juce::Result result)
        {
            if (safeThis == nullptr)
                return;

            if (result.failed())
                safeThis->showStatus (result.getErrorMessage(), palette::warning);
            else
                safeThis->showStatus ("Wrote " + wav.getFileName() + ". Place it at song position "
                                      + formatSongPosition (songStart) + " (the picture start).", palette::playing);
        });
        startTimerHz (10);
    });
}

void PictureLockEditor::timerCallback()
{
    if (pictureLock.isExportingGuideAudio())
        showStatus ("Exporting guide audio... " + juce::String (juce::roundToInt (pictureLock.guideAudioProgress() * 100.0)) + "%",
                    palette::mutedText);
    else
        stopTimer();
}

void PictureLockEditor::showMovieDescription()
{
    const auto info = pictureLock.movieInfo();
    exportGuideAudioButton.setEnabled (info && info->hasAudio);

    if (! info)
    {
        movieDescription.setText ("No movie loaded", juce::dontSendNotification);
        return;
    }

    juce::StringArray parts { info->file.getFileName(),
                              juce::String (info->width) + " x " + juce::String (info->height),
                              info->codecName,
                              juce::String (info->framesPerSecond, 3) + " fps" };

    if (info->embeddedTimecode)
        parts.add ("TC " + *info->embeddedTimecode);

    movieDescription.setText (parts.joinIntoString ("  |  "), juce::dontSendNotification);
}

void PictureLockEditor::showMoment (const PictureMoment& moment)
{
    timecodeReadout.setText (moment.sessionTimecode.toString(), juce::dontSendNotification);

    if (! moment.hasHostTransport)
    {
        transportStatus.setText ("Waiting for host transport", juce::dontSendNotification);
        transportStatus.setColour (juce::Label::textColourId, palette::mutedText);
        return;
    }

    const auto info = pictureLock.movieInfo();
    juce::String frameText;

    if (info)
        frameText = moment.frameIndex >= 0 ? "Frame " + juce::String (moment.frameIndex + 1) + " / " + juce::String (info->frameCount)
                                           : "Outside picture";

    // Song time is shown so it can be checked against Live's time ruler after tempo changes.
    transportStatus.setText ((moment.transport.isPlaying ? "PLAYING" : "STOPPED")
                                 + juce::String ("   Song ") + formatSongPosition (moment.transport.songSeconds)
                                 + "   " + frameText,
                             juce::dontSendNotification);
    transportStatus.setColour (juce::Label::textColourId, moment.transport.isPlaying ? palette::playing : palette::mutedText);
}

void PictureLockEditor::showStatus (const juce::String& message, juce::Colour colour)
{
    statusMessage.setText (message, juce::dontSendNotification);
    statusMessage.setColour (juce::Label::textColourId, colour);
}

void PictureLockEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto isWindowOpen = pictureLock.settings().videoWindow().isVisible;
    videoWindowButton.setToggleState (isWindowOpen, juce::dontSendNotification);
    videoWindowButton.setButtonText (isWindowOpen ? "Hide Video Window" : "Show Video Window");
    showMovieDescription();
    preview.repaint();
}

bool PictureLockEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension (movieFilePatterns.removeCharacters ("*"));
}

void PictureLockEditor::filesDropped (const juce::StringArray& files, int, int)
{
    isDragHovering = false;
    repaint();
    openMovie (juce::File (files[0]));
}

void PictureLockEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::background);
}

void PictureLockEditor::paintOverChildren (juce::Graphics& g)
{
    if (! isDragHovering)
        return;

    g.setColour (palette::timecode.withAlpha (0.15f));
    g.fillAll();
    g.setColour (palette::timecode);
    g.drawRect (getLocalBounds(), 3);
}

void PictureLockEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (headerHeight).reduced (0, 6);
    openMovieButton.setBounds (header.removeFromLeft (120));
    exportGuideAudioButton.setBounds (header.removeFromRight (170));
    header.removeFromRight (8);
    videoWindowButton.setBounds (header.removeFromRight (160));
    header.removeFromLeft (10);
    movieDescription.setBounds (header);

    auto readout = area.removeFromTop (readoutHeight);
    transportStatus.setBounds (readout.removeFromRight (420));
    timecodeReadout.setBounds (readout);

    statusMessage.setBounds (area.removeFromBottom (statusHeight));
    area.removeFromBottom (6);

    settingsPanel.setBounds (area.removeFromRight (settingsPanelWidth));
    area.removeFromRight (12);
    preview.setBounds (area);
}

} // namespace picturelock
