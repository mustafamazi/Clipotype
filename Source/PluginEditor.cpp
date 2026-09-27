#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
namespace
{
    constexpr int radioGroupId = 1;

    // All row/knob sizes are expressed as a fraction of the reference 720x600 layout,
    // so resized() can scale everything proportionally as the window is resized.
    constexpr float titleHeightRatio      = 40.0f  / 600.0f;
    constexpr float bandRowHeightRatio    = 30.0f  / 600.0f;
    constexpr float knobRowHeightRatio    = 130.0f / 600.0f;
    constexpr float amountRowHeightRatio  = 50.0f  / 600.0f;
    constexpr float knobDiameterRatio     = 78.0f  / 600.0f; // yields a ~70px drawn heptagon after the LookAndFeel's padding
    constexpr float knobTopPaddingRatio   = 8.0f   / 600.0f;
    constexpr float amountLabelWidthRatio = 80.0f  / 720.0f;
    constexpr float deltaBypassWidthRatio = 90.0f  / 720.0f;
    constexpr float amountSliderGapRatio  = 10.0f  / 720.0f;
    constexpr float soloStripWidthRatio   = 50.0f  / 720.0f;
    constexpr float amountSliderInsetRatio = 14.0f / 600.0f;

    constexpr float knobHoverScale       = 1.06f;
    constexpr float amountThumbHoverScale = 1.15f;

    // ~120ms time constant at 60fps: exp(-1 / (60 * 0.12))
    constexpr float knobSmoothingCoeff = 0.87f;
    // ~200ms time constant at 60fps: exp(-1 / (60 * 0.2))
    constexpr float backgroundSmoothingCoeff = 0.92f;
    // ~150ms time constant at 60fps: exp(-1 / (60 * 0.15))
    constexpr float labelCrossfadeCoeff = 0.894f;

    const juce::Colour neutralBackground { 0xFFF2EDE4 };
    const juce::Colour lowBackground     { 0xFFC98872 };
    const juce::Colour midBackground     { 0xFF8098AD };
    const juce::Colour highBackground    { 0xFF9EAF82 };
    const juce::Colour textColour        { ClipotypeLookAndFeel::textColourArgb };

    void smoothTowards (float& current, float target, float coeff)
    {
        current = coeff * current + (1.0f - coeff) * target;
    }

    void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float newValue)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (newValue));
    }
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      frequencyMap (p, lowBackground, midBackground, highBackground, textColour)
{
    setLookAndFeel (&clipotypeLookAndFeel);
    // getTypefaceForFont() only takes effect when this instance is ALSO the process-wide
    // default look and feel, since JUCE's font/typeface cache resolves purely through
    // LookAndFeel::getDefaultLookAndFeel() rather than any per-component override.
    juce::LookAndFeel::setDefaultLookAndFeel (&clipotypeLookAndFeel);

    titleTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceMonoBold_ttf,
                                                              (size_t) BinaryData::SpaceMonoBold_ttfSize);

    titleLabel.setText ("CLIPOTYPE", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, textColour);
    titleLabel.setFont (juce::Font (juce::FontOptions (titleTypeface).withHeight (52.0f)));
    addAndMakeVisible (titleLabel);

    generateNoiseTexture();

    for (auto* button : { &lowButton, &midButton, &highButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (radioGroupId);
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::textColourOffId, textColour.withAlpha (0.4f));
        button->setColour (juce::TextButton::textColourOnId, textColour);
        addAndMakeVisible (button);
    }

    lowButton.onClick  = [this] { selectBand (Band::low); };
    midButton.onClick  = [this] { selectBand (Band::mid); };
    highButton.onClick = [this] { selectBand (Band::high); };

    auto knobLabelFont = juce::Font (juce::FontOptions (15.0f, juce::Font::bold)).withExtraKerningFactor (0.08f);

    auto configureKnob = [this, &knobLabelFont] (juce::Slider& slider, juce::Label& label, juce::Label& valueLabel,
                                                  const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (slider);
        slider.addMouseListener (this, false);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, textColour);
        label.setFont (knobLabelFont);
        addAndMakeVisible (label);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, textColour);
        valueLabel.setFont (knobLabelFont);
        valueLabel.setAlpha (0.0f);
        addAndMakeVisible (valueLabel);
    };

    configureKnob (intensitySlider, intensityLabel, intensityValueLabel, "INTENSITY");
    configureKnob (driveSlider,     driveLabel,     driveValueLabel,     "DRIVE");
    configureKnob (stereoSlider,    stereoLabel,    stereoValueLabel,    "STEREO");
    configureKnob (thresholdSlider, thresholdLabel, thresholdValueLabel, "THRESHOLD");

    amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (amountSlider);
    amountSlider.addMouseListener (this, false);

    auto amountLabelFont = juce::Font (juce::FontOptions (15.0f, juce::Font::bold)).withExtraKerningFactor (0.04f);

    amountLabel.setText ("AMOUNT", juce::dontSendNotification);
    amountLabel.setJustificationType (juce::Justification::centredLeft);
    amountLabel.setColour (juce::Label::textColourId, textColour);
    amountLabel.setFont (amountLabelFont);
    addAndMakeVisible (amountLabel);

    amountValueLabel.setJustificationType (juce::Justification::centredLeft);
    amountValueLabel.setColour (juce::Label::textColourId, textColour);
    amountValueLabel.setFont (amountLabelFont);
    amountValueLabel.setAlpha (0.0f);
    addAndMakeVisible (amountValueLabel);

    deltaButton.onToggle = [this] (bool isOn) { setParam (processorRef.apvts, "delta", isOn ? 1.0f : 0.0f); };
    bypassButton.onToggle = [this] (bool isOn) { setParam (processorRef.apvts, "bypass", isOn ? 1.0f : 0.0f); };
    addAndMakeVisible (deltaButton);
    addAndMakeVisible (bypassButton);

    soloButton.onToggle = [this] (bool isOn)
    {
        setParam (processorRef.apvts, "solo", isOn ? (float) ((int) currentBand + 1) : 0.0f);
    };
    addChildComponent (soloButton); // visibility is driven by updateBandLabelVisibility()

    addAndMakeVisible (frequencyMap);
    frequencyMap.onBandSelected = [this] (int bandIndex)
    {
        selectBand (bandIndex == 0 ? Band::low : bandIndex == 1 ? Band::mid : Band::high);
    };

    thresholdAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "threshold", thresholdSlider);
    amountAttachment    = std::make_unique<SliderAttachment> (processorRef.apvts, "amount", amountSlider);

    // Single source of truth for band selection: sets currentBand, syncs the top
    // labels' toggle state, the parameter attachments and the frequency map's highlight.
    // Start on the soloed band (if any), so selectBand() doesn't move a restored solo.
    const auto initialSolo = getSolo();
    const auto initialBand = initialSolo == 2 ? Band::mid : initialSolo == 3 ? Band::high : Band::low;
    selectBand (isBandAvailable (initialBand) ? initialBand : Band::low);
    updateBandLabelVisibility();

    currentBackgroundColour = computeTargetBackgroundColour();

    editorConstrainer.setSizeLimits (720, 600, 1440, 1200);
    editorConstrainer.setFixedAspectRatio (6.0 / 5.0);
    setConstrainer (&editorConstrainer);
    setResizable (true, true); // AudioProcessorEditor creates and auto-positions its own resizableCorner

    setSize (processorRef.getLastEditorWidth(), processorRef.getLastEditorHeight());
    startTimerHz (60);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
int AudioPluginAudioProcessorEditor::getNumCrossovers() const
{
    if (auto* param = processorRef.apvts.getRawParameterValue ("numCrossovers"))
        return juce::jlimit (0, 2, (int) std::round (param->load()));

    return 0;
}

int AudioPluginAudioProcessorEditor::getSolo() const
{
    if (auto* param = processorRef.apvts.getRawParameterValue ("solo"))
        return juce::jlimit (0, 3, (int) std::round (param->load()));

    return 0;
}

bool AudioPluginAudioProcessorEditor::isBandAvailable (Band band) const
{
    const auto numCrossovers = getNumCrossovers();
    return band == Band::mid ? numCrossovers >= 2 : numCrossovers >= 1;
}

juce::Colour AudioPluginAudioProcessorEditor::computeTargetBackgroundColour() const
{
    if (getNumCrossovers() == 0)
        return neutralBackground;

    return currentBand == Band::low  ? lowBackground
         : currentBand == Band::mid  ? midBackground
                                       : highBackground;
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    const auto numCrossovers = getNumCrossovers();

    if (numCrossovers != lastKnownNumCrossovers)
    {
        // Solo is meaningless with a single band: clear it when the user drops to 0
        // crossovers (only on a real transition, not on the editor's first tick).
        if (numCrossovers == 0 && lastKnownNumCrossovers > 0 && getSolo() != 0)
            setParam (processorRef.apvts, "solo", 0.0f);

        lastKnownNumCrossovers = numCrossovers;

        // If the currently selected band's label is no longer shown, fall back to LOW.
        if ((currentBand == Band::mid && numCrossovers < 2)
         || (currentBand == Band::high && numCrossovers < 1))
        {
            selectBand (Band::low);
        }

        updateBandLabelVisibility();
    }

    currentBackgroundColour = currentBackgroundColour.interpolatedWith (computeTargetBackgroundColour(),
                                                                         1.0f - backgroundSmoothingCoeff);

    smoothTowards (intensityScale,   intensityTargetScale,   knobSmoothingCoeff);
    smoothTowards (driveScale,       driveTargetScale,       knobSmoothingCoeff);
    smoothTowards (stereoScale,      stereoTargetScale,      knobSmoothingCoeff);
    smoothTowards (thresholdScale,   thresholdTargetScale,   knobSmoothingCoeff);
    smoothTowards (amountThumbScale, amountThumbTargetScale, knobSmoothingCoeff);

    auto applyKnobScale = [] (juce::Slider& slider, juce::Rectangle<int> baseBounds, float scale)
    {
        const auto scaledWidth  = (int) std::round ((float) baseBounds.getWidth()  * scale);
        const auto scaledHeight = (int) std::round ((float) baseBounds.getHeight() * scale);
        slider.setBounds (juce::Rectangle<int> (scaledWidth, scaledHeight).withCentre (baseBounds.getCentre()));
    };

    applyKnobScale (intensitySlider, intensityBaseBounds, intensityScale);
    applyKnobScale (driveSlider,     driveBaseBounds,     driveScale);
    applyKnobScale (stereoSlider,    stereoBaseBounds,    stereoScale);
    applyKnobScale (thresholdSlider, thresholdBaseBounds, thresholdScale);

    amountSlider.getProperties().set ("thumbScale", amountThumbScale);

    // Cross-fade each knob's name label with its live numeric value on hover
    smoothTowards (intensityCrossfade, intensityCrossfadeTarget, labelCrossfadeCoeff);
    smoothTowards (driveCrossfade,     driveCrossfadeTarget,     labelCrossfadeCoeff);
    smoothTowards (stereoCrossfade,    stereoCrossfadeTarget,    labelCrossfadeCoeff);
    smoothTowards (thresholdCrossfade, thresholdCrossfadeTarget, labelCrossfadeCoeff);

    auto applyCrossfade = [] (juce::Label& nameLabel, juce::Label& valueLabel, float crossfade,
                              juce::Slider& slider, bool isThresholdFormat)
    {
        nameLabel.setAlpha (1.0f - crossfade);
        valueLabel.setAlpha (crossfade);

        valueLabel.setText (isThresholdFormat
                                 ? juce::String (slider.getValue(), 1) + " dB"
                                 : juce::String ((int) std::round (slider.getValue())) + "%",
                             juce::dontSendNotification);
    };

    applyCrossfade (intensityLabel, intensityValueLabel, intensityCrossfade, intensitySlider, false);
    applyCrossfade (driveLabel,     driveValueLabel,     driveCrossfade,     driveSlider,     false);
    applyCrossfade (stereoLabel,    stereoValueLabel,    stereoCrossfade,    stereoSlider,    false);
    applyCrossfade (thresholdLabel, thresholdValueLabel, thresholdCrossfade, thresholdSlider, true);

    smoothTowards (amountLabelCrossfade, amountLabelCrossfadeTarget, labelCrossfadeCoeff);
    applyCrossfade (amountLabel, amountValueLabel, amountLabelCrossfade, amountSlider, false);

    // Keep the DELTA/BYPASS switches in sync in case the parameters changed from
    // outside this UI (host automation, or the host's own bypass control).
    deltaButton.setToggleState (processorRef.apvts.getRawParameterValue ("delta")->load() > 0.5f,
                                juce::dontSendNotification);
    bypassButton.setToggleState (processorRef.apvts.getRawParameterValue ("bypass")->load() > 0.5f,
                                 juce::dontSendNotification);

    // Same for SOLO. If automation soloed a band other than the selected one, follow
    // it, so the "S" button always refers to the band whose controls are shown.
    const auto solo = getSolo();
    soloButton.setToggleState (solo != 0, juce::dontSendNotification);

    if (solo != 0)
    {
        const auto soloBand = solo == 1 ? Band::low : solo == 2 ? Band::mid : Band::high;

        if (soloBand != currentBand && isBandAvailable (soloBand))
            selectBand (soloBand);
    }

    // Spectrum: pull the latest FFT magnitudes and tint the fill with a dark shade
    // of the current (animated) background colour.
    frequencyMap.updateSpectrum();
    frequencyMap.setSpectrumColour (currentBackgroundColour.darker (0.45f).withAlpha (0.35f));
    frequencyMap.setBackgroundColour (currentBackgroundColour);

    repaint();
    intensitySlider.repaint();
    driveSlider.repaint();
    stereoSlider.repaint();
    thresholdSlider.repaint();
    amountSlider.repaint();
    intensityLabel.repaint();
    driveLabel.repaint();
    stereoLabel.repaint();
    thresholdLabel.repaint();
    intensityValueLabel.repaint();
    driveValueLabel.repaint();
    stereoValueLabel.repaint();
    thresholdValueLabel.repaint();
    amountLabel.repaint();
    amountValueLabel.repaint();
    frequencyMap.repaint();
}

void AudioPluginAudioProcessorEditor::updateBandLabelVisibility()
{
    const auto numCrossovers = getNumCrossovers();

    lowButton.setVisible  (numCrossovers >= 1);
    midButton.setVisible  (numCrossovers >= 2);
    highButton.setVisible (numCrossovers >= 1);
    soloButton.setVisible (numCrossovers >= 1);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::mouseEnter (const juce::MouseEvent& e)
{
    if (e.eventComponent == &intensitySlider) { intensityTargetScale = knobHoverScale; intensityCrossfadeTarget = 1.0f; }
    else if (e.eventComponent == &driveSlider) { driveTargetScale = knobHoverScale; driveCrossfadeTarget = 1.0f; }
    else if (e.eventComponent == &stereoSlider) { stereoTargetScale = knobHoverScale; stereoCrossfadeTarget = 1.0f; }
    else if (e.eventComponent == &thresholdSlider) { thresholdTargetScale = knobHoverScale; thresholdCrossfadeTarget = 1.0f; }
    else if (e.eventComponent == &amountSlider) amountLabelCrossfadeTarget = 1.0f;
}

void AudioPluginAudioProcessorEditor::mouseExit (const juce::MouseEvent& e)
{
    if (e.eventComponent == &intensitySlider) { intensityTargetScale = 1.0f; intensityCrossfadeTarget = 0.0f; }
    else if (e.eventComponent == &driveSlider) { driveTargetScale = 1.0f; driveCrossfadeTarget = 0.0f; }
    else if (e.eventComponent == &stereoSlider) { stereoTargetScale = 1.0f; stereoCrossfadeTarget = 0.0f; }
    else if (e.eventComponent == &thresholdSlider) { thresholdTargetScale = 1.0f; thresholdCrossfadeTarget = 0.0f; }
    else if (e.eventComponent == &amountSlider) { amountThumbTargetScale = 1.0f; amountLabelCrossfadeTarget = 0.0f; }
}

void AudioPluginAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    if (e.eventComponent == &amountSlider)
    {
        const auto thumbX = amountSlider.getPositionOfValue (amountSlider.getValue());
        const auto hovered = std::abs (e.position.x - thumbX) <= 20.0f;
        amountThumbTargetScale = hovered ? amountThumbHoverScale : 1.0f;
    }
}

//==============================================================================
void AudioPluginAudioProcessorEditor::selectBand (Band newBand)
{
    // Single source of truth for the selected band: every view that reflects the
    // selection (top labels, frequency map highlight, bound parameters) is resynced
    // here, regardless of what triggered the change (a label click, a frequency map
    // click, or the numCrossovers fallback in timerCallback()).
    currentBand = newBand;

    lowButton.setToggleState  (newBand == Band::low,  juce::dontSendNotification);
    midButton.setToggleState  (newBand == Band::mid,  juce::dontSendNotification);
    highButton.setToggleState (newBand == Band::high, juce::dontSendNotification);

    // While soloing, the solo follows the selection to the new band.
    if (getSolo() != 0)
        setParam (processorRef.apvts, "solo", (float) ((int) currentBand + 1));

    updateBandAttachments();
    frequencyMap.setSelectedBand ((int) currentBand);
    repaint();
}

void AudioPluginAudioProcessorEditor::updateBandAttachments()
{
    // Destroy the old attachments first so the sliders don't briefly show a
    // stale value before being re-bound to the newly selected band's parameters.
    intensityAttachment.reset();
    driveAttachment.reset();
    stereoAttachment.reset();

    const auto intensityParamId = currentBand == Band::low  ? "lowIntensity"
                                 : currentBand == Band::mid  ? "midIntensity"
                                                              : "highIntensity";

    const auto driveParamId = currentBand == Band::low  ? "lowDrive"
                             : currentBand == Band::mid  ? "midDrive"
                                                           : "highDrive";

    const auto stereoParamId = currentBand == Band::low  ? "lowStereo"
                              : currentBand == Band::mid  ? "midStereo"
                                                            : "highStereo";

    intensityAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, intensityParamId, intensitySlider);
    driveAttachment     = std::make_unique<SliderAttachment> (processorRef.apvts, driveParamId, driveSlider);
    stereoAttachment    = std::make_unique<SliderAttachment> (processorRef.apvts, stereoParamId, stereoSlider);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::generateNoiseTexture()
{
    // A small tileable noise texture, generated once so paint() never has to
    // re-randomise pixels on every repaint.
    constexpr int tileSize = 128;
    noiseTexture = juce::Image (juce::Image::RGB, tileSize, tileSize, false);

    juce::Random random;
    juce::Image::BitmapData bitmap (noiseTexture, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < tileSize; ++y)
    {
        for (int x = 0; x < tileSize; ++x)
        {
            const auto grey = (juce::uint8) random.nextInt (256);
            bitmap.setPixelColour (x, y, juce::Colour (grey, grey, grey));
        }
    }
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (currentBackgroundColour);

    // Subtle film-grain noise overlay, tiled from the pre-generated texture.
    g.setOpacity (0.015f);

    for (int y = 0; y < getHeight(); y += noiseTexture.getHeight())
        for (int x = 0; x < getWidth(); x += noiseTexture.getWidth())
            g.drawImageAt (noiseTexture, x, y);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // Persist the current size so it round-trips through getStateInformation().
    processorRef.setLastEditorSize (getWidth(), getHeight());

    auto area = getLocalBounds();
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();

    auto heightPx = [h] (float ratio) { return (int) std::round (h * ratio); };
    auto widthPx  = [w] (float ratio) { return (int) std::round (w * ratio); };

    titleLabel.setBounds (area.removeFromTop (heightPx (titleHeightRatio)));

    auto bandRow = area.removeFromTop (heightPx (bandRowHeightRatio));

    // SOLO pill sits at the right end of the band row; an equal strip is trimmed on
    // the left so the three band labels stay centred. 28px tall = 16px pill + glow margin.
    const auto soloStripWidth = widthPx (soloStripWidthRatio);
    bandRow.removeFromLeft (soloStripWidth);
    soloButton.setBounds (bandRow.removeFromRight (soloStripWidth).withSizeKeepingCentre (soloStripWidth, 28));

    const auto bandButtonWidth = bandRow.getWidth() / 3;
    lowButton.setBounds  (bandRow.removeFromLeft (bandButtonWidth));
    midButton.setBounds  (bandRow.removeFromLeft (bandButtonWidth));
    highButton.setBounds (bandRow.removeFromLeft (bandButtonWidth));

    auto knobRow = area.removeFromTop (heightPx (knobRowHeightRatio));
    const auto knobColumnWidth = knobRow.getWidth() / 4;
    const auto knobDiameter    = heightPx (knobDiameterRatio);
    const auto knobTopPadding  = heightPx (knobTopPaddingRatio);

    auto layoutKnob = [&] (juce::Slider& slider, juce::Label& label, juce::Label& valueLabel,
                          juce::Rectangle<int>& baseBounds)
    {
        auto column = knobRow.removeFromLeft (knobColumnWidth);
        auto knobArea = column.removeFromTop (knobDiameter + knobTopPadding);

        baseBounds = juce::Rectangle<int> (knobDiameter, knobDiameter).withCentre (knobArea.getCentre());
        slider.setBounds (baseBounds);

        column.removeFromTop (2);
        label.setBounds (column);
        valueLabel.setBounds (column);
    };

    layoutKnob (intensitySlider, intensityLabel, intensityValueLabel, intensityBaseBounds);
    layoutKnob (driveSlider,     driveLabel,     driveValueLabel,     driveBaseBounds);
    layoutKnob (stereoSlider,    stereoLabel,    stereoValueLabel,    stereoBaseBounds);
    layoutKnob (thresholdSlider, thresholdLabel, thresholdValueLabel, thresholdBaseBounds);

    auto amountRow = area.removeFromTop (heightPx (amountRowHeightRatio));

    auto amountLabelArea = amountRow.removeFromLeft (widthPx (amountLabelWidthRatio));
    amountLabel.setBounds (amountLabelArea);
    amountValueLabel.setBounds (amountLabelArea);

    auto deltaBypassArea = amountRow.removeFromRight (widthPx (deltaBypassWidthRatio));
    amountSlider.setBounds (amountRow.reduced (widthPx (amountSliderGapRatio), heightPx (amountSliderInsetRatio)));

    // Each button is 28px tall: the 16px pill is centred inside, leaving 6px above
    // and below so the DropShadow glow isn't clipped. Those margins also act as the
    // gap between the two pills (12px pill-to-pill).
    auto deltaBypassColumn = deltaBypassArea.withSizeKeepingCentre (deltaBypassArea.getWidth(), 56);
    deltaButton.setBounds (deltaBypassColumn.removeFromTop (28));
    bypassButton.setBounds (deltaBypassColumn.removeFromTop (28));

    frequencyMap.setBounds (area); // everything left over is the reserved frequency map area
}
