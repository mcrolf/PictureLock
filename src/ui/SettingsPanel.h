#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace picturelock
{

class PictureLockProcessor;

/** Controls for where the picture sits on the timeline and how it is shown. */
class SettingsPanel : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit SettingsPanel (PictureLockProcessor&);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showCurrentSettings();
    void setUpTimecodeEditor (juce::TextEditor&, std::function<void (const juce::String&)> commit);
    void commitSessionStart (const juce::String&);
    void commitPictureStart (const juce::String&);
    void usePictureStartFromMovie();

    PictureLockProcessor& processor;

    juce::Label timecodeRateLabel { {}, "Timecode rate" };
    juce::ComboBox timecodeRate;
    juce::Label sessionStartLabel { {}, "Session start (song position 0)" };
    juce::TextEditor sessionStart;
    juce::Label pictureStartLabel { {}, "Picture start" };
    juce::TextEditor pictureStart;
    juce::TextButton pictureStartFromMovie { "From Movie" };
    juce::TextButton pictureStartAtPlayhead { "At Playhead" };
    juce::Label syncOffsetLabel { {}, "Sync offset (ms, + delays picture)" };
    juce::Slider syncOffset { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label syncOffsetFrames;
    juce::ToggleButton timecodeBurnIn { "Timecode burn-in" };
};

} // namespace picturelock
