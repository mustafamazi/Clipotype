#include "BrandBadge.h"
#include "ClipotypeLookAndFeel.h"

namespace
{
    const juce::String brandUrl { "https://bopsaudio.com" };

    const juce::String brandName { "BOPSAUDIO" };

    constexpr float glyphRadius = 5.0f; // ~10px heptagon
    constexpr float glyphGap    = 6.0f;
    constexpr float versionGap  = 6.0f;

    constexpr float idleAlpha    = 0.45f;
    constexpr float hoverAlpha   = 0.8f;
    constexpr float versionAlpha = 0.6f; // relative to the name, so the version reads fainter

    // ~150ms time constant at 60fps, matching the editor's label cross-fades
    constexpr float hoverSmoothingCoeff = 0.894f;

    juce::Font nameFont()
    {
        return juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.12f);
    }

    juce::Font versionFont()
    {
        return juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.04f);
    }
}

//==============================================================================
BrandBadge::BrandBadge (juce::Colour textColourToUse)
    : textColour (textColourToUse),
      versionText ("v" + juce::String (JucePlugin_VersionString))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (brandName);
    setDescription ("Opens " + brandUrl);
}

BrandBadge::~BrandBadge()
{
    stopTimer();
}

int BrandBadge::getIdealWidth() const
{
    const auto width = glyphRadius * 2.0f + glyphGap
                     + juce::GlyphArrangement::getStringWidth (nameFont(), brandName) + versionGap
                     + juce::GlyphArrangement::getStringWidth (versionFont(), versionText);

    return (int) std::ceil (width) + 2;
}

void BrandBadge::mouseEnter (const juce::MouseEvent&)
{
    hoverTarget = 1.0f;
    startTimerHz (60);
}

void BrandBadge::mouseExit (const juce::MouseEvent&)
{
    hoverTarget = 0.0f;
    startTimerHz (60);
}

void BrandBadge::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
        juce::URL (brandUrl).launchInDefaultBrowser();
}

void BrandBadge::timerCallback()
{
    hoverAmount = hoverSmoothingCoeff * hoverAmount + (1.0f - hoverSmoothingCoeff) * hoverTarget;

    if (std::abs (hoverAmount - hoverTarget) < 0.001f)
    {
        hoverAmount = hoverTarget;
        stopTimer();
    }

    repaint();
}

void BrandBadge::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto alpha = juce::jmap (hoverAmount, idleAlpha, hoverAlpha);
    const auto colour = textColour.withAlpha (alpha);

    auto glyphArea = bounds.removeFromLeft (glyphRadius * 2.0f);
    g.setColour (colour);
    g.fillPath (ClipotypeLookAndFeel::buildHeptagonPath (glyphArea.getCentre(), glyphRadius));
    bounds.removeFromLeft (glyphGap);

    // Name and version share one baseline, with the name's cap height centred on the glyph.
    const auto name = nameFont();
    const auto baseline = bounds.getCentreY() + (name.getAscent() - name.getDescent()) * 0.5f;

    g.setFont (name);
    g.drawSingleLineText (brandName, (int) bounds.getX(), (int) std::round (baseline));
    bounds.removeFromLeft (juce::GlyphArrangement::getStringWidth (name, brandName) + versionGap);

    g.setColour (textColour.withAlpha (alpha * versionAlpha));
    g.setFont (versionFont());
    g.drawSingleLineText (versionText, (int) bounds.getX(), (int) std::round (baseline));
}
