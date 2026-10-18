/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / CommandGlyphs
   ------------------------------------------------------------------
   Glyph del KEY COMMANDS editor (header). Stesso meccanismo di
   `ayra_DeviceGlyphs.h` / `ayra_SnapshotGlyphs.h`: stringa SVG `path d="..."`
   REALE di un icon-set, convertita a runtime in `juce::Path` -> blob binario
   compatibile con `drawAppWindowHeader` / `loadPathFromData`, via il facade
   `ayra::glyphs::svgPathBlob(...)` (definito in DeviceGlyphs, incluso prima).

   Le azioni Import/Export RIUSANO `importAllSvg` / `exportAllSvg` di
   DeviceGlyphs (stessa semantica di freccia-nel-box), quindi non sono
   ridefinite qui.

   ## Provenienza icona (NON inventata)
   Material Design Icons (Pictogrammers), licenza Apache-2.0, viewBox 0 0 24 24,
   single-path fill. Nome originale:
     keyboard <- "keyboard"   (tastiera = scorciatoie da tastiera)

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

/// Header "Key Commands": tastiera. MDI "keyboard".
inline constexpr const char* keyboardSvg =
    "M19,10H17V8H19M19,13H17V11H19M16,10H14V8H16M16,13H14V11H16M16,17H8V15H16M7,10H5V8H7"
    "M7,13H5V11H7M8,11H10V13H8M8,8H10V10H8M11,11H13V13H11M11,8H13V10H11M20,5H4"
    "C2.89,5 2,5.89 2,7V17A2,2 0 0,0 4,19H20A2,2 0 0,0 22,17V7C22,5.89 21.1,5 20,5Z";

} // namespace ayra::glyphs
