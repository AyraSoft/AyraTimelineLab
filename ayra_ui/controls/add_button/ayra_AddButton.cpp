/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / AddButton (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui; dipendenze visibili da ayra_common.h.

  ==============================================================================
*/

namespace ayra
{

AddButton::AddButton (juce::String tooltipText)
    : ShapeIconButton ("Add")
{
    setGlyph (glyphs::plus);
    setIconPadding (0.28f);
    setDrawBackground (true);
    setShowIdleBackground (true);
    setUseThemeAccentAtRest (true);
    setClickingTogglesState (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    if (tooltipText.isNotEmpty()) { setTooltip (tooltipText); }
}

} // namespace ayra
