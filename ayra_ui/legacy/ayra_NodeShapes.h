/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / legacy / NodeShapes
   -------------------------------------------------------
   Resta solo l'helper `assignShapeToButton`, ora sull'interfaccia UNICA `Glyph`.

   I path-data byte-array che vivevano qui (shutdown/bypass/clock/window/basket/
   logo/settings/stack/thunderCircle/midiPanic/sliders) erano DUPLICATI dei
   corrispondenti `ayra::glyphs::*Path`: sono stati rimossi. Usare i Glyph del
   registry (`ayra::glyphs::shutdown`, `...bypass`, `...sliders`, ...).

   Zero-include (dottrina moduli Ayra): gli #include stanno solo in ayra_common.h.
   `ShapeButton`/`juce::Path`/`glyphs::Glyph` sono gia' visibili a cascata.

  ==============================================================================
*/

#pragma once

namespace ayra
{

/** Carica il path del `Glyph` (SVG o binario, via la sua vista `path()` cache-ata)
    in un `juce::ShapeButton`, con fit proporzionale. Interfaccia unica: niente piu'
    overload raw `(const unsigned char*, size_t)`. */
inline void assignShapeToButton (const glyphs::Glyph& glyph, juce::ShapeButton& button)
{
    button.setShape (glyph.path(), true, true, false);
}

} // namespace ayra
