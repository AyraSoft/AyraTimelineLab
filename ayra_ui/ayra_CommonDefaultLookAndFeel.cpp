/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

/**
 * @file ayra_CommonDefaultLookAndFeel.cpp
 * @brief Fornisce SOLO il Meyer's singleton `getDefaultInstance()` di
 *        CommonDefaultLookAndFeel.
 *
 * # CONVENZIONE INCLUDE (moduli Ayra)
 * Questo file e i 5 partial dell'implementazione di default sono inclusi dal root
 * `ayra_common.cpp` (NON l'uno dall'altro): vedi `ayra_common.cpp` righe 63-68
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel.cpp"     (questo file)
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel_Chrome.cpp"
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel_NodeGraph.cpp"
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel_Mixer.cpp"
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel_Inspector.cpp"
 *   #include "look_and_feel/ayra_CommonDefaultLookAndFeel_Form.cpp"
 * I partial NON hanno include propri: vedono tutti i simboli (JUCE, token, shadow
 * cache, interfaccia) gia' risolti transitivamente via `ayra_common.h`.
 *
 * Le ricette bespoke sono ripartite per leggibilita' (< ~600 righe ciascuno) nei
 * 5 partial:
 *   - Chrome    : fondo app, pannelli, glass strip, header, chip, logo.
 *   - NodeGraph : node card, pin, cable (Dark Cosmic).
 *   - Mixer     : channel strip, colour cap, fader, VU, macro knob.
 *   - Inspector : clip cell, transport, CPU arc, snapshot, node list, curve.
 *   - Form      : switch, badge, search field, param slider, plugin card.
 */

namespace ayra
{

//==============================================================================
CommonDefaultLookAndFeel& CommonDefaultLookAndFeel::getDefaultInstance()
{
    static CommonDefaultLookAndFeel instance;
    return instance;
}

} // namespace ayra

