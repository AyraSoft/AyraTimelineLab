/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_NodeButtons.h
 * @brief Geometria dei BOTTONI-nodo (console attorno alla card) — fonte di
 *        verita' UNICA condivisa da `drawNodeCard` (disegno) e dal canvas
 *        (hit-test interattivo).
 *
 * Nel NodeComponent reale di ayra_performer i bottoni erano `ShapeButton` figli,
 * posizionati in `resized()`. Qui il canvas NON usa Component: i glyph sono
 * disegnati come geometria pura e i click sono risolti con hit-test a mano. Per
 * evitare divergenze tra "dove disegno il bottone" e "dove lo clicco", le
 * posizioni vivono SOLO qui (coordinate native 1:1 con `resized()`, scalate dal
 * solo zoom e traslate all'origine del footprint).
 *
 * @note `<JuceHeader.h>` e' incluso dal file root del modulo prima di questo.
 */

namespace ayra
{

//==============================================================================
/** Identita' di un bottone-nodo (per hit-test + dispatch azione). */
enum class NodeButton
{
    none,
    shutdown,    ///< on/off del nodo (power)            -> setNodeShutdown
    bypass,      ///< bypass DSP                          -> setNodeBypassed
    window,      ///< apri/chiudi editor del plugin       -> callback app
    latency,     ///< compensazione latenza (clock)       -> setNodeCompensated
    buffer,      ///< bufferizzazione (block resize)      -> setNodeBufferized
    oversample,  ///< oversampling                        -> setNodeOversampled
    settings,    ///< menu impostazioni nodo              -> callback app
    panic,       ///< MIDI panic                          -> callback app
    params,      ///< finestra mapping parametri (CV)     -> callback app
    allParams,   ///< menu Parameters (lista param vivi)  -> callback app
    remove,      ///< elimina nodo (cestino)              -> removeNode
    format       ///< logo formato (non interattivo: tooltip)
};

//==============================================================================
/** Rettangoli (in coord CANVAS) dei bottoni a posizione fissa di un nodo, dato
    il suo FOOTPRINT (gia' in px canvas) e lo zoom. `remove` NON e' qui: dipende
    dalla larghezza del titolo -> `nodeRemoveButtonRect`. */
struct NodeButtonLayout
{
    juce::Rectangle<float> shutdown, bypass, window, latency, buffer,
                           oversample, settings, panic, params, allParams, format;
};

/** Calcola i rettangoli dei bottoni a posizione fissa, 1:1 con `resized()` del
    NodeComponent reale (coordinate native, poi scalate da `scale` e traslate
    all'origine del footprint). */
[[nodiscard]] inline NodeButtonLayout computeNodeButtonLayout (juce::Rectangle<float> footprint, float scale) noexcept
{
    const float s  = juce::jmax (0.25f, scale);
    const float oX = footprint.getX(), oY = footprint.getY();
    const float nW = footprint.getWidth()  / s;   // footprint nativo (pre-zoom).
    const float nH = footprint.getHeight() / s;
    auto R = [&] (float x, float y, float w, float h)
    { return juce::Rectangle<float> (oX + x * s, oY + y * s, w * s, h * s); };

    NodeButtonLayout L;
    L.shutdown   = R (38, 38, 10, 10);
    L.bypass     = R (62, 33,  8,  8);
    L.window     = R (34,  2, 10, 10);
    L.latency    = R (14,  2, 10, 10);
    L.buffer     = R ( 0, 12, 10, 10);
    L.oversample = R ( 0, 26, 10, 10);
    // Cluster azioni in alto a dx, griglia 2x2 (1:1 con NodeComponent::resized):
    //   [allParams] [settings]
    //   [panic]     [params]
    L.allParams  = R (nW - 26, 11, 11, 11);
    L.settings   = R (nW - 12, 12, 10, 10);
    L.panic      = R (nW - 26, 26, 11, 11);
    L.params     = R (nW - 12, 26, 10, 10);
    L.format     = R (nW - 20, nH - 20, 12, 12);
    return L;
}

/** Rettangolo (coord CANVAS) del bottone REMOVE, accanto al titolo. Misura il
    titolo con la stessa ricetta di `drawNodeCard` -> hit-test e disegno coincidono
    sempre. */
[[nodiscard]] inline juce::Rectangle<float> nodeRemoveButtonRect (juce::Rectangle<float> footprint,
                                                                  float scale, const juce::String& title) noexcept
{
    const float s   = juce::jmax (0.25f, scale);
    const auto  box = footprint.reduced (20.0f * s);
    const juce::Font nameFont (ayra::tokens::type::nodeName().withHeight (ayra::tokens::type::kNodeNameSize * s));
    const float nameH = nameFont.getHeight();
    const auto  textBounds = box.reduced (box.getWidth() * 0.25f, box.getHeight() * 0.5f - nameH * 0.5f);

    juce::GlyphArrangement ga;
    ga.addFittedText (nameFont, title, textBounds.getX(), textBounds.getY(),
                      textBounds.getWidth(), nameH, juce::Justification::centred, 1);
    const int nameW = juce::jmax ((int) (40.0f * s), (int) ga.getBoundingBox (0, -1, true).getWidth());

    return { textBounds.getCentreX() + (float) nameW * 0.5f + 8.0f * s,
             textBounds.getY() - 8.0f * s, 8.0f * s, 8.0f * s };
}

//==============================================================================
/** Capacita' di un nodo che governano QUALI bottoni appaiono. Derivate da
    layout pin (audio/midi) + chrome (editor/IO) + documento (remove). */
struct NodeButtonFlags
{
    bool isInputNode  { false };
    bool isOutputNode { false };
    bool hasAudio     { true };
    bool hasMidi      { false };
    bool hasEditor    { false };
    bool hasParams    { false };          ///< bottone "Parameters" (allParams): ha parametri vivi o program change.
    bool canRemove    { true };
};

/** Predicato di visibilita' di un bottone — FONTE DI VERITA' UNICA condivisa da
    `drawNodeCard` (decide cosa disegnare) e dal canvas (decide cosa e'
    cliccabile). Se divergessero, un bottone potrebbe risultare cliccabile ma non
    disegnato (o viceversa): qui non puo' succedere. */
[[nodiscard]] inline bool isNodeButtonVisible (NodeButton b, const NodeButtonFlags& f) noexcept
{
    const bool isIO = f.isInputNode || f.isOutputNode;
    switch (b)
    {
        case NodeButton::shutdown:
        case NodeButton::settings:
        case NodeButton::params:
        case NodeButton::format:     return true;
        case NodeButton::bypass:     return ! isIO;
        case NodeButton::window:     return f.hasEditor;
        case NodeButton::latency:    return ! f.isInputNode;
        case NodeButton::buffer:
        case NodeButton::oversample: return ! isIO && f.hasAudio;
        case NodeButton::panic:      return f.hasMidi;
        case NodeButton::allParams:  return ! isIO && f.hasParams;
        case NodeButton::remove:     return f.canRemove;
        case NodeButton::none:
        default:                     return false;
    }
}

//==============================================================================
/** Colore FISSO dello stato ATTIVO (toggle-on) di un bottone-nodo, per tipologia.

    FONTE DI VERITA' UNICA del colore-per-tipo: la consumano il `NodeComponent`
    reale del canvas (`ShapeButton::setColours`), il `NodesInspectorComponent` e la
    ricetta `drawNodeCard` -> un "bypass" e' giallo OVUNQUE, un "buffer" viola
    ovunque, ecc. Per cambiare il colore di una tipologia si tocca SOLO qui.

    A riposo i flag non-power restano grigi (`Colours::lightgrey` lato
    ShapeButton, `grey` lato drawNodeCard). Il power e' l'eccezione semantica:
    operativo verde e shutdown rosso tramite `nodePowerColour`. I bottoni senza
    stato attivo (settings / panic / params / remove / format) usano il fallback
    neutro `chromeAlloy`. */
[[nodiscard]] inline juce::Colour nodeButtonActiveColour (NodeButton b) noexcept
{
    switch (b)
    {
        case NodeButton::shutdown:   return tokens::powerOffIndicator(); // rosso
        case NodeButton::bypass:     return tokens::node::param();   // giallo
        case NodeButton::window:     return tokens::node::audio();   // verde
        case NodeButton::latency:    return tokens::node::video();   // ciano
        case NodeButton::buffer:     return tokens::node::dmx();     // viola
        case NodeButton::oversample: return tokens::node::string();  // arancio
        case NodeButton::settings:
        case NodeButton::panic:
        case NodeButton::params:
        case NodeButton::allParams:
        case NodeButton::remove:
        case NodeButton::format:
        case NodeButton::none:
        default:                     return tokens::node::chromeAlloy();
    }
}

/** Colore del power di un nodo in base allo stato OPERATIVO, non al valore raw
    del flag `shutdown`. Centralizza l'inversione semantica: nel documento
    `shutdown=true` significa spento, mentre nell'interfaccia verde significa
    operativo. */
[[nodiscard]] inline juce::Colour nodePowerColour (bool isShutdown) noexcept
{
    return isShutdown ? tokens::powerOffIndicator()
                      : tokens::powerOnIndicator();
}

//==============================================================================
/** Glyph canonico di un bottone-nodo — FONTE DI VERITA' UNICA del glyph-per-tipo,
    gemella di `nodeButtonActiveColour`. La consumano il Node Inspector, il mixer
    (toolbar di flag del NODO) e chiunque disegni un flag-nodo: uno "shutdown" e'
    un power OVUNQUE, un "buffer" e' lo stack ovunque. Per cambiare il glyph di una
    tipologia si tocca SOLO qui. Fallback neutro `chip` per i tipi senza glyph. */
[[nodiscard]] inline const glyphs::Glyph& nodeButtonGlyph (NodeButton b) noexcept
{
    switch (b)
    {
        case NodeButton::shutdown:   return glyphs::shutdown;
        case NodeButton::bypass:     return glyphs::bypass;
        case NodeButton::window:     return glyphs::window;
        case NodeButton::latency:    return glyphs::clock;
        case NodeButton::buffer:     return glyphs::stack;
        case NodeButton::oversample: return glyphs::thunderCircle;
        case NodeButton::settings:   return glyphs::settings;
        case NodeButton::panic:      return glyphs::midiPanic;
        case NodeButton::params:
        case NodeButton::allParams:  return glyphs::sliders;
        case NodeButton::remove:     return glyphs::basket;
        case NodeButton::format:
        case NodeButton::none:
        default:                     return glyphs::chip;
    }
}

} // namespace ayra
