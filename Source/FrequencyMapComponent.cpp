#include "FrequencyMapComponent.h"

namespace
{
    void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float newValue)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (newValue));
    }
}

//==============================================================================
FrequencyMapComponent::FrequencyMapComponent (AudioPluginAudioProcessor& processorToUse,
                                               juce::Colour lowColourToUse,
                                               juce::Colour midColourToUse,
                                               juce::Colour highColourToUse,
                                               juce::Colour textColourToUse)
    : processorRef (processorToUse), apvts (processorToUse.apvts),
      lowColour (lowColourToUse), midColour (midColourToUse), highColour (highColourToUse), textColour (textColourToUse),
      spectrumColour (textColourToUse.withAlpha (0.35f))
{
    setInterceptsMouseClicks (true, false);
    smoothedSpectrumDb.fill (minSpectrumDb);
}

//==============================================================================
float FrequencyMapComponent::xToFreq (float x) const
{
    const auto proportion = juce::jlimit (0.0f, 1.0f, x / (float) juce::jmax (1, getWidth()));
    return minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, proportion);
}

float FrequencyMapComponent::freqToX (float freq) const
{
    const auto clamped = juce::jlimit (minFrequencyHz, maxFrequencyHz, freq);
    const auto proportion = std::log (clamped / minFrequencyHz) / std::log (maxFrequencyHz / minFrequencyHz);
    return proportion * (float) getWidth();
}

float FrequencyMapComponent::roundFrequencyForDisplay (float freq)
{
    if (freq < 1000.0f)
        return std::round (freq / 10.0f) * 10.0f;

    if (freq < 10000.0f)
        return std::round (freq / 100.0f) * 100.0f;

    return std::round (freq / 1000.0f) * 1000.0f;
}

int FrequencyMapComponent::getNumCrossovers() const
{
    if (auto* param = apvts.getRawParameterValue ("numCrossovers"))
        return juce::jlimit (0, 2, (int) std::round (param->load()));

    return 0;
}

std::vector<float> FrequencyMapComponent::getLineXPositions (int numCrossovers) const
{
    std::vector<float> xs;

    if (numCrossovers >= 1)
        xs.push_back (freqToX (apvts.getRawParameterValue ("freq1")->load()));

    if (numCrossovers >= 2)
        xs.push_back (freqToX (apvts.getRawParameterValue ("freq2")->load()));

    return xs;
}

int FrequencyMapComponent::regionIndexForX (float x, int numCrossovers) const
{
    if (numCrossovers == 0)
        return 0;

    const auto lineXs = getLineXPositions (numCrossovers);

    if (numCrossovers == 1)
        return x < lineXs[0] ? 0 : 2; // low : high (mid is skipped with a single crossover)

    if (x < lineXs[0]) return 0;      // low
    if (x < lineXs[1]) return 1;      // mid
    return 2;                          // high
}

void FrequencyMapComponent::addLine (float freq, int numCrossovers)
{
    freq = juce::jlimit (minFrequencyHz, maxFrequencyHz, freq);

    if (numCrossovers == 0)
    {
        setParam (apvts, "freq1", freq);
        setParam (apvts, "numCrossovers", 1.0f);
    }
    else if (numCrossovers == 1)
    {
        const auto existingFreq1 = apvts.getRawParameterValue ("freq1")->load();

        if (freq < existingFreq1)
        {
            setParam (apvts, "freq2", existingFreq1);
            setParam (apvts, "freq1", freq);
        }
        else
        {
            setParam (apvts, "freq2", juce::jmax (freq, existingFreq1 + 1.0f));
        }

        setParam (apvts, "numCrossovers", 2.0f);
    }
}

void FrequencyMapComponent::removeLine (int lineIndex, int numCrossovers)
{
    if (numCrossovers == 1)
    {
        // Dropping from 1 crossover to 0 collapses low+high into a single "low" band;
        // the high band's identity disappears, so its parameters reset to default.
        setParam (apvts, "numCrossovers", 0.0f);
        resetBandToDefault ("high");
    }
    else if (numCrossovers == 2)
    {
        if (lineIndex == 0)
        {
            // Keep the remaining boundary visually anchored: it becomes the new freq1.
            const auto freq2 = apvts.getRawParameterValue ("freq2")->load();
            setParam (apvts, "freq1", freq2);
        }

        // Dropping from 2 crossovers to 1 always removes the "mid" band's identity
        // (a single-crossover setup only ever has low/high), regardless of which
        // line was removed, so mid's parameters reset to default.
        setParam (apvts, "numCrossovers", 1.0f);
        resetBandToDefault ("mid");
    }
}

void FrequencyMapComponent::resetBandToDefault (const juce::String& bandParamPrefix)
{
    setParam (apvts, bandParamPrefix + "Intensity", 0.0f);
    setParam (apvts, bandParamPrefix + "Drive", 0.0f);
    setParam (apvts, bandParamPrefix + "Stereo", 100.0f);
}

//==============================================================================
void FrequencyMapComponent::setSelectedBand (int bandIndex)
{
    selectedBandIndex = bandIndex;
    repaint();
}

void FrequencyMapComponent::setSpectrumColour (juce::Colour colour)
{
    spectrumColour = colour;
}

void FrequencyMapComponent::setBackgroundColour (juce::Colour colour)
{
    backgroundColour = colour;
}

void FrequencyMapComponent::updateSpectrum()
{
    processorRef.getLatestSpectrum (rawSpectrumMagnitudes);

    const auto fftSize = (float) AudioPluginAudioProcessor::spectrumFftSize;

    for (size_t i = 0; i < rawSpectrumMagnitudes.size(); ++i)
    {
        const auto newDb = juce::jlimit (minSpectrumDb, maxSpectrumDb,
                                          juce::Decibels::gainToDecibels (rawSpectrumMagnitudes[i] / fftSize, minSpectrumDb));

        smoothedSpectrumDb[i] = 0.7f * smoothedSpectrumDb[i] + 0.3f * newDb;
    }
}

//==============================================================================
void FrequencyMapComponent::mouseMove (const juce::MouseEvent& e)
{
    hasHover = true;
    hoverX = e.position.x;
    repaint();
}

void FrequencyMapComponent::mouseExit (const juce::MouseEvent&)
{
    hasHover = false;
    repaint();
}

void FrequencyMapComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto numCrossovers = getNumCrossovers();
    const auto lineXs = getLineXPositions (numCrossovers);

    for (int i = 0; i < (int) lineXs.size(); ++i)
    {
        if (std::abs (e.position.x - lineXs[(size_t) i]) <= lineHitToleranceInPixels)
        {
            if (e.mods.isRightButtonDown())
            {
                removeLine (i, numCrossovers);
                repaint();
                return;
            }

            draggingLineIndex = i;
            return;
        }
    }

    if (e.mods.isRightButtonDown())
        return;

    const auto bandIndex = regionIndexForX (e.position.x, numCrossovers);

    if (onBandSelected)
        onBandSelected (bandIndex);

    if (numCrossovers < 2)
        addLine (xToFreq (e.position.x), numCrossovers);

    repaint();
}

void FrequencyMapComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingLineIndex < 0)
        return;

    // Keep the hover frequency readout live while dragging a crossover line.
    hasHover = true;
    hoverX = e.position.x;

    const auto numCrossovers = getNumCrossovers();
    const auto freq = xToFreq (juce::jlimit (0.0f, (float) getWidth(), e.position.x));

    if (numCrossovers == 1)
    {
        setParam (apvts, "freq1", juce::jlimit (minFrequencyHz, maxFrequencyHz, freq));
    }
    else if (numCrossovers == 2)
    {
        const auto freq1 = apvts.getRawParameterValue ("freq1")->load();
        const auto freq2 = apvts.getRawParameterValue ("freq2")->load();

        if (draggingLineIndex == 0)
            setParam (apvts, "freq1", juce::jlimit (minFrequencyHz, freq2 - 1.0f, freq));
        else
            setParam (apvts, "freq2", juce::jlimit (freq1 + 1.0f, maxFrequencyHz, freq));
    }

    repaint();
}

void FrequencyMapComponent::mouseUp (const juce::MouseEvent&)
{
    draggingLineIndex = -1;
}

//==============================================================================
void FrequencyMapComponent::paint (juce::Graphics& g)
{
    g.setColour (textColour.withAlpha (0.15f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);

    // Spectrum fill, drawn first so the crossover lines, band highlight and hover
    // text (drawn below) always remain clearly on top and readable.
    const auto sampleRate = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 44100.0;
    const auto fftSize    = (float) AudioPluginAudioProcessor::spectrumFftSize;

    juce::Path spectrumPath;
    bool spectrumPathStarted = false;

    for (size_t bin = 0; bin < smoothedSpectrumDb.size(); ++bin)
    {
        const auto freq = (float) bin * (float) sampleRate / fftSize;
        const auto x = freqToX (freq);
        const auto y = juce::jmap (smoothedSpectrumDb[bin], minSpectrumDb, maxSpectrumDb, (float) getHeight(), 0.0f);

        if (! spectrumPathStarted)
        {
            spectrumPath.startNewSubPath (x, y);
            spectrumPathStarted = true;
        }
        else
        {
            spectrumPath.lineTo (x, y);
        }
    }

    if (spectrumPathStarted)
    {
        spectrumPath.lineTo ((float) getWidth(), (float) getHeight());
        spectrumPath.lineTo (0.0f, (float) getHeight());
        spectrumPath.closeSubPath();

        g.setColour (spectrumColour);
        g.fillPath (spectrumPath);
    }

    const auto numCrossovers = getNumCrossovers();
    const auto lineXs = getLineXPositions (numCrossovers);

    // Highlight the selected band's region
    float regionStart = 0.0f;
    float regionEnd   = (float) getWidth();

    if (numCrossovers == 1)
    {
        if (selectedBandIndex == 0) regionEnd = lineXs[0];
        else                        regionStart = lineXs[0];
    }
    else if (numCrossovers == 2)
    {
        if (selectedBandIndex == 0)      regionEnd = lineXs[0];
        else if (selectedBandIndex == 1) { regionStart = lineXs[0]; regionEnd = lineXs[1]; }
        else                              regionStart = lineXs[1];
    }

    const auto highlightColour = selectedBandIndex == 0 ? lowColour
                                : selectedBandIndex == 1 ? midColour
                                                          : highColour;

    g.setColour (highlightColour.withAlpha (0.15f));
    g.fillRect (juce::Rectangle<float> (regionStart, 0.0f, regionEnd - regionStart, (float) getHeight()));

    // Crossover / band-boundary lines: a darker shade of the current background
    // rather than flat black, for an embedded-groove feel instead of a hard line.
    g.setColour (backgroundColour.darker (0.175f));
    for (auto x : lineXs)
        g.drawLine (x, 0.0f, x, (float) getHeight(), 2.0f);

    // Hover frequency readout
    if (hasHover)
    {
        const auto freq = roundFrequencyForDisplay (xToFreq (hoverX));
        const auto text = freq >= 1000.0f
            ? juce::String (freq / 1000.0f, 1) + " kHz"
            : juce::String ((int) freq) + " Hz";

        g.setColour (textColour);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (text, juce::Rectangle<float> (hoverX - 40.0f, 6.0f, 80.0f, 18.0f), juce::Justification::centred);
    }
}
