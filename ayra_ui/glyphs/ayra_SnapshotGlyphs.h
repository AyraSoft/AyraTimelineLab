/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / SnapshotGlyphs
   ------------------------------------------------------------------
   Glyph dello SNAPSHOT EDITOR (launch/take + i flag "load X" della riga). Stesso
   meccanismo di `ayra_DeviceGlyphs.h`: stringhe SVG `path d="..."` REALI di un
   icon-set, convertite a runtime in `juce::Path` -> blob binario compatibile con
   `loadPathFromData` / `ShapeIconButton::setGlyph`, via il facade
   `ayra::glyphs::svgPathBlob(...)` (definito in DeviceGlyphs, incluso prima).

   ## Perche' SVG e non blob binari
   Identico razionale di DeviceGlyphs: i blob binari si autorano solo eseguendo
   JUCE offline; le stringhe SVG sono editabili a mano e provengono da icon-set
   reali (nessuna icona "inventata"). La conversione avviene UNA volta per glyph
   (cache di `svgPathBlob`), message-thread only.

   ## Provenienza icone (NON inventate)
   Material Icons (Google), licenza Apache-2.0, viewBox 0 0 24 24, single-path
   fill solido. Nomi originali:
     launch              <- "play_arrow"            (attiva/recall snapshot)
     take                <- "photo_camera"          (cattura snapshot)
     loadConnections     <- "link"
     loadVisibility      <- "visibility"            (occhio)
     loadPositions       <- "open_with"             (4 frecce = sposta)
     loadEditorsPosition <- "picture_in_picture"    (finestra nella finestra)
     lookahead           <- "fast_forward"          (anticipo temporale)
     transition          <- "swap_horiz"            (morph A->B)

   I flag che hanno gia' un glyph "nativo" del NodeGraph (shutdown/bypass/
   parameters/editors) RIUSANO quelli (`ayra::glyphs::shutdown`, `...bypass`,
   `...sliders`, `...window`) per coerenza visiva col Node Inspector, quindi NON
   sono ridefiniti qui.

   @note JUCE arriva dal root del modulo (ayra_common.h) prima di questo file:
         nessun #include qui (regola moduli Ayra).

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

//==============================================================================
// SVG path-data (attributo `d`) — viewBox 0 0 24 24, fill solido.
//==============================================================================

/// Launch / recall snapshot: triangolo play. MDI "play_arrow".
inline constexpr const char* snapLaunchSvg =
    "M8 5v14l11-7z";

/// Take / cattura snapshot: fotocamera (corpo + anello obiettivo). MDI "photo_camera".
inline constexpr const char* snapTakeSvg =
    "M9 2 7.17 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V6"
    "c0-1.1-.9-2-2-2h-3.17L15 2H9zm3 15c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5z";

/// loadConnections: catena/link. MDI "link".
inline constexpr const char* snapConnectionsSvg =
    "M3.9 12c0-1.71 1.39-3.1 3.1-3.1h4V7H7c-2.76 0-5 2.24-5 5s2.24 5 5 5h4v-1.9H7"
    "c-1.71 0-3.1-1.39-3.1-3.1zM8 13h8v-2H8v2zm9-6h-4v1.9h4c1.71 0 3.1 1.39 3.1 3.1"
    "s-1.39 3.1-3.1 3.1h-4V17h4c2.76 0 5-2.24 5-5s-2.24-5-5-5z";

/// loadVisibility: occhio. MDI "visibility".
inline constexpr const char* snapVisibilitySvg =
    "M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5"
    "c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5z"
    "m0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z";

/// loadPositions: 4 frecce (sposta). MDI "open_with".
inline constexpr const char* snapPositionsSvg =
    "M10 9h4V6h3l-5-5-5 5h3v3zm-1 1H6V7l-5 5 5 5v-3h3v-4zm14 2-5-5v3h-3v4h3v3l5-5z"
    "m-9 3h-4v3H7l5 5 5-5h-3v-3z";

/// loadEditorsPosition: finestra nella finestra. MDI "picture_in_picture".
inline constexpr const char* snapEditorsPositionSvg =
    "M19 7h-8v6h8V7zm2-4H3c-1.1 0-2 .9-2 2v14c0 1.1.9 1.98 2 1.98h18c1.1 0 2-.88 2-1.98V5"
    "c0-1.1-.9-2-2-2zm0 16.01H3V4.98h18v14.03z";

/// lookahead: doppia freccia di avanzamento. MDI "fast_forward".
inline constexpr const char* snapLookaheadSvg =
    "M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z";

/// transition: scambio orizzontale (morph A->B). MDI "swap_horiz".
inline constexpr const char* snapTransitionSvg =
    "M6.99 11 3 15l3.99 4v-3H14v-2H6.99v-3zM21 9l-3.99-4v3H10v2h7.01v3L21 9z";

} // namespace ayra::glyphs
