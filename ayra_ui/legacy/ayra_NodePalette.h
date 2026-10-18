/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / legacy / NodePalette
   -------------------------------------------------------
   VENDOR as-is della palette colori del node graph, importata 1:1 da
   `ayra_performer/headers/ayra_ColorPalette.h` per preservare i pixel del
   canvas legacy durante il port a `ayra::GraphProcessor` (Tensor M1).

   Namespace `NodePalette` (globale) come nell'originale: il canvas vi accede
   come `NodePalette::pinAudio()` ecc. Zero-include (dottrina moduli Ayra):
   `juce::Colour` e' gia' visibile via la catena di inclusione di ayra_common.h.

  ==============================================================================
*/

#pragma once

//==============================================================================
/**
 * @namespace NodePalette
 * @brief Contains color definitions for the node graph UI
 */
namespace NodePalette
{
    //==============================================================================
    /** @name Base Colors */
    ///@{
    inline static juce::Colour outerVoid()       { return juce::Colour::fromRGB(12, 12, 12); }     // Deep black
    inline static juce::Colour innerAbyss()      { return juce::Colour::fromRGB(26, 36, 36); }     // Cool dark green
    inline static juce::Colour neonSurge()       { return juce::Colour::fromRGB(70, 255, 220); }   // Bright turquoise
    inline static juce::Colour quantumFlare()    { return juce::Colour::fromRGB(255, 20, 60); }    // Vivid orange
    inline static juce::Colour plasmaGlow()      { return juce::Colour::fromRGB(255, 102, 102); }  // Bright red
    inline static juce::Colour quantumPurple()   { return juce::Colour::fromRGB(130, 50, 160); }   // Deep purple
    inline static juce::Colour chromeAlloy()     { return juce::Colour::fromRGB(150, 180, 190); }  // Blue-gray
    inline static juce::Colour shadowReactor()   { return juce::Colour::fromRGB(18, 24, 24); }     // Very dark gray
    ///@}

    //==============================================================================
    /** @name Node Colors */
    ///@{
    inline static juce::Colour nodeColour()      { return juce::Colour::fromRGB(100, 150, 160); }  // Soft blue-gray
    ///@}

    //==============================================================================
    /** @name Pin Colors */
    ///@{
    inline static juce::Colour pinAudio()        { return juce::Colour::fromRGB(0, 255, 100); }    // Bright green
    inline static juce::Colour pinMidi()         { return juce::Colour::fromRGB(255, 60, 60); }    // Saturated red
    inline static juce::Colour pinVideo()        { return juce::Colour::fromRGB(0, 200, 255); }    // Deep cyan
    inline static juce::Colour pinParameter()    { return juce::Colour::fromRGB(255, 220, 0); }    // Warm yellow
    inline static juce::Colour pinDmx()          { return juce::Colour::fromRGB(180, 80, 220); }   // Vibrant purple
    inline static juce::Colour pinOsc()          { return juce::Colour::fromRGB(70, 100, 255); }   // Bright blue
    inline static juce::Colour pinString()       { return juce::Colour::fromRGB(255, 140, 0); }    // Balanced medium gray
    ///@}

    //==============================================================================
    /** @name DefaultNode Colors */
    ///@{
    inline static juce::Colour instrumentNode()     { return juce::Colour::fromRGB(0, 220, 200); }    // Bright cyan
    inline static juce::Colour audioEffectNode()    { return juce::Colour::fromRGB(0, 255, 100); }    // Bright green
    inline static juce::Colour midiEffectNode()     { return juce::Colour::fromRGB(255, 60, 60); }    // Saturated red
    inline static juce::Colour dmxEffectNode()      { return juce::Colour::fromRGB(180, 80, 220); }   // Vibrant purple
    inline static juce::Colour oscEffectNode()      { return juce::Colour::fromRGB(70, 100, 255); }   // Bright blue
    inline static juce::Colour paramEffectNode()    { return juce::Colour::fromRGB(255, 220, 0); }    // Warm yellow
    inline static juce::Colour stringEffectNode()   { return juce::Colour::fromRGB(255, 140, 0); }    // Balanced medium gray
    inline static juce::Colour imageEffectNode()    { return juce::Colour::fromRGB(0, 200, 255); }    // Deep cyan
    inline static juce::Colour utilityNode()        { return juce::Colour::fromRGB(255, 255, 255); }  // Deep white
    ///@}

    //==============================================================================
    /** @name Text Colors */
    ///@{
    inline static juce::Colour stringColour()    { return juce::Colour::fromRGB(255, 255, 255); }  // White
    ///@}
}
