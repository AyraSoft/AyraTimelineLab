/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_Tokens_Geometry.h
 * @brief Token di GEOMETRIA e PROFONDITA' del design system di ayra_common.
 *
 * Raggi degli angoli (`radius::`) e costanti di profondita' neumorfica
 * (`depth::`) — blur radius, offset, inset. Valori dalla spec di design
 * approvata (Frosted Lab / CleanMyMac).
 *
 * @note Questo file NON va incluso direttamente: usa il facade
 *       `ayra_Tokens.h`.
 */

namespace ayra::tokens
{

//==============================================================================
// GEOMETRY — raggi standard degli angoli.
//==============================================================================
namespace radius
{
    inline constexpr float control = 9.0f;   ///< slider/combo/control generico.
    inline constexpr float button  = 11.0f;  ///< bottoni (CleanMyMac-like rounding).
    inline constexpr float card    = 16.0f;  ///< card/pannelli/nodi.
    inline constexpr float popup   = 14.0f;  ///< popup/menu/dialog.
    // pill = full: usare height * 0.5f (toggle, badge, search field).
}

//==============================================================================
// DEPTH — raggi/offset standard della profondita' (neumorphism duale).
//  Convenzione offset: dark = (+o, +o) bottom-right, light = (-o, -o) top-left.
//==============================================================================
namespace depth
{
    inline constexpr int raisedBlurDark   = 10;  ///< blur ombra scura (raised).
    inline constexpr int raisedBlurLight  = 9;   ///< blur ombra chiara (raised).
    inline constexpr int raisedOffset     = 3;   ///< offset ombra raised.
    inline constexpr int raisedInset      = 8;   ///< margine che il widget lascia alla soft shadow.
    inline constexpr int recessedBlurIn   = 8;   ///< blur inner-shadow scura (recessed).
    inline constexpr int recessedBlurHi   = 6;   ///< blur inner-shadow chiara (recessed).
    inline constexpr int recessedOffset   = 2;   ///< offset inner-shadow recessed.
    inline constexpr int hoverBlurBoost   = 3;   ///< hover: +blur (mai muovere il contenuto).
    inline constexpr int glowBlur         = 12;  ///< alone accent (CTA / ON) stile CleanMyMac.
    inline constexpr int popupBlur        = 24;  ///< blur ombra popup.
    inline constexpr int popupOffsetY     = 8;   ///< offset verticale ombra popup.
}

//==============================================================================
// LAYOUT & METRICS — altezze canoniche, padding e distanze tra elementi UI.
//==============================================================================
namespace layout
{
    inline constexpr int buttonH       = 30;   ///< altezza standard dei bottoni d'azione (action/reset).
    inline constexpr int resetButtonW  = 140;  ///< larghezza standard per "Reset to defaults".
    inline constexpr int rowH          = 34;   ///< altezza riga semplice (solo label).
    inline constexpr int rowH2         = 46;   ///< altezza riga estesa (label + caption).
    inline constexpr int rowGap        = 6;    ///< gap verticale standard tra controlli correlati.
    inline constexpr int actionGap     = rowGap * 2; ///< gap di separazione per footer/azioni (12px).
    inline constexpr int subHeaderH    = 26;   ///< altezza sotto-header di raggruppamento interno.
    inline constexpr int headerH       = 22;   ///< altezza header di sezione interna.
    inline constexpr int cardPad       = 18;   ///< padding standard interno card/sezioni.
    inline constexpr int windowHeaderH     = 56;   ///< altezza header strip delle finestre di sistema.
    inline constexpr int cornerResizerSize = 18;   ///< dimensione standard dell'area di presa del corner resizer.
}

} // namespace ayra::tokens
