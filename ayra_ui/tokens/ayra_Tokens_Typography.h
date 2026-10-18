/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_Tokens_Typography.h
 * @brief Token di TIPOGRAFIA del design system di ayra_common.
 *
 * Due famiglie:
 *  - **Avenir Next** / **SF Mono** (Frosted Lab): chrome, inspector, form, value.
 *  - **Helvetica** (node-card): i 5 ruoli mappati 1:1 da ayra_performer
 *    `NodeFont` (nodeName/nodeID/bodyText/smallBodyText/detailText), tenuti su
 *    Helvetica per fedelta' visiva al NodeComponent reale.
 *
 * Tutti i font passano da `detail::make(...)`, che rende ESPLICITO e
 * centralizzato il fallback: se il typeface preferito non e' installato
 * (Linux/CI/headless render — "Helvetica" e "Avenir Next" spesso assenti),
 * JUCE sostituisce col default di sistema invece di lasciare FontOptions a mano
 * sparse nel codice. Niente piu' literal `juce::FontOptions("Helvetica", ...)`.
 *
 * @note Questo file NON va incluso direttamente: usa il facade `ayra_Tokens.h`.
 */

namespace ayra::tokens::type
{

namespace detail
{
    /** Costruisce un juce::Font dal typeface preferito. Fallback automatico di
        sistema se `family` non e' installato (vedi nota del file). Punto unico
        per la garanzia anti-typeface-mancante. */
    inline juce::Font make (const char* family, float height, const char* style) noexcept
    {
        return juce::Font (juce::FontOptions().withName (family).withHeight (height).withStyle (style));
    }
}

//==============================================================================
// MARK: - Frosted Lab (chrome / inspector / form)
//==============================================================================

/// Titolo (Avenir Next DemiBold 17) — header di pannello, nome traccia.
inline juce::Font title()   { return detail::make ("Avenir Next", 17.0f,  "Demi Bold"); }
/// Sezione (Avenir Next Bold 10.5, uppercase nei chiamanti) — header di gruppo.
inline juce::Font section() { return detail::make ("Avenir Next", 10.5f, "Bold"); }
/// Corpo (Avenir Next Medium 13) — label, testo generico.
inline juce::Font body()    { return detail::make ("Avenir Next", 13.0f, "Medium"); }
/// Caption (Avenir Next Regular 11) — sottotitoli, hint.
inline juce::Font caption() { return detail::make ("Avenir Next", 11.0f, "Regular"); }
/// Valore numerico (SF Mono Medium 12.5) — tabular, "Bitwig feel".
inline juce::Font value()   { return detail::make ("SF Mono",     12.5f, "Medium"); }

//==============================================================================
// MARK: - Node-card (mappati da NodeFont di ayra_performer, family Helvetica)
//
// Size BASE come costanti: i chiamanti che scalano con il fattore di zoom usano
// p.es. `type::nodeName().withHeight (type::kNodeNameSize * s)` senza ri-hardcoddare.
//==============================================================================

inline constexpr float kNodeNameSize   = 14.0f;   ///< nodeName  (Bold)
inline constexpr float kNodeIdSize      = 5.0f;    ///< nodeID    (Bold)
inline constexpr float kNodeBodySize    = 12.0f;   ///< bodyText  (Regular)
inline constexpr float kNodeSmallSize   = 10.0f;   ///< smallBody (Regular)
inline constexpr float kNodeDetailSize  = 8.0f;    ///< detail    (Regular)

inline juce::Font nodeName()   { return detail::make ("Helvetica", kNodeNameSize,   "Bold"); }
inline juce::Font nodeId()     { return detail::make ("Helvetica", kNodeIdSize,     "Bold"); }
inline juce::Font nodeBody()   { return detail::make ("Helvetica", kNodeBodySize,   "Regular"); }
inline juce::Font nodeSmall()  { return detail::make ("Helvetica", kNodeSmallSize,  "Regular"); }
inline juce::Font nodeDetail() { return detail::make ("Helvetica", kNodeDetailSize, "Regular"); }

} // namespace ayra::tokens::type
