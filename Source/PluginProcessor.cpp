#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    numCrossoversParam = apvts.getRawParameterValue ("numCrossovers");
    freq1Param         = apvts.getRawParameterValue ("freq1");
    freq2Param         = apvts.getRawParameterValue ("freq2");
    lowIntensityParam  = apvts.getRawParameterValue ("lowIntensity");
    midIntensityParam  = apvts.getRawParameterValue ("midIntensity");
    highIntensityParam = apvts.getRawParameterValue ("highIntensity");
    lowDriveParam      = apvts.getRawParameterValue ("lowDrive");
    midDriveParam      = apvts.getRawParameterValue ("midDrive");
    highDriveParam     = apvts.getRawParameterValue ("highDrive");
    lowStereoParam     = apvts.getRawParameterValue ("lowStereo");
    midStereoParam     = apvts.getRawParameterValue ("midStereo");
    highStereoParam    = apvts.getRawParameterValue ("highStereo");
    thresholdParam     = apvts.getRawParameterValue ("threshold");
    amountParam        = apvts.getRawParameterValue ("amount");
    deltaParam         = apvts.getRawParameterValue ("delta");
    bypassParam        = apvts.getRawParameterValue ("bypass");
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    const auto ovFactor     = (int) oversampling.getOversamplingFactor();
    const auto ovSampleRate = sampleRate * (double) ovFactor;
    const auto ovBlockSize  = samplesPerBlock * ovFactor;

    juce::dsp::ProcessSpec ovSpec;
    ovSpec.sampleRate       = ovSampleRate;
    ovSpec.maximumBlockSize = (juce::uint32) ovBlockSize;
    ovSpec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    crossover1LowFilter.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover1LowFilter.prepare (ovSpec);
    crossover1LowFilter.reset();

    crossover1HighFilter.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    crossover1HighFilter.prepare (ovSpec);
    crossover1HighFilter.reset();

    crossover2LowFilter.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover2LowFilter.prepare (ovSpec);
    crossover2LowFilter.reset();

    crossover2HighFilter.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    crossover2HighFilter.prepare (ovSpec);
    crossover2HighFilter.reset();

    const auto numChannels = (int) ovSpec.numChannels;
    lowBandBuffer.setSize (numChannels, ovBlockSize);
    midHighBuffer.setSize (numChannels, ovBlockSize);
    midBandBuffer.setSize (numChannels, ovBlockSize);
    highBandBuffer.setSize (numChannels, ovBlockSize);

    dryBuffer.setSize (numChannels, samplesPerBlock);

    const auto latencySamples = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (latencySamples);

    juce::dsp::ProcessSpec dryDelaySpec;
    dryDelaySpec.sampleRate       = sampleRate;
    dryDelaySpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    dryDelaySpec.numChannels      = (juce::uint32) numChannels;

    dryDelayLine.setMaximumDelayInSamples (latencySamples + 1);
    dryDelayLine.prepare (dryDelaySpec);
    dryDelayLine.setDelay ((float) latencySamples);
    dryDelayLine.reset();
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples  = buffer.getNumSamples();

    // Feed the spectrum analyser from the dry, original-sample-rate input, before
    // any processing (oversampling included) touches the signal.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float monoSample = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            monoSample += buffer.getReadPointer (channel)[sample];

        if (numChannels > 0)
            monoSample /= (float) numChannels;

        pushNextSampleIntoSpectrumFifo (monoSample);
    }

    // Capture the dry signal and delay it by the oversampling latency so it stays
    // phase-aligned with the wet signal for the final mix, which happens after downsampling.
    dryBuffer.makeCopyOf (buffer, true);
    juce::dsp::AudioBlock<float> dryBlock (dryBuffer);
    dryDelayLine.process (juce::dsp::ProcessContextReplacing<float> (dryBlock));

    const auto numCrossovers = juce::jlimit (0, 2, (int) std::round (numCrossoversParam->load()));

    const auto freq1 = juce::jlimit (20.0f, 20000.0f, freq1Param->load());
    // Guarantee freq2 > freq1 regardless of what the parameters are set to.
    const auto freq2 = juce::jlimit (20.0f, 20000.0f, juce::jmax (freq2Param->load(), freq1 + 1.0f));

    const auto thresholdGain = juce::Decibels::decibelsToGain (thresholdParam->load());

    // Upsample: all band processing below runs in the oversampled domain
    juce::dsp::AudioBlock<float> mainBlock (buffer);
    auto oversampledBlock = oversampling.processSamplesUp (mainBlock);
    const auto ovNumSamples = (int) oversampledBlock.getNumSamples();

    if (numCrossovers == 0)
    {
        // Single band: the whole signal is processed with the "low" parameters, no filtering
        lowBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);
        juce::dsp::AudioBlock<float> lowBlock (lowBandBuffer);
        lowBlock.copyFrom (oversampledBlock);

        applyWaveshaper (lowBandBuffer, thresholdGain, lowDriveParam->load(), lowIntensityParam->load());
        applyWidth (lowBandBuffer, lowStereoParam->load());

        for (int channel = 0; channel < numChannels; ++channel)
            std::copy_n (lowBandBuffer.getReadPointer (channel), ovNumSamples,
                         oversampledBlock.getChannelPointer ((size_t) channel));
    }
    else if (numCrossovers == 1)
    {
        // Two bands: a single crossover at freq1 splits into low and high
        crossover1LowFilter.setCutoffFrequency (freq1);
        crossover1HighFilter.setCutoffFrequency (freq1);

        lowBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);
        highBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);

        juce::dsp::AudioBlock<float> lowBlock (lowBandBuffer);
        juce::dsp::AudioBlock<float> highBlock (highBandBuffer);

        lowBlock.copyFrom (oversampledBlock);
        highBlock.copyFrom (oversampledBlock);

        crossover1LowFilter.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
        crossover1HighFilter.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

        applyWaveshaper (lowBandBuffer,  thresholdGain, lowDriveParam->load(),  lowIntensityParam->load());
        applyWaveshaper (highBandBuffer, thresholdGain, highDriveParam->load(), highIntensityParam->load());

        applyWidth (lowBandBuffer,  lowStereoParam->load());
        applyWidth (highBandBuffer, highStereoParam->load());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* outData  = oversampledBlock.getChannelPointer ((size_t) channel);
            auto* lowData  = lowBandBuffer.getReadPointer (channel);
            auto* highData = highBandBuffer.getReadPointer (channel);

            for (int sample = 0; sample < ovNumSamples; ++sample)
                outData[sample] = lowData[sample] + highData[sample];
        }
    }
    else // numCrossovers == 2
    {
        // Three bands: freq1 splits low from (mid+high), freq2 splits mid from high
        crossover1LowFilter.setCutoffFrequency (freq1);
        crossover1HighFilter.setCutoffFrequency (freq1);
        crossover2LowFilter.setCutoffFrequency (freq2);
        crossover2HighFilter.setCutoffFrequency (freq2);

        lowBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);
        midHighBuffer.setSize (numChannels, ovNumSamples, false, false, true);
        midBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);
        highBandBuffer.setSize (numChannels, ovNumSamples, false, false, true);

        juce::dsp::AudioBlock<float> lowBlock (lowBandBuffer);
        juce::dsp::AudioBlock<float> midHighBlock (midHighBuffer);

        lowBlock.copyFrom (oversampledBlock);
        midHighBlock.copyFrom (oversampledBlock);

        crossover1LowFilter.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
        crossover1HighFilter.process (juce::dsp::ProcessContextReplacing<float> (midHighBlock));

        juce::dsp::AudioBlock<float> midBlock (midBandBuffer);
        juce::dsp::AudioBlock<float> highBlock (highBandBuffer);

        midBlock.copyFrom (midHighBlock);
        highBlock.copyFrom (midHighBlock);

        crossover2LowFilter.process (juce::dsp::ProcessContextReplacing<float> (midBlock));
        crossover2HighFilter.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

        applyWaveshaper (lowBandBuffer,  thresholdGain, lowDriveParam->load(),  lowIntensityParam->load());
        applyWaveshaper (midBandBuffer,  thresholdGain, midDriveParam->load(),  midIntensityParam->load());
        applyWaveshaper (highBandBuffer, thresholdGain, highDriveParam->load(), highIntensityParam->load());

        applyWidth (lowBandBuffer,  lowStereoParam->load());
        applyWidth (midBandBuffer,  midStereoParam->load());
        applyWidth (highBandBuffer, highStereoParam->load());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* outData  = oversampledBlock.getChannelPointer ((size_t) channel);
            auto* lowData  = lowBandBuffer.getReadPointer (channel);
            auto* midData  = midBandBuffer.getReadPointer (channel);
            auto* highData = highBandBuffer.getReadPointer (channel);

            for (int sample = 0; sample < ovNumSamples; ++sample)
                outData[sample] = lowData[sample] + midData[sample] + highData[sample];
        }
    }

    // Downsample back to the original sample rate, writing the result into 'buffer'
    oversampling.processSamplesDown (mainBlock);

    // Delta: replace the wet signal with (wet - dry), with a fixed +12dB makeup gain
    // since the difference signal is usually much quieter. Applied before the global
    // Amount mix below, so at Amount = 100% the output is exactly the delta signal.
    if (deltaParam->load() > 0.5f)
    {
        const auto deltaMakeupGain = juce::Decibels::decibelsToGain (12.0f);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* outData = buffer.getWritePointer (channel);
            auto* dryData = dryBuffer.getReadPointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
                outData[sample] = (outData[sample] - dryData[sample]) * deltaMakeupGain;
        }
    }

    // Global dry/wet mix happens outside the oversampled path, after downsampling
    const auto wetGain = amountParam->load() / 100.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* outData = buffer.getWritePointer (channel);
        auto* dryData = dryBuffer.getReadPointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            outData[sample] = dryData[sample] + wetGain * (outData[sample] - dryData[sample]);
    }

    // Bypass: host-driven via getBypassParameter(), so latency compensation stays
    // correct. Passthrough uses the same latency-aligned dry signal used above, so
    // the reported plugin latency doesn't need to change when bypassed.
    if (bypassParam->load() > 0.5f)
        buffer.makeCopyOf (dryBuffer, true);
}

void AudioPluginAudioProcessor::pushNextSampleIntoSpectrumFifo (float sample) noexcept
{
    spectrumFifo[(size_t) spectrumFifoIndex++] = sample;

    if (spectrumFifoIndex == spectrumFftSize)
    {
        spectrumFifoIndex = 0;
        computeSpectrum();
    }
}

void AudioPluginAudioProcessor::computeSpectrum() noexcept
{
    std::array<float, (size_t) spectrumFftSize * 2> fftWorkBuffer {};
    std::copy (spectrumFifo.begin(), spectrumFifo.end(), fftWorkBuffer.begin());

    spectrumWindow.multiplyWithWindowingTable (fftWorkBuffer.data(), (size_t) spectrumFftSize);
    spectrumFft.performFrequencyOnlyForwardTransform (fftWorkBuffer.data(), true);

    // Publish into whichever buffer isn't currently the one the editor might be reading.
    const auto writeIndex = 1 - spectrumReadyBufferIndex.load (std::memory_order_relaxed);
    auto& target = spectrumMagnitudeBuffers[(size_t) writeIndex];

    for (int i = 0; i < spectrumNumBins; ++i)
        target[(size_t) i] = fftWorkBuffer[(size_t) i];

    spectrumReadyBufferIndex.store (writeIndex, std::memory_order_release);
}

void AudioPluginAudioProcessor::applyWaveshaper (juce::AudioBuffer<float>& bandBuffer,
                                                  float thresholdGain,
                                                  float drivePercent,
                                                  float intensityPercent)
{
    // Saturator: drive pushes the signal into the tanh curve and is never divided back out,
    // so the output stays pinned near the ceiling as drive increases (no automatic gain recovery).
    const auto driveGain = juce::Decibels::decibelsToGain ((drivePercent / 100.0f) * 30.0f);
    const auto m = intensityPercent / 100.0f;

    for (int channel = 0; channel < bandBuffer.getNumChannels(); ++channel)
    {
        auto* data = bandBuffer.getWritePointer (channel);

        for (int sample = 0; sample < bandBuffer.getNumSamples(); ++sample)
        {
            const auto x    = (data[sample] * driveGain) / thresholdGain;
            const auto soft = std::tanh (x);
            const auto hard = juce::jlimit (-1.0f, 1.0f, x);

            data[sample] = soft * (1.0f - m) + hard * m;
        }
    }
}

void AudioPluginAudioProcessor::applyWidth (juce::AudioBuffer<float>& bandBuffer, float widthPercent)
{
    if (bandBuffer.getNumChannels() < 2)
        return;

    const auto widthFactor = widthPercent / 100.0f;
    auto* left  = bandBuffer.getWritePointer (0);
    auto* right = bandBuffer.getWritePointer (1);

    for (int sample = 0; sample < bandBuffer.getNumSamples(); ++sample)
    {
        const auto mid  = 0.5f * (left[sample] + right[sample]);
        const auto side = 0.5f * (left[sample] - right[sample]) * widthFactor;

        left[sample]  = mid + side;
        right[sample] = mid - side;
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorWidth", lastEditorWidth, nullptr);
    state.setProperty ("editorHeight", lastEditorHeight, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xml);

            lastEditorWidth  = newState.getProperty ("editorWidth", lastEditorWidth);
            lastEditorHeight = newState.getProperty ("editorHeight", lastEditorHeight);

            apvts.replaceState (newState);
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    return
    {
        std::make_unique<juce::AudioParameterInt> ("numCrossovers", "Num Crossovers", 0, 2, 0),

        std::make_unique<juce::AudioParameterFloat> ("freq1", "Freq 1",
            juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f), 250.0f),

        std::make_unique<juce::AudioParameterFloat> ("freq2", "Freq 2",
            juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f), 3000.0f),

        std::make_unique<juce::AudioParameterFloat> ("lowIntensity", "Low Intensity",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("lowDrive", "Low Drive",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("lowStereo", "Low Stereo",
            juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f),

        std::make_unique<juce::AudioParameterFloat> ("midIntensity", "Mid Intensity",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("midDrive", "Mid Drive",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("midStereo", "Mid Stereo",
            juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f),

        std::make_unique<juce::AudioParameterFloat> ("highIntensity", "High Intensity",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("highDrive", "High Drive",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("highStereo", "High Stereo",
            juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f),

        std::make_unique<juce::AudioParameterFloat> ("threshold", "Threshold",
            juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), 0.0f),

        std::make_unique<juce::AudioParameterFloat> ("amount", "Amount",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f),

        std::make_unique<juce::AudioParameterBool> ("delta", "Delta", false),

        std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false)
    };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
