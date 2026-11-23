#include "SettingsPanel.h"

#include "LookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace picturelock
{

namespace
{
    constexpr double syncOffsetSliderRangeMilliseconds = 500.0;
}

SettingsPanel::SettingsPanel (PictureLockProcessor& p) : processor (p)
{
    for (auto* label : { &timecodeRateLabel, &sessionStartLabel, &pictureStartLabel, &syncOffsetLabel })
    {
        label->setColour (juce::Label::textColourId, palette::mutedText);
        label->setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (label);
    }

    int itemId = 1;
    for (const auto& rate : FrameRate::all())
        timecodeRate.addItem (rate.name, itemId++);

    timecodeRate.setTitle ("Timecode rate");
    timecodeRate.onChange = [this]
    {
        if (const auto index = timecodeRate.getSelectedItemIndex(); index >= 0)
            processor.settings().setTimecodeRate (FrameRate::all()[(size_t) index]);
    };
    addAndMakeVisible (timecodeRate);

    setUpTimecodeEditor (sessionStart, [this] (const juce::String& text) { commitSessionStart (text); });
    setUpTimecodeEditor (pictureStart, [this] (const juce::String& text) { commitPictureStart (text); });
    sessionStart.setTitle ("Session start timecode");
    pictureStart.setTitle ("Picture start timecode");

    pictureStartFromMovie.setTooltip ("Use the start timecode stored in the movie file");
    pictureStartFromMovie.onClick = [this] { usePictureStartFromMovie(); };
    pictureStartAtPlayhead.setTooltip ("Start the picture at Live's current song position");
    pictureStartAtPlayhead.onClick = [this] { processor.placePictureStartAtPlayhead(); };
    addAndMakeVisible (pictureStartFromMovie);
    addAndMakeVisible (pictureStartAtPlayhead);

    syncOffset.setTitle ("Sync offset in milliseconds");
    syncOffset.setRange (-syncOffsetSliderRangeMilliseconds, syncOffsetSliderRangeMilliseconds, 1.0);
    syncOffset.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
    syncOffset.setDoubleClickReturnValue (true, 0.0);
    syncOffset.onValueChange = [this] { processor.settings().setSyncOffsetMilliseconds (syncOffset.getValue()); };
    syncOffsetFrames.setColour (juce::Label::textColourId, palette::mutedText);
    syncOffsetFrames.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (syncOffset);
    addAndMakeVisible (syncOffsetFrames);

    timecodeBurnIn.onClick = [this] { processor.settings().setShowsTimecodeOverlay (timecodeBurnIn.getToggleState()); };
    addAndMakeVisible (timecodeBurnIn);

    processor.settings().addChangeListener (this);
    showCurrentSettings();
}

SettingsPanel::~SettingsPanel()
{
    processor.settings().removeChangeListener (this);
}

void SettingsPanel::setUpTimecodeEditor (juce::TextEditor& editor, std::function<void (const juce::String&)> commit)
{
    editor.setFont (PictureLockLookAndFeel::timecodeFont (17.0f));
    editor.setJustification (juce::Justification::centredLeft);
    editor.setInputRestrictions (12, "0123456789:;.-");
    editor.onReturnKey = [&editor, commit] { commit (editor.getText()); editor.giveAwayKeyboardFocus(); };
    editor.onFocusLost = [&editor, commit] { commit (editor.getText()); };
    editor.onEscapeKey = [this, &editor] { showCurrentSettings(); editor.giveAwayKeyboardFocus(); };
    addAndMakeVisible (editor);
}

void SettingsPanel::commitSessionStart (const juce::String& text)
{
    if (auto timecode = Timecode::parse (text, processor.settings().placement().timecodeRate))
        processor.settings().setSessionStart (*timecode);
    else
        showCurrentSettings();
}

void SettingsPanel::commitPictureStart (const juce::String& text)
{
    if (auto timecode = Timecode::parse (text, processor.settings().placement().timecodeRate))
        processor.settings().setPictureStart (*timecode);
    else
        showCurrentSettings();
}

void SettingsPanel::usePictureStartFromMovie()
{
    const auto info = processor.movieInfo();
    const auto rate = processor.settings().placement().timecodeRate;

    if (info && info->embeddedTimecode)
        if (auto timecode = Timecode::parse (*info->embeddedTimecode, rate))
            processor.settings().setPictureStart (*timecode);
}

void SettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    showCurrentSettings();
}

void SettingsPanel::showCurrentSettings()
{
    const auto& settings = processor.settings();
    const auto& placement = settings.placement();
    const auto& rates = FrameRate::all();
    const auto rateIndex = std::find (rates.begin(), rates.end(), placement.timecodeRate) - rates.begin();

    timecodeRate.setSelectedItemIndex ((int) rateIndex, juce::dontSendNotification);

    if (! sessionStart.hasKeyboardFocus (false))
        sessionStart.setText (Timecode (placement.sessionStartFrame, placement.timecodeRate).toString(), false);

    if (! pictureStart.hasKeyboardFocus (false))
        pictureStart.setText (Timecode (placement.pictureStartFrame, placement.timecodeRate).toString(), false);

    const auto info = processor.movieInfo();
    pictureStartFromMovie.setEnabled (info && info->embeddedTimecode);

    syncOffset.setValue (settings.syncOffsetMilliseconds(), juce::dontSendNotification);
    const auto offsetFrames = settings.syncOffsetMilliseconds() / 1000.0 * placement.timecodeRate.framesPerSecond();
    syncOffsetFrames.setText (juce::String (offsetFrames, 2) + " frames", juce::dontSendNotification);

    timecodeBurnIn.setToggleState (settings.showsTimecodeOverlay(), juce::dontSendNotification);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.setColour (palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void SettingsPanel::resized()
{
    constexpr int labelHeight = 18, rowHeight = 28, gap = 10;
    auto area = getLocalBounds().reduced (12);

    auto row = [&area] (int height) { auto r = area.removeFromTop (height); area.removeFromTop (4); return r; };

    timecodeRateLabel.setBounds (row (labelHeight));
    timecodeRate.setBounds (row (rowHeight));
    area.removeFromTop (gap);

    sessionStartLabel.setBounds (row (labelHeight));
    sessionStart.setBounds (row (rowHeight));
    area.removeFromTop (gap);

    pictureStartLabel.setBounds (row (labelHeight));
    pictureStart.setBounds (row (rowHeight));
    auto buttons = row (rowHeight);
    pictureStartFromMovie.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    pictureStartAtPlayhead.setBounds (buttons.removeFromRight (buttons.getWidth() - 4));
    area.removeFromTop (gap);

    syncOffsetLabel.setBounds (row (labelHeight));
    syncOffset.setBounds (row (rowHeight));
    syncOffsetFrames.setBounds (row (labelHeight));
    area.removeFromTop (gap);

    timecodeBurnIn.setBounds (row (rowHeight));
}

} // namespace picturelock
