/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / fader_law
   --------------------------------------------
   Legge del fader dei pannelli mixer-like (MixerPanel, CuesPanel). Promossa dal
   file-scope di MixerPanel.cpp quando il pannello Cues ha avuto bisogno della
   STESSA legge: un'unica fonte di verita' evita che due pannelli mostrino dB
   diversi per la stessa posizione.

   NOTA amalgama: zero #include (dottrina moduli Ayra) - juce::* e' gia'
   visibile via ayra_common.h.

  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Compatibilita' nomi app-side: la legge e' posseduta dal provider Gain e viene
// consumata sia dall'editor del nodo sia dai pannelli mixer-like.
//==============================================================================
inline constexpr float kFaderUnityPos   = GainProcessor::GainContract::faderUnityProportion;
inline constexpr float kFaderMinDb      = GainProcessor::GainContract::faderMinimumDb;
inline constexpr float kFaderMaxDb      = GainProcessor::GainContract::authoringMaximumDb (false);
inline constexpr float kFaderSilencePos = GainProcessor::GainContract::faderSilenceProportion;
inline constexpr float kSilenceDb       = GainProcessor::GainContract::silenceDb;

[[nodiscard]] inline float faderPosToDb (float pos, float maximumDb) noexcept
{
    return GainProcessor::GainContract::faderProportionToDecibels (pos, maximumDb);
}

[[nodiscard]] inline float faderDbToPos (float db, float maximumDb) noexcept
{
    return GainProcessor::GainContract::decibelsToFaderProportion (db, maximumDb);
}

/** Varianti standard: la preferenza +48 modifica soltanto la legge di vista. */
[[nodiscard]] inline float faderPosToDb (float pos) noexcept
{ return faderPosToDb (pos, kFaderMaxDb); }
[[nodiscard]] inline float faderDbToPos (float db) noexcept
{ return faderDbToPos (db, kFaderMaxDb); }

/** Il parser dB e' quello del provider Gain: nessuna seconda grammatica locale. */
[[nodiscard]] inline bool parseDbText (const juce::String& raw, float& outDb)
{
    if (const auto value = GainProcessor::parseGainDecibels (raw))
    { outDb = *value; return true; }
    return false;
}

} // namespace ayra
