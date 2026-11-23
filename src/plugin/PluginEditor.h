#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/PictureView.h"
#include "ui/SettingsPanel.h"

namespace picturelock
{

class PictureLockEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit PictureLockEditor (PictureLockProcessor&);
    ~PictureLockEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { isDragHovering = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { isDragHovering = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void chooseMovie();
    void openMovie (const juce::File&);
    void chooseGuideAudioDestination();
    void showMovieDescription();
    void showMoment (const PictureMoment&);
    void showStatus (const juce::String&, juce::Colour);

    PictureLockProcessor& pictureLock;
    PictureLockLookAndFeel lookAndFeel;

    juce::TextButton openMovieButton { "Open Movie..." };
    juce::Label movieDescription;
    juce::TextButton videoWindowButton { "Video Window" };
    juce::TextButton exportGuideAudioButton { "Export Guide Audio..." };
    juce::Label timecodeReadout;
    juce::Label transportStatus;
    juce::Label statusMessage;
    PictureView preview;
    SettingsPanel settingsPanel;
    juce::TooltipWindow tooltips { this };
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool isDragHovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PictureLockEditor)
};

} // namespace picturelock
