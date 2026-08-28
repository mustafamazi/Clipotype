#pragma once

#include "PluginProcessor.h"

//==============================================================================
// The reserved area at the bottom of the editor: a logarithmic 20 Hz-20 kHz axis
// used to place, drag and remove crossover lines (driving numCrossovers/freq1/freq2),
// to pick a band by clicking the region it falls in, and to display the (smoothed)
// input spectrum as a filled backdrop behind all of the above.
class FrequencyMapComponent final : public juce::Component
{
public:
    FrequencyMapComponent (AudioPluginAudioProcessor& processorToUse,
                           juce::Colour lowColourToUse,
                           juce::Colour midColourToUse,
                           juce::Colour highColourToUse,
                           juce::Colour textColourToUse);

    void paint (juce::Graphics&) override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Called by the editor whenever the selected band changes elsewhere (e.g. the top labels),
    // so the highlighted region here stays in sync. bandIndex: 0 = low, 1 = mid, 2 = high.
    void setSelectedBand (int bandIndex);

    // Called by the editor every animation tick with a ready-to-use fill colour
    // (a dark shade of the current background, at the required opacity).
    void setSpectrumColour (juce::Colour colour);

    // Called by the editor every animation tick with the current (animated) background
    // colour, so the crossover lines can be drawn as a darker shade of it rather than
    // flat black -- an "embedded groove" look instead of a hard line.
    void setBackgroundColour (juce::Colour colour);

    // Called by the editor every animation tick (~30Hz is enough) to pull the latest
    // FFT magnitudes and blend them into the displayed spectrum.
    void updateSpectrum();

    // Fired when the user clicks a region; bandIndex uses the same 0/1/2 convention.
    std::function<void (int)> onBandSelected;

private:
    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float lineHitToleranceInPixels = 6.0f;
    static constexpr float minSpectrumDb = -100.0f;
    static constexpr float maxSpectrumDb = 0.0f;

    float xToFreq (float x) const;
    float freqToX (float freq) const;
    static float roundFrequencyForDisplay (float freq);

    int getNumCrossovers() const;
    std::vector<float> getLineXPositions (int numCrossovers) const;
    int regionIndexForX (float x, int numCrossovers) const;
    void addLine (float freq, int numCrossovers);
    void removeLine (int lineIndex, int numCrossovers);
    void resetBandToDefault (const juce::String& bandParamPrefix);

    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;
    const juce::Colour lowColour, midColour, highColour, textColour;
    juce::Colour spectrumColour;
    juce::Colour backgroundColour;

    int selectedBandIndex = 0;
    int draggingLineIndex = -1;
    bool hasHover = false;
    float hoverX = 0.0f;

    std::array<float, (size_t) AudioPluginAudioProcessor::spectrumNumBins> rawSpectrumMagnitudes {};
    std::array<float, (size_t) AudioPluginAudioProcessor::spectrumNumBins> smoothedSpectrumDb {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrequencyMapComponent)
};
