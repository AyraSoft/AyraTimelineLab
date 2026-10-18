/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / ActionGlyphs
   ------------------------------------------------------------------------------
   Glifi d'azione PROCEDURALI (plus / minus / close) come `juce::Path` buildati
   in codice, non blob byte-array: shape geometriche banali che non vale la pena
   esportare da un SVG. Pensati per `ayra::ShapeIconButton::setShape`, che FA IL
   FILL della path -> i glifi sono shape PIENE (barre), non stroke.

   Coordinate su griglia 0..100: `ShapeIconButton` le scala a fit mantenendo le
   proporzioni, quindi la griglia e' arbitraria purche' coerente.

   Doctrine: header-only (funzioni inline), ZERO #include (juce gia' visibile via
   ayra_common.h), namespace ayra::glyphs, ASCII-only.
  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

/// "+" pieno: barra verticale + barra orizzontale incrociate (spessore 18/100).
inline juce::Path makePlus()
{
    juce::Path p;
    p.addRectangle (41.0f, 8.0f, 18.0f, 84.0f);   // verticale
    p.addRectangle (8.0f, 41.0f, 84.0f, 18.0f);   // orizzontale
    return p;
}

/// "-" pieno: una barra orizzontale centrata.
inline juce::Path makeMinus()
{
    juce::Path p;
    p.addRectangle (8.0f, 41.0f, 84.0f, 18.0f);
    return p;
}

/// "x" pieno: il "+" ruotato di 45 gradi attorno al centro (50,50).
inline juce::Path makeClose()
{
    auto p = makePlus();
    p.applyTransform (juce::AffineTransform::rotation (
        juce::MathConstants<float>::pi * 0.25f, 50.0f, 50.0f));
    return p;
}

} // namespace ayra::glyphs
