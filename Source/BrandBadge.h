#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Small "BOPSAUDIO vX.Y.Z" maker's mark with a heptagon glyph, shown in the
// editor's top-left corner. Drawn with a translucent text colour so the animated
// background shows through; brightens on hover and opens the brand URL on click.
class BrandBadge final : public juce::Component,
                          private juce::Timer
{
public:
    explicit BrandBadge (juce::Colour textColourToUse);
    ~BrandBadge() override;

    // Width needed to fit the glyph, name and version without clipping.
    int getIdealWidth() const;

    void paint (juce::Graphics&) override;

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Colour textColour;
    juce::String versionText;

    float hoverAmount = 0.0f; // 0 = idle, 1 = hovered
    float hoverTarget = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrandBadge)
};
