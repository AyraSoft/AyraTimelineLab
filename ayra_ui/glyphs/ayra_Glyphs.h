/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / Glyphs (FACADE)
   ------------------------------------------------------------------
   Punto d'ingresso unico per i glyph del design system. Include tutte le
   categorie (split per FORMA visiva, nomi generici: niente "node"/"graph") e
   definisce gli helper di disegno condivisi:
     - drawGlyph(...)        : fitta un path-data in un rettangolo (cache parsing)
     - drawCirclePortion(...): arco/anello (progress, CPU meter, ...)

   I path-data sono portati 1:1 (byte-esatti) da ayra_performer/headers/
   ayra_Shapes.h. Namespace unico: ayra::glyphs.

   Categorie:
     Transport  clock
     State      shutdown, bypass
     File       basket
     Layout     window
     Logo       vst3/vst2/au/lv2/ayra/juce
     Control    settings, sliders, midiPanic
     System     stack, thunderCircle, chip

   @note Mirror di ayra_Tokens.h (facade dei token): include i sotto + helper.
         JUCE arriva dal root del modulo (ayra_common.h) prima di questo file.

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

//==============================================================================
/** Disegna un glyph (path-data) fittato in `area`, proporzioni mantenute (come
    juce::ShapeButton::setShape(p, true, true, false)). Tinta = `colour`. */
inline void drawGlyph (juce::Graphics& g, const unsigned char* data, size_t numBytes,
                       juce::Rectangle<float> area, juce::Colour colour)
{
    if (area.isEmpty()) { return; }

    // Cache del path PARSATO (vedi `cachedPath` in ayra_Glyph.h): `loadPathFromData`
    // (parsing del blob) e' il costo vero e si ripeterebbe identico ad ogni frame;
    // lo eseguiamo una sola volta per blob. Una sola cache condivisa con `Glyph::path()`.
    //
    // Il fit per il rect corrente passa da un AffineTransform applicato al volo da
    // `g.fillPath` (overload const&): il point-array del path cache-ato resta intatto
    // e NON viene copiato ad ogni paint (era `juce::Path p = ...; p.scaleToFit(...)`,
    // una heap-copy per glyph per frame). `getTransformToScaleToFit(...,true)` usa
    // Justification::centred di default -> stesso risultato pixel di `scaleToFit(...,true)`.
    const juce::Path& p = cachedPath (data, numBytes);
    if (p.isEmpty()) { return; }
    g.setColour (colour);
    g.fillPath (p, p.getTransformToScaleToFit (area.getX(), area.getY(),
                                               area.getWidth(), area.getHeight(), true));
}

/** Overload UNICO pubblico: disegna un `Glyph` (SVG o binario) fittato in `area`.
    E' la forma da usare ovunque; la variante raw `(data, numBytes)` qui sopra resta
    solo come primitiva interna (ricette LAF). */
inline void drawGlyph (juce::Graphics& g, const Glyph& glyph,
                       juce::Rectangle<float> area, juce::Colour colour)
{
    const auto b = glyph.binary();
    if (b.data == nullptr) { return; }
    drawGlyph (g, b.data, b.size, area, colour);
}

//==============================================================================
/** Porzione di anello (arco) come ayra::drawCirclePortion reale: angolo 0 = ore
    9 (pi greco), cresce in senso orario; pStart/pEnd in 0..1 (frazione di giro). */
inline void drawCirclePortion (juce::Path& path, juce::Rectangle<float> bounds,
                               float pStart01, float pEnd01) noexcept
{
    pStart01 = juce::jlimit (0.0f, 1.0f, pStart01);
    pEnd01   = juce::jlimit (0.0f, 1.0f, pEnd01);
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float dia = radius * 2.0f;
    const float startA = juce::MathConstants<float>::pi + juce::MathConstants<float>::twoPi * pStart01;
    const float endA   = juce::MathConstants<float>::pi + juce::MathConstants<float>::twoPi * pEnd01;
    path.addArc (cx - radius, cy - radius, dia, dia, startA, endA, true);
}

/** Overload in percentuale 0..100 (come gli usi del NodeComponent reale). */
inline void drawCirclePortion (juce::Path& path, juce::Rectangle<float> bounds,
                               int pStart, int pEnd) noexcept
{
    drawCirclePortion (path, bounds, juce::jlimit (0, 100, pStart) / 100.0f,
                                     juce::jlimit (0, 100, pEnd)   / 100.0f);
}

} // namespace ayra::glyphs
