/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / legacy / NodeFont
   -------------------------------------------------------
   VENDOR as-is dei font del node graph, importati 1:1 da
   `ayra_performer/headers/ayra_Fonts.h` per preservare i pixel del canvas
   legacy durante il port a `ayra::GraphProcessor` (Tensor M1).

   Namespace `NodeFont` (globale) come nell'originale. Zero-include: `juce::Font`
   / `juce::FontOptions` gia' visibili via la catena di inclusione di ayra_common.h.

  ==============================================================================
*/

#pragma once

//==============================================================================
/**
 * @namespace NodeFont
 * @brief Contains font definitions for the node graph UI
 */
namespace NodeFont
{
    /** @name Font Styles */
    ///@{
    inline static juce::Font nodeName()       { return juce::Font(juce::FontOptions("Helvetica", "Bold", 14.0f)); }
    inline static juce::Font nodeID()         { return juce::Font(juce::FontOptions("Helvetica", "Bold", 5.0f)); }
    inline static juce::Font bodyText()       { return juce::Font(juce::FontOptions("Helvetica", "Regular", 12.0f)); }
    inline static juce::Font smallBodyText()  { return juce::Font(juce::FontOptions("Helvetica", "Regular", 10.0f)); }
    inline static juce::Font detailText()     { return juce::Font(juce::FontOptions("Helvetica", "Regular", 8.0f)); }
    ///@}
}
