#pragma once

#include "PluginProcessor.h"
#include "ClipotypeLookAndFeel.h"
#include "FrequencyMapComponent.h"
#include "PillToggleButton.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Band { low, mid, high };

    void selectBand (Band newBand);
    void updateBandAttachments();
    void updateBandLabelVisibility();
    void timerCallback() override;
    int getNumCrossovers() const;
    juce::Colour computeTargetBackgroundColour() const;

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    void generateNoiseTexture();

    AudioPluginAudioProcessor& processorRef;
    ClipotypeLookAndFeel clipotypeLookAndFeel;
    juce::Typeface::Ptr titleTypeface;
    juce::Image noiseTexture;

    Band currentBand = Band::low;
    int lastKnownNumCrossovers = -1;

    juce::Colour currentBackgroundColour;

    juce::Label titleLabel;

    juce::TextButton lowButton  { "LOW" };
    juce::TextButton midButton  { "MID" };
    juce::TextButton highButton { "HIGH" };

    juce::Slider intensitySlider;
    juce::Slider driveSlider;
    juce::Slider stereoSlider;
    juce::Slider thresholdSlider;
    juce::Slider amountSlider;

    juce::Label intensityLabel;
    juce::Label driveLabel;
    juce::Label stereoLabel;
    juce::Label thresholdLabel;
    juce::Label amountLabel;

    // Cross-faded on top of the name labels above, showing the live numeric value
    // while the mouse hovers the corresponding knob (or the AMOUNT slider).
    juce::Label intensityValueLabel;
    juce::Label driveValueLabel;
    juce::Label stereoValueLabel;
    juce::Label thresholdValueLabel;
    juce::Label amountValueLabel;

    // Base (unhovered) bounds and current animated hover-scale for each knob
    juce::Rectangle<int> intensityBaseBounds, driveBaseBounds, stereoBaseBounds, thresholdBaseBounds;
    float intensityScale = 1.0f, driveScale = 1.0f, stereoScale = 1.0f, thresholdScale = 1.0f;
    float intensityTargetScale = 1.0f, driveTargetScale = 1.0f, stereoTargetScale = 1.0f, thresholdTargetScale = 1.0f;

    // 0 = name label fully visible, 1 = value label fully visible
    float intensityCrossfade = 0.0f, driveCrossfade = 0.0f, stereoCrossfade = 0.0f, thresholdCrossfade = 0.0f;
    float intensityCrossfadeTarget = 0.0f, driveCrossfadeTarget = 0.0f, stereoCrossfadeTarget = 0.0f, thresholdCrossfadeTarget = 0.0f;

    float amountThumbScale = 1.0f;
    float amountThumbTargetScale = 1.0f;
    float amountLabelCrossfade = 0.0f;
    float amountLabelCrossfadeTarget = 0.0f;

    PillToggleButton deltaButton  { "D", juce::Colour (ClipotypeLookAndFeel::textColourArgb),
                                   juce::Colour (0xFFFF8C42), juce::Colours::white, true };
    PillToggleButton bypassButton { "B", juce::Colour (ClipotypeLookAndFeel::textColourArgb),
                                   juce::Colour (0xFF3A3A3A), juce::Colour (0xFFD64545), false };

    FrequencyMapComponent frequencyMap;

    // AudioProcessorEditor already owns and auto-positions a resizableCorner member;
    // we only need to give it a constrainer with our size limits and aspect ratio.
    juce::ComponentBoundsConstrainer editorConstrainer;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> intensityAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> stereoAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
