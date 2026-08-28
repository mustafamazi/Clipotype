#include "ClipotypeLookAndFeel.h"
#include <BinaryData.h>

ClipotypeLookAndFeel::ClipotypeLookAndFeel()
{
    robotoMonoTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::RobotoMonoVariableFont_wght_ttf,
                                                                   (size_t) BinaryData::RobotoMonoVariableFont_wght_ttfSize);
}

juce::Typeface::Ptr ClipotypeLookAndFeel::getTypefaceForFont (const juce::Font&)
{
    // Every font resolved through this look and feel uses Roboto Mono at its
    // default (Regular/Medium) instance. The CLIPOTYPE title bypasses this
    // entirely by carrying its own explicit Space Mono Typeface::Ptr.
    return robotoMonoTypeface;
}

juce::Path ClipotypeLookAndFeel::buildHeptagonPath (juce::Point<float> centre, float radius)
{
    constexpr int numSides = 7;
    juce::Path path;

    for (int i = 0; i < numSides; ++i)
    {
        const auto angle = juce::MathConstants<float>::twoPi * (float) i / (float) numSides;
        const auto point = centre.getPointOnCircumference (radius, angle);

        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    path.closeSubPath();
    return path;
}

void ClipotypeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;

    const auto heptagon = buildHeptagonPath (centre, radius);

    // Soft drop shadow, offset slightly downward, to lift the knob off the background
    juce::DropShadow dropShadow (juce::Colours::black.withAlpha (0.25f), 8, { 0, 3 });
    dropShadow.drawForPath (g, heptagon);

    // Flat heptagon body
    g.setColour (juce::Colour (textColourArgb));
    g.fillPath (heptagon);

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Thick white indicator line from centre outward, rotating with the slider's value
    const auto tip = centre.getPointOnCircumference (radius * 0.85f, angle);

    juce::Path indicator;
    indicator.startNewSubPath (centre);
    indicator.lineTo (tip);

    g.setColour (juce::Colours::white);
    g.strokePath (indicator, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ClipotypeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                              juce::Slider::SliderStyle, juce::Slider& slider)
{
    constexpr float trackHeight = 14.0f;
    constexpr float thumbWidth  = 26.0f;
    constexpr float thumbHeight = 22.0f;

    const auto trackY = (float) y + (float) height * 0.5f;
    const auto trackBounds = juce::Rectangle<float> ((float) x, trackY - trackHeight * 0.5f, (float) width, trackHeight);

    // "Elevator" style track: a soft rounded slot
    g.setColour (juce::Colour (textColourArgb).withAlpha (0.16f));
    g.fillRoundedRectangle (trackBounds, trackHeight * 0.5f);

    const auto thumbScale = (float) slider.getProperties().getWithDefault ("thumbScale", 1.0);
    const auto scaledThumbWidth  = thumbWidth  * thumbScale;
    const auto scaledThumbHeight = thumbHeight * thumbScale;

    const auto thumbBounds = juce::Rectangle<float> (scaledThumbWidth, scaledThumbHeight)
                                .withCentre ({ sliderPos, trackY });

    g.setColour (juce::Colour (textColourArgb));
    g.fillRoundedRectangle (thumbBounds, scaledThumbHeight * 0.4f);
}

void ClipotypeLookAndFeel::drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                                  bool, bool)
{
    // Band selector buttons render as plain clickable text with no background at all.
}

juce::Font ClipotypeLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (15.0f, juce::Font::bold)).withExtraKerningFactor (0.08f);
}
