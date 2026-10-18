/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / legacy / NodeDrawings
   -------------------------------------------------------
   VENDOR as-is dell'helper di disegno ad arco del node graph, importato 1:1 da
   `ayra_performer/headers/ayra_Drawings.h` per preservare i pixel del canvas
   legacy durante il port a `ayra::GraphProcessor` (Tensor M1).

   Solo `drawCirclePortion` (entrambi gli overload int/float) effettivamente usato
   dal canvas; `static` -> `inline` per non emettere -Wunused-function nelle TU
   dell'app che includono ayra_common.h. Zero-include: `juce::Path`/`juce::Rectangle`
   /`juce::MathConstants` gia' visibili via la catena di inclusione di ayra_common.h.

  ==============================================================================
*/

#pragma once

namespace ayra
{

//==============================================================================
/** Disegna una porzione di arco (cerchio) nel `path`, in percentuale [0..100].
    Versione int: percentuali 0..100. */
inline void drawCirclePortion (juce::Path& path, const juce::Rectangle<int>& bounds,
                               int percentageStart, int percentageEnd) noexcept
{
    percentageStart = juce::jlimit (0, 100, percentageStart);
    percentageEnd   = juce::jlimit (0, 100, percentageEnd);

    const float centerX = bounds.getCentreX();
    const float centerY = bounds.getCentreY();
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;

    const float left     = centerX - radius;
    const float top      = centerY - radius;
    const float diameter = radius * 2.0f;

    const float startAngle = juce::MathConstants<float>::pi +
                             juce::MathConstants<float>::twoPi * (percentageStart / 100.0f);

    const float endAngle = juce::MathConstants<float>::pi +
                           juce::MathConstants<float>::twoPi * (percentageEnd / 100.0f);

    path.addArc (left, top, diameter, diameter, startAngle, endAngle, true);
}

//==============================================================================
/** Versione float: percentuali normalizzate 0..1. */
inline void drawCirclePortion (juce::Path& path, const juce::Rectangle<int>& bounds,
                               float percentageStart, float percentageEnd) noexcept
{
    percentageStart = juce::jlimit (0.0f, 1.0f, percentageStart);
    percentageEnd   = juce::jlimit (0.0f, 1.0f, percentageEnd);

    const float centerX = bounds.getCentreX();
    const float centerY = bounds.getCentreY();
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;

    const float left     = centerX - radius;
    const float top      = centerY - radius;
    const float diameter = radius * 2.0f;

    const float startAngle = juce::MathConstants<float>::pi +
                             juce::MathConstants<float>::twoPi * percentageStart;
    const float endAngle = juce::MathConstants<float>::pi +
                           juce::MathConstants<float>::twoPi * percentageEnd;

    path.addArc (left, top, diameter, diameter, startAngle, endAngle, true);
}

} // namespace ayra
