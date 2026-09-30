#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Flat, heptagon-based look: rotary knobs and the AMOUNT slider thumb are all
// drawn as a regular seven-sided polygon and given a soft drop shadow for a
// subtle 2.5D feel.
//
// Central font management: getTypefaceForFont() returns the embedded Roboto Mono
// typeface for every Font resolved through this look and feel. For this to take
// effect, this instance must be installed as BOTH the component-tree look and feel
// (Component::setLookAndFeel) AND the process-wide default
// (juce::LookAndFeel::setDefaultLookAndFeel) -- JUCE's Font/Typeface cache always
// resolves typefaces via LookAndFeel::getDefaultLookAndFeel(), never via whatever
// look and feel happens to be active on a particular component. A Font that already
// carries an explicit Typeface::Ptr (as the CLIPOTYPE title's Space Mono font does)
// bypasses this lookup entirely and is unaffected.
class ClipotypeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 textColourArgb = 0xFF1A1A1A;

    ClipotypeLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    static juce::Path buildHeptagonPath (juce::Point<float> centre, float radius);

private:
    juce::Typeface::Ptr robotoMonoTypeface;
};
