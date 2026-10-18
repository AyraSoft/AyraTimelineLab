/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / GlyphHandles (REGISTRY UNICO)
   ------------------------------------------------------------------
   Registro UNICO dei glyph PUBBLICI del design system: per ogni glyph un
   `ayra::glyphs::Glyph` con nome pulito (niente suffisso `Svg`/`Path`). E'
   l'unica interfaccia che il resto del codebase usa; i simboli grezzi
   (`*Svg` stringhe, `*Path` byte-array) restano nei rispettivi file SOLO come
   dato di backing del Glyph corrispondente.

   Incluso DOPO tutti i file-dato dei glyph (SVG di Device/Snapshot/Command
   compresi), cosi' ogni simbolo sorgente referenziato qui e' gia' visibile.

   Aggiungere un glyph nuovo = 2 righe: il dato grezzo nel file di categoria +
   una riga `inline constexpr Glyph <nome> { <dato> };` qui. Niente API parallele.

   Doctrine: ZERO #include, namespace ayra::glyphs, ASCII-only.

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

//==============================================================================
// SVG-backed (forma sorgente: stringa SVG `d`)
//==============================================================================
inline constexpr Glyph sum                { sumSvg };
inline constexpr Glyph audio              { audioSvg };
inline constexpr Glyph camera             { cameraSvg };
inline constexpr Glyph close              { closeSvg };
inline constexpr Glyph cog                { cogSvg };
inline constexpr Glyph trash              { deleteSvg };          // "delete" e' keyword: nome handle = trash
inline constexpr Glyph deviceManager      { deviceManagerSvg };
inline constexpr Glyph dmx                { dmxSvg };
inline constexpr Glyph exportAll          { exportAllSvg };
inline constexpr Glyph feedbackLoop       { feedbackLoopSvg };
inline constexpr Glyph importAll          { importAllSvg };
inline constexpr Glyph keyboard           { keyboardSvg };
inline constexpr Glyph midi               { midiSvg };
inline constexpr Glyph osc                { oscSvg };
inline constexpr Glyph plus               { plusSvg };
inline constexpr Glyph profiler           { profilerSvg };
inline constexpr Glyph recalibrate        { recalibrateSvg };
inline constexpr Glyph warning            { warningSvg };
inline constexpr Glyph snapConnections    { snapConnectionsSvg };
inline constexpr Glyph snapEditorsPosition{ snapEditorsPositionSvg };
inline constexpr Glyph snapLaunch         { snapLaunchSvg };
inline constexpr Glyph snapLookahead      { snapLookaheadSvg };
inline constexpr Glyph snapPositions      { snapPositionsSvg };
inline constexpr Glyph snapTake           { snapTakeSvg };
inline constexpr Glyph snapTransition     { snapTransitionSvg };
inline constexpr Glyph snapVisibility     { snapVisibilitySvg };

//==============================================================================
// Binary-backed (forma sorgente: path-data JUCE `*Path` byte-array)
//==============================================================================
inline constexpr Glyph auFormatLogo       { auFormatLogoPath };
inline constexpr Glyph ayraLogo           { ayraLogoPath };
inline constexpr Glyph basket             { basketPath };
inline constexpr Glyph bypass             { bypassPath };
inline constexpr Glyph chip               { chipPath };
inline constexpr Glyph clock              { clockPath };
inline constexpr Glyph metronome          { metronomeSvg };
inline constexpr Glyph counter            { counterSvg };
inline constexpr Glyph play               { playSvg };
inline constexpr Glyph rewind             { rewindSvg };
inline constexpr Glyph stop               { stopSvg };
inline constexpr Glyph juceLogo           { juceLogoPath };
inline constexpr Glyph lv2FormatLogo      { lv2FormatLogoPath };
inline constexpr Glyph midiPanic          { midiPanicPath };
inline constexpr Glyph randomize          { randomizePath };
inline constexpr Glyph settings           { settingsPath };
inline constexpr Glyph shutdown           { shutdownPath };
inline constexpr Glyph sliders            { slidersPath };
inline constexpr Glyph stack              { stackPath };
inline constexpr Glyph thunderCircle      { thunderCirclePath };
inline constexpr Glyph vst2FormatLogo     { vst2FormatLogoPath };
inline constexpr Glyph vst3FormatLogo     { vst3FormatLogoPath };
inline constexpr Glyph window             { windowPath };

//==============================================================================
// Semantic selectors — one mapping from domain identity to design-system glyph.
//==============================================================================

/** Canonical plugin-format logo used everywhere the app needs a compact format
    identity. Keep aliases here so Nodes Inspector, External Map and future
    surfaces cannot drift into parallel VST/AU/LV2 switch statements. */
[[nodiscard]] inline const Glyph& pluginFormatLogo (const juce::String& format) noexcept
{
    if (format.containsIgnoreCase ("VST3")) { return vst3FormatLogo; }
    if (format.containsIgnoreCase ("VST"))  { return vst2FormatLogo; }
    if (format.containsIgnoreCase ("AudioUnit")
        || format.equalsIgnoreCase ("AU")
        || format.containsIgnoreCase ("AUv3"))
        return auFormatLogo;
    if (format.containsIgnoreCase ("LV2"))  { return lv2FormatLogo; }
    return ayraLogo;
}

} // namespace ayra::glyphs
