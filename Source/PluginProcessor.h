#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    // The editor reads/writes this so its last size round-trips through
    // getStateInformation()/setStateInformation() and is restored on reload.
    void setLastEditorSize (int width, int height) noexcept { lastEditorWidth = width; lastEditorHeight = height; }
    int getLastEditorWidth() const noexcept  { return lastEditorWidth; }
    int getLastEditorHeight() const noexcept { return lastEditorHeight; }

    //==============================================================================
    // Spectrum analyser: fed from the dry, pre-oversampling input signal.
    static constexpr int spectrumFftOrder = 11;
    static constexpr int spectrumFftSize  = 1 << spectrumFftOrder; // 2048
    static constexpr int spectrumNumBins  = spectrumFftSize / 2;

    // Thread-safe for the editor to poll: no locks, just an atomically-published index
    // into whichever of the two magnitude buffers was most recently completed.
    void getLatestSpectrum (std::array<float, (size_t) spectrumNumBins>& outMagnitudes) const noexcept
    {
        outMagnitudes = spectrumMagnitudeBuffers[(size_t) spectrumReadyBufferIndex.load (std::memory_order_acquire)];
    }

private:
    //==============================================================================
    // 4x oversampling: all band processing (crossover + waveshaper + width) runs
    // in the oversampled domain to reduce aliasing from the nonlinear waveshaper.
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    // Delays the dry signal by the oversampling latency so the final dry/wet mix,
    // which happens after downsampling, stays phase-aligned.
    juce::dsp::DelayLine<float> dryDelayLine;

    // Crossover filters for a variable band count (0, 1 or 2 crossovers -> 1, 2 or 3 bands):
    // crossover1 splits the input at freq1 into low and (mid+high); when numCrossovers == 1,
    // crossover1HighFilter's output *is* the high band directly (no further split).
    // crossover2 splits (mid+high) at freq2 into mid and high, only used when numCrossovers == 2.
    juce::dsp::LinkwitzRileyFilter<float> crossover1LowFilter;
    juce::dsp::LinkwitzRileyFilter<float> crossover1HighFilter;
    juce::dsp::LinkwitzRileyFilter<float> crossover2LowFilter;
    juce::dsp::LinkwitzRileyFilter<float> crossover2HighFilter;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midHighBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    std::atomic<float>* numCrossoversParam = nullptr;
    std::atomic<float>* freq1Param         = nullptr;
    std::atomic<float>* freq2Param         = nullptr;
    std::atomic<float>* lowIntensityParam  = nullptr;
    std::atomic<float>* midIntensityParam  = nullptr;
    std::atomic<float>* highIntensityParam = nullptr;
    std::atomic<float>* lowDriveParam      = nullptr;
    std::atomic<float>* midDriveParam      = nullptr;
    std::atomic<float>* highDriveParam     = nullptr;
    std::atomic<float>* lowStereoParam     = nullptr;
    std::atomic<float>* midStereoParam     = nullptr;
    std::atomic<float>* highStereoParam    = nullptr;
    std::atomic<float>* thresholdParam     = nullptr;
    std::atomic<float>* amountParam        = nullptr;
    std::atomic<float>* deltaParam         = nullptr;
    std::atomic<float>* bypassParam        = nullptr;
    std::atomic<float>* soloParam          = nullptr;

    int lastEditorWidth  = 720;
    int lastEditorHeight = 600;

    //==============================================================================
    // Parameter smoothing (removes zipper noise under automation / knob drags).
    // Sample-rate domains matter here: band parameters and threshold are consumed
    // per oversampled sample, so they're reset with the oversampled rate; Amount is
    // consumed after downsampling, so it's reset with the host rate. Crossover
    // frequencies are advanced per block (host rate) and applied once per block.
    static constexpr double smoothingRampSeconds = 0.025;

    enum BandIndex { lowBand = 0, midBand, highBand, numBands };

    struct BandSmoothers
    {
        // Drive is smoothed as a linear gain, multiplicatively (= linear in dB).
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain;
        juce::SmoothedValue<float> intensity; // 0..1 hard-clip blend
        juce::SmoothedValue<float> width;     // 0..2 side gain
    };

    std::array<BandSmoothers, numBands> bandSmoothers;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> thresholdGainSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq1Smoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq2Smoother;
    juce::SmoothedValue<float> amountSmoother; // 0..1 wet gain

    // Threshold is shared by every band, so it's advanced once per oversampled
    // sample into this scratch array and each band reads from it.
    std::vector<float> thresholdGainScratch;

    // Last band count seen by processBlock; the crossover filters are reset when it changes.
    int lastNumCrossovers = -1;

    void applyWidth (juce::AudioBuffer<float>& bandBuffer, BandIndex band);
    void applyWaveshaper (juce::AudioBuffer<float>& bandBuffer, BandIndex band);
    void skipBandSmoothers (BandIndex band, int numSamples);

    //==============================================================================
    juce::dsp::FFT spectrumFft { spectrumFftOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow { (size_t) spectrumFftSize,
                                                          juce::dsp::WindowingFunction<float>::hann };

    std::array<float, (size_t) spectrumFftSize> spectrumFifo {};
    int spectrumFifoIndex = 0;

    std::array<std::array<float, (size_t) spectrumNumBins>, 2> spectrumMagnitudeBuffers {};
    std::atomic<int> spectrumReadyBufferIndex { 0 };

    void pushNextSampleIntoSpectrumFifo (float sample) noexcept;
    void computeSpectrum() noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
