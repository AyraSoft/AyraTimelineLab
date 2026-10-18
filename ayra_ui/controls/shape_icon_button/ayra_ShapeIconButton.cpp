/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / ShapeIconButton (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (token gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

namespace
{
juce::Colour shapeButtonFallback (int colourId, juce::Colour accent,
                                  juce::Colour accentSoft, bool accentAtRest)
{
    switch (colourId)
    {
        case ShapeIconButton::iconColourId:                  return accentAtRest ? accent : tokens::textSecondary();
        case ShapeIconButton::iconOverColourId:              return accentAtRest ? accent.brighter (0.12f) : tokens::textPrimary();
        case ShapeIconButton::iconDownColourId:              return accent;
        case ShapeIconButton::iconOnColourId:                return accent;
        case ShapeIconButton::backgroundColourId:            return tokens::surfaceRaised();
        case ShapeIconButton::backgroundOnColourId:          return accentSoft;
        case ShapeIconButton::backgroundIdleColourId:        return accentAtRest ? accentSoft : tokens::surfaceRaised();
        case ShapeIconButton::backgroundIdleOutlineColourId: return tokens::borderHairline();
        default:                                             return tokens::textSecondary();
    }
}

} // namespace

//==============================================================================
ShapeIconButton::ShapeIconButton (const juce::String& name)
    : juce::Button (name)
{
}

//==============================================================================
void ShapeIconButton::setShape (juce::Path newShape)
{
    shape = std::move (newShape);
    repaint();
}

void ShapeIconButton::setGlyph (const ayra::glyphs::Glyph& glyph)
{
    setShape (glyph.path());
}

void ShapeIconButton::setIconPadding (float fractionOfMinSide) noexcept
{
    const float next = juce::jlimit (0.0f, 0.49f, fractionOfMinSide);
    if (juce::approximatelyEqual (iconPadding, next)) { return; }
    iconPadding = next;
    repaint();
}

void ShapeIconButton::setDrawBackground (bool shouldDraw) noexcept
{
    if (drawBackground == shouldDraw) { return; }
    drawBackground = shouldDraw;
    repaint();
}

void ShapeIconButton::setShowIdleBackground (bool shouldShow) noexcept
{
    if (showIdleBackground == shouldShow) { return; }
    showIdleBackground = shouldShow;
    repaint();
}

void ShapeIconButton::setUseThemeAccentAtRest (bool shouldUse) noexcept
{
    if (useThemeAccentAtRest == shouldUse) { return; }
    useThemeAccentAtRest = shouldUse;
    repaint();
}

//==============================================================================
void ShapeIconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto bounds  = getLocalBounds().toFloat();
    const bool on      = getToggleState();
    const bool enabled = isEnabled();
    auto& commonLaf = getCommonLAF (*this);
    const auto accent = commonLaf.accentOr (tokens::accent());
    const auto accentSoft = commonLaf.accentSoftOr (tokens::accentSoft());
    const auto fallbackColour = [accent, accentSoft, accentAtRest = useThemeAccentAtRest] (int colourId)
    {
        return shapeButtonFallback (colourId, accent, accentSoft, accentAtRest);
    };

    // --- Chip di sfondo (proprio, NON delegato al LookAndFeel) -----------------
    // Storia del bug: prima delegavamo a `getLookAndFeel().drawButtonBackground`,
    // che pero' IGNORA il colore passato e, su toggle-ON, dipinge una CTA ambra
    // PIENA. Su un bottone quadrato il suo rounded-rect a corner pieno diventava
    // un "cerchio" ambra, e l'icona ON (anch'essa ambra) ci spariva sopra ->
    // "il disegno non si distingue piu', resta un cerchio dopo il click".
    // Qui il chip usa i ColourIds del widget (soft tint), che i caller possono
    // personalizzare per bottone. L'icona saturata sopra resta sempre leggibile:
    // "soft is the bed, crisp is the signal".
    //
    // A riposo (off, no hover) NIENTE chip: solo l'icona nuda -> footer pulito,
    // stile transport di Logic/Live. Il chip compare solo su hover/down/ON.
    // ECCEZIONE: con `showIdleBackground` il chip a riposo c'e' SEMPRE (superficie
    // raised piena + rim), cosi' una toolbar di tasti (es. flag dello Snapshot
    // Editor) si legge come tale gia' a riposo invece di apparire solo dopo il click.
    const bool reactive = (on || over || down);
    if (drawBackground && enabled && (reactive || showIdleBackground))
    {
        const auto  chip   = bounds.reduced (1.0f);
        const float corner = juce::jmin (chip.getWidth(), chip.getHeight()) * 0.30f;

        juce::Colour bg = on       ? tokens::componentColour (*this, backgroundOnColourId, fallbackColour (backgroundOnColourId))
                        : reactive ? tokens::componentColour (*this, backgroundColourId, fallbackColour (backgroundColourId))
                                   : tokens::componentColour (*this, backgroundIdleColourId, fallbackColour (backgroundIdleColourId));
        if (down) { bg = bg.brighter (0.08f); }   // press feedback, stessa tinta semantica

        juce::Path chipPath;
        chipPath.addRoundedRectangle (chip, corner);
        g.setColour (bg);
        g.fillPath (chipPath);

        if (on && ! over && ! down)   // In hover resta solo il background.
        {
            g.setColour (tokens::componentColour (*this, iconOnColourId, fallbackColour (iconOnColourId)).withAlpha (0.60f));
            g.strokePath (chipPath, juce::PathStrokeType (1.0f));
        }
        else if (showIdleBackground && ! reactive)   // rim tenue: definisce il bordo del "tasto" a riposo
        {
            g.setColour (tokens::componentColour (*this, backgroundIdleOutlineColourId, fallbackColour (backgroundIdleOutlineColourId)));
            g.strokePath (chipPath, juce::PathStrokeType (1.0f));
        }
    }

    if (shape.isEmpty()) { return; }

    // Un toggle ON conserva la propria tinta anche durante la pressione: il
    // feedback viene dalla luminosita' del chip, senza mostrare per un frame il
    // colore dello stato opposto.
    juce::Colour iconColour;
    if      (! enabled) { iconColour = tokens::componentColour (*this, iconColourId, fallbackColour (iconColourId)).withMultipliedAlpha (0.4f); }
    else if (on)        { iconColour = tokens::componentColour (*this, iconOnColourId, fallbackColour (iconOnColourId)); }
    else if (down)      { iconColour = tokens::componentColour (*this, iconDownColourId, fallbackColour (iconDownColourId)); }
    else if (over)      { iconColour = tokens::componentColour (*this, iconOverColourId, fallbackColour (iconOverColourId)); }
    else                { iconColour = tokens::componentColour (*this, iconColourId, fallbackColour (iconColourId)); }

    const float pad      = juce::jmin (bounds.getWidth(), bounds.getHeight()) * iconPadding;
    const auto  iconArea = bounds.reduced (pad);

    juce::Path p = shape;
    p.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
    g.setColour (iconColour);
    g.fillPath (p);
}

//==============================================================================
// Toolbar di flag-toggle (helper condivisi) — vedi header.
//==============================================================================
void configureFlagToggle (ShapeIconButton& button, const glyphs::Glyph& glyph, juce::Colour onColour)
{
    button.setGlyph (glyph);
    button.setColour (ShapeIconButton::iconOnColourId,       onColour);
    button.setColour (ShapeIconButton::backgroundOnColourId, onColour.withAlpha (0.22f));
}

void configurePowerToggle (ShapeIconButton& button, PowerTogglePolarity polarity)
{
    const bool onMeansOperational = polarity == PowerTogglePolarity::onMeansOperational;
    const auto idleColour = onMeansOperational ? tokens::powerOffIndicator()
                                               : tokens::powerOnIndicator();
    const auto onColour   = onMeansOperational ? tokens::powerOnIndicator()
                                               : tokens::powerOffIndicator();

    button.setGlyph (glyphs::shutdown);
    button.setDrawBackground (true);
    button.setShowIdleBackground (true);
    button.setColour (ShapeIconButton::iconColourId,           idleColour);
    button.setColour (ShapeIconButton::iconOverColourId,       idleColour.brighter (0.12f));
    button.setColour (ShapeIconButton::iconDownColourId,       idleColour.brighter (0.12f));
    button.setColour (ShapeIconButton::iconOnColourId,         onColour);
    button.setColour (ShapeIconButton::backgroundColourId,     tokens::powerBackground (idleColour));
    button.setColour (ShapeIconButton::backgroundIdleColourId, tokens::powerBackground (idleColour));
    button.setColour (ShapeIconButton::backgroundIdleOutlineColourId,
                      tokens::powerOutline (idleColour));
    button.setColour (ShapeIconButton::backgroundOnColourId,   tokens::powerBackground (onColour));
}

void configureUnavailablePowerToggle (ShapeIconButton& button)
{
    const auto neutral = tokens::powerUnavailableIndicator();
    button.setGlyph (glyphs::shutdown);
    button.setDrawBackground (false);
    button.setShowIdleBackground (false);
    button.setColour (ShapeIconButton::iconColourId,     neutral);
    button.setColour (ShapeIconButton::iconOverColourId, neutral);
    button.setColour (ShapeIconButton::iconDownColourId, neutral);
    button.setColour (ShapeIconButton::iconOnColourId,   neutral);
}

void configureNodeFlagToggle (ShapeIconButton& button, NodeButton kind)
{
    if (kind == NodeButton::shutdown)
        configurePowerToggle (button, PowerTogglePolarity::onMeansShutdown);
    else
        configureFlagToggle (button, nodeButtonGlyph (kind), nodeButtonActiveColour (kind));
}

void drawFlagToolbarWell (juce::Graphics& g, juce::Rectangle<float> well)
{
    if (well.isEmpty()) { return; }
    fillRecessedSurface (g, well, 7.0f);
}

void layoutFlagToolbar (juce::Rectangle<int> well, const juce::Array<ShapeIconButton*>& buttons)
{
    const int n = buttons.size();
    if (n <= 0) { return; }

    auto strip = well.reduced (3, 1);
    const int slot = juce::jmax (1, strip.getWidth() / n);
    for (auto* b : buttons)
    {
        auto cell = strip.removeFromLeft (slot);
        const int w = juce::jmin (cell.getWidth(), cell.getHeight() + 10);
        b->setBounds (cell.withSizeKeepingCentre (w, cell.getHeight()));
    }
}

} // namespace ayra
