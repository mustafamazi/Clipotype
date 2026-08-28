#include "PillToggleButton.h"

namespace
{
    constexpr float pillWidth  = 32.0f;
    constexpr float pillHeight = 16.0f;
    constexpr float pressedScale = 0.95f;

    const juce::Colour idleOutlineColour { 0xFFBBBBBB };

    // ~60ms snappy press feedback at 60fps: exp(-1 / (60 * 0.06))
    constexpr float pressSmoothingCoeff = 0.78f;
}

//==============================================================================
PillToggleButton::PillToggleButton (juce::String labelText,
                                    juce::Colour normalTextColourToUse,
                                    juce::Colour activeFillColourToUse,
                                    juce::Colour activeTextColourToUse,
                                    bool glowWhenActiveToUse)
    : label (std::move (labelText)),
      normalTextColour (normalTextColourToUse),
      activeFillColour (activeFillColourToUse),
      activeTextColour (activeTextColourToUse),
      glowWhenActive (glowWhenActiveToUse)
{
    startTimerHz (60);
}

PillToggleButton::~PillToggleButton()
{
    stopTimer();
}

void PillToggleButton::setToggleState (bool shouldBeOn, juce::NotificationType notification)
{
    if (toggled == shouldBeOn)
        return;

    toggled = shouldBeOn;
    repaint();

    if (notification != juce::dontSendNotification && onToggle != nullptr)
        onToggle (toggled);
}

void PillToggleButton::mouseDown (const juce::MouseEvent&)
{
    pressScaleTarget = pressedScale;
}

void PillToggleButton::mouseUp (const juce::MouseEvent& e)
{
    pressScaleTarget = 1.0f;

    if (! e.mouseWasDraggedSinceMouseDown())
        setToggleState (! toggled, juce::sendNotification);
}

void PillToggleButton::timerCallback()
{
    const auto previous = pressScale;
    pressScale = pressSmoothingCoeff * pressScale + (1.0f - pressSmoothingCoeff) * pressScaleTarget;

    if (std::abs (pressScale - previous) > 0.0005f)
        repaint();
}

void PillToggleButton::paint (juce::Graphics& g)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto bounds = juce::Rectangle<float> (pillWidth * pressScale, pillHeight * pressScale)
                            .withCentre (centre);

    juce::Path pillPath;
    pillPath.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    if (toggled && glowWhenActive)
    {
        juce::DropShadow glow (activeFillColour.withAlpha (0.8f), 6, { 0, 0 });
        glow.drawForPath (g, pillPath);
    }

    if (toggled)
    {
        g.setColour (activeFillColour);
        g.fillPath (pillPath);
    }
    else
    {
        g.setColour (idleOutlineColour);
        g.strokePath (pillPath, juce::PathStrokeType (1.2f));
    }

    g.setColour (toggled ? activeTextColour : normalTextColour);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText (label, bounds, juce::Justification::centred);
}
