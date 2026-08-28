#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// A small (~32x16px) pill-shaped toggle used for the BYPASS and DELTA switches.
// Idle: hollow with a light grey outline, label in the normal text colour.
// Active: filled with activeFillColour, label in activeTextColour, and optionally
// a soft glow (used for DELTA's orange glow). Gives tactile press feedback by
// shrinking slightly between mouseDown and mouseUp.
class PillToggleButton final : public juce::Component,
                                private juce::Timer
{
public:
    PillToggleButton (juce::String labelText,
                      juce::Colour normalTextColourToUse,
                      juce::Colour activeFillColourToUse,
                      juce::Colour activeTextColourToUse,
                      bool glowWhenActiveToUse);
    ~PillToggleButton() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool getToggleState() const noexcept { return toggled; }
    void setToggleState (bool shouldBeOn, juce::NotificationType notification);

    std::function<void (bool)> onToggle;

private:
    void timerCallback() override;

    juce::String label;
    juce::Colour normalTextColour, activeFillColour, activeTextColour;
    bool glowWhenActive;

    bool toggled = false;
    float pressScale = 1.0f;
    float pressScaleTarget = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PillToggleButton)
};
