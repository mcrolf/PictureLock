#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace picturelock
{

namespace palette
{
    const juce::Colour background   { 0xff141619 };
    const juce::Colour panel        { 0xff1d2024 };
    const juce::Colour outline      { 0xff2e3238 };
    const juce::Colour text         { 0xffe7e9ec };
    const juce::Colour mutedText    { 0xff9aa1ab };
    const juce::Colour timecode     { 0xfff3b23e }; // amber, like a hardware timecode reader
    const juce::Colour playing      { 0xff4cc38a };
    const juce::Colour warning      { 0xffef6b5b };
}

class PictureLockLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PictureLockLookAndFeel();

    static juce::Font timecodeFont (float height);

    /** The PictureLock logo, drawn from the embedded SVG. */
    static std::unique_ptr<juce::Drawable> createLogo();
    static juce::Image createLogoImage (int size);
};

} // namespace picturelock
