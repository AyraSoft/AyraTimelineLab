/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_Tokens.h
 * @brief Facade dei token del design system di ayra_common — "Frosted Lab,
 *        Daylight" (chrome) + "Dark Cosmic" (grafo).
 *
 * Aggrega i tre sotto-file di token (colore / geometria / tipografia) e
 * definisce gli helper di disegno CONDIVISI (crisp, zero blur, per-frame).
 *
 * # HELPER CONDIVISI (i "primitivi" riusabili del design system)
 *   - `fillWindowBackground` -> gradiente verticale di fondo.
 *   - `drawBevelRim`         -> stroke rounded-rect FULL-corner con gradiente
 *                               (zero angoli vivi — fix canonico).
 *   - `drawDualRim`          -> rim-light/alto + rim-dark/basso (via drawBevelRim).
 *   - `fillRaisedSurface`    -> RICETTA COERENTE raised (gradiente + dual-rim,
 *                               NIENTE drop-shadow clippata sui Component a se').
 *   - `drawCircleRims`       -> rim duale per thumb circolari (stile juce::Slider).
 *   Il fit della soft-shadow al margine (`fitShadow`) vive in `ayra_ChromePrimitives.h`.
 *
 * # NAMESPACE
 *   `ayra::tokens` — tutta la design-language di Tensor vive qui.
 *
 * # COSA INCLUDE
 *   - `..._Colours.h`    -> `StreamType`, `Mode`/`pick()`, surfaces, shadows,
 *                            rim, text, accent, `streamColour()`, `nodeColour()`.
 *   - `..._Geometry.h`   -> `radius::`, `depth::`.
 *   - `..._Typography.h` -> `type::` (Avenir Next + SF Mono).
 *
 * # USO
 * @code
 *   namespace tk = ayra::tokens;
 *   g.setColour (tk::accent());
 *   tk::fillWindowBackground (g, getLocalBounds().toFloat());
 *   tk::drawDualRim (g, r, tk::radius::card);
 * @endcode
 *
 * @note Le dipendenze JUCE (`<JuceHeader.h>`) sono incluse dal file root del
 *       modulo (`ayra_common.h`), NON qui — convenzione include dei moduli Ayra.
 *       Questo header assume `juce::Colour`/`juce::Graphics`/`juce::Font` gia'
 *       visibili.
 */

namespace ayra::tokens
{

//==============================================================================
// HELPER DI DISEGNO CONDIVISI (crisp, zero blur — chiamabili ad ogni frame).
//==============================================================================

/** Gradiente verticale di fondo finestra (base top -> bottom). */
inline void fillWindowBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::ColourGradient grad (surfaceBaseTop(), area.getX(), area.getY(),
                               surfaceBaseBot(), area.getX(), area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (area);
}

/** Bevel rim: stroke di un rounded-rect COMPLETAMENTE arrotondato (tutti e 4 gli
    angoli tondi) con gradiente verticale topCol -> botCol.

    NB: il vecchio approccio disegnava due `addRoundedRectangle` a corner PARZIALI
    (true,true,false,false / false,false,true,true). Quel path e' chiuso ma con 2
    angoli SQUADRATI: stroccandolo gli angoli vivi sporgevano -> "2 angoli vivi in
    alto + linea squadrata che si arrotonda solo in basso". Un singolo rounded-rect
    con gradiente elimina del tutto gli angoli vivi e da' lo stesso bevel. */
inline void drawBevelRim (juce::Graphics& g, juce::Rectangle<float> r, float corner,
                          juce::Colour topCol, juce::Colour botCol, float thickness = 1.0f)
{
    // Stroke INSET di mezzo spessore: lo stroke e' centrato sul path; sul bordo
    // esatto di `r` la sua meta' esterna verrebbe clippata ai bounds rettangolari
    // del componente (bordo piu' sottile / squadrato agli estremi). Insettando il
    // path lo stroke resta interamente dentro `r` -> resa identica per ogni caller,
    // a prescindere dal fatto che abbia gia' fatto reduce().
    const auto ri = r.reduced (thickness * 0.5f);
    juce::Path p;
    p.addRoundedRectangle (ri, juce::jmax (0.0f, corner - thickness * 0.5f));
    juce::ColourGradient grad (topCol, ri.getX(), ri.getY(), botCol, ri.getX(), ri.getBottom(), false);
    g.setGradientFill (grad);
    g.strokePath (p, juce::PathStrokeType (thickness));
}

/** Rim-light (alto) -> rim-dark (basso) su rounded rect: il "crisp" che da'
    definizione al letto soft. Tutti gli angoli tondi (vedi drawBevelRim). */
inline void drawDualRim (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    drawBevelRim (g, r, corner, rimLight(), rimDark(), 1.0f);
}

/** RICETTA COERENTE per una superficie chrome RAISED (bottone, combobox, tab,
    card): gradiente verticale sottile (chiaro sopra -> scuro sotto) + dual-rim.

    REGOLA DI COERENZA: i widget che sono `juce::Component` a se' NON usano una
    drop-shadow esterna per il rilievo. melatonin la renderizza dentro il contesto,
    che e' clippato ai bounds RETTANGOLARI del componente -> "piastra" squadrata,
    invisibile su tema scuro ma evidente su tema chiaro. Il rilievo qui viene SOLO
    da gradiente + bevel: identico in light e dark, zero clip. (Le INNER-shadow,
    che rendono dentro il path tondo, restano ok per gli stati recessed/pressed.) */
inline void fillRaisedSurface (juce::Graphics& g, juce::Rectangle<float> area, float corner,
                               juce::Colour base, float topBoost = 0.05f, float botCut = 0.05f)
{
    juce::Path p;
    p.addRoundedRectangle (area, corner);
    juce::ColourGradient grad (base.brighter (topBoost), area.getX(), area.getY(),
                               base.darker (botCut), area.getX(), area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (p);

    // FLOOR DI CONTORNO (solo tema scuro): il rim-light di drawDualRim e' white@0.10
    // sul dark, troppo tenue -> il bordo superiore del pannello sparisce nel fondo.
    // Una hairline garantita da' a raised lo stesso "pavimento" di contorno che il
    // recessed (e l'icon chip) hanno gia'. Sul tema chiaro il dual-rim basta da solo.
    if (detail::dark())
    {
        const auto ri = area.reduced (0.5f);
        juce::Path hp; hp.addRoundedRectangle (ri, juce::jmax (0.0f, corner - 0.5f));
        g.setColour (borderHairline());
        g.strokePath (hp, juce::PathStrokeType (1.0f));
    }

    drawDualRim (g, area, corner);
}

/** Dual-rim crisp per un CERCHIO raised (thumb stile juce::Slider): arco chiaro in
    alto-sinistra + arco scuro in basso-destra, spessore `t`. E' la rifinitura
    neumorfica canonica dei thumb circolari — niente anello accent pieno. */
inline void drawCircleRims (juce::Graphics& g, juce::Rectangle<float> bounds, float t = 1.5f)
{
    const auto rim = bounds.reduced (t * 0.5f);

    juce::Path hi;
    hi.addArc (rim.getX(), rim.getY(), rim.getWidth(), rim.getHeight(),
               juce::MathConstants<float>::pi * 1.25f,
               juce::MathConstants<float>::pi * 2.25f, true);
    g.setColour (rimLight());
    g.strokePath (hi, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path lo;
    lo.addArc (rim.getX(), rim.getY(), rim.getWidth(), rim.getHeight(),
               juce::MathConstants<float>::pi * 0.25f,
               juce::MathConstants<float>::pi * 1.25f, true);
    g.setColour (rimDark());
    g.strokePath (lo, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace ayra::tokens
