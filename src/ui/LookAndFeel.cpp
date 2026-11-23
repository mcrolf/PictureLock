#include "LookAndFeel.h"

namespace picturelock
{

PictureLockLookAndFeel::PictureLockLookAndFeel()
{
    using namespace palette;
    setColourScheme ({ background, panel, panel.brighter (0.08f), outline, text,
                       timecode.withAlpha (0.85f), juce::Colours::black, timecode, text });

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextEditor::backgroundColourId, background);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, timecode);
    setColour (juce::ComboBox::backgroundColourId, background);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::TextButton::buttonColourId, panel.brighter (0.06f));
    setColour (juce::TextButton::buttonOnColourId, timecode.darker (0.35f));
    setColour (juce::Slider::thumbColourId, timecode);
    setColour (juce::Slider::trackColourId, timecode.withAlpha (0.5f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, timecode);
}

juce::Font PictureLockLookAndFeel::timecodeFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::bold));
}

} // namespace picturelock
