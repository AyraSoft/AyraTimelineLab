/** @file LabLookAndFeel.h
    Backend visuale del banco. I widget inviano dati e geometrie, non ricette.
    Il tema live opzionale importa token e primitive canoniche in sola lettura.
    Il fallback e' uno snapshot fixture, non MultiverseLookAndFeel.
    @see CanonicalTheme.cpp, generated/ThemeData.h */
#pragma once
#include "ayra_ui.h"
#include "SharedChromeLookAndFeel.h"
#include "LabModel.h"
#include "clips/ayra_clips.h"
#include <array>

namespace ayra::lab
{
struct Palette
{
    juce::Colour base, top, bottom, raised, recessed, border, text, secondary, value;
    juce::Colour accent, mute, solo, play;
};

Palette readCanonicalPalette(bool dark);
void canonicalRaised(juce::Graphics&, juce::Rectangle<float>, float, juce::Colour);

struct Peak {float minimum=0,maximum=0;};
using Peaks=std::array<Peak,256>;
juce::Path makeSyntheticShape(int seed);
Peaks makeSyntheticPeaks(int seed); // only fixture setup; never decode or synthesize in paint.

class LabLookAndFeel : public SharedChromeLookAndFeel,
                       public clips::ClipLookAndFeelMethods
{
public:
    LabLookAndFeel();
    ~LabLookAndFeel() override = default;

    void setDark(bool);
    const Palette& colours() const noexcept {return palette;}
    const char* sourceLabel() const noexcept;
    void drawHeader(juce::Graphics&, juce::Rectangle<int>, const Track&, bool selected);
    void drawTrackGrid(juce::Graphics&, juce::Rectangle<int>, double pixelsPerSecond, double originX=0.0) const;
#if !AYRA_LAB_VISUALS_FROM_MULTIVERSE
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
#endif
private:
    Palette palette;
    bool dark=true;
    void raised(juce::Graphics&,juce::Rectangle<float>,float,juce::Colour) const;
};
}
