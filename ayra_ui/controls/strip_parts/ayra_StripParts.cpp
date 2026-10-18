/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   Impl delle primitive di disegno condivise delle channel strip.
   NOTA amalgama: zero #include (dottrina moduli Ayra).

  ==============================================================================
*/

namespace ayra
{

void drawPanKnob (juce::Graphics& g, juce::Rectangle<float> area,
                  float panValue, const bool highlighted, const bool dragging,
                  const juce::String& readout, const juce::String& startLabel,
                  const juce::String& endLabel)
{
    const auto kb = StripKnobGeometry::bodyBounds (area);
    auto labelRow = StripKnobGeometry::readoutBounds (area);

    const float aMin = juce::degreesToRadians (-135.0f);   // full L
    const float aMax = juce::degreesToRadians (135.0f);    // full R (0 = centro, ore 12)
    const float pan  = juce::jlimit (-1.0f, 1.0f, panValue);
    const bool  centered = std::abs (pan) <= StripPanGeometry::centreSnap;

    RotaryKnobStyle knobStyle;
    knobStyle.valueColour = tokens::accent();
    knobStyle.glowColour = tokens::accentSoft();
    knobStyle.pointerColour = centered ? tokens::accent() : tokens::textPrimary();
    drawRotaryKnob (g, kb.reduced (1.5f), (pan + 1.0f) * 0.5f,
                    aMin, aMax, 0.5f, knobStyle, true,
                    highlighted, dragging);

    // Etichette L / R + readout (C / L## / R##).
    g.setColour (tokens::textSecondary().withAlpha (0.7f));
    g.setFont (tokens::type::section().withHeight (8.0f));
    g.drawText (startLabel, labelRow.removeFromLeft (12.0f).toNearestInt(),  juce::Justification::centredLeft,  false);
    g.drawText (endLabel, labelRow.removeFromRight (12.0f).toNearestInt(), juce::Justification::centredRight, false);

    const juce::String pv = centered ? juce::String ("C")
        : juce::String (pan < 0.0f ? "L" : "R") + juce::String (juce::roundToInt (std::abs (pan) * 100.0f));
    g.setColour (tokens::textValue());
    g.setFont (tokens::type::value().withHeight (9.0f));
    g.drawText (readout.isNotEmpty() ? readout : pv, labelRow.toNearestInt(), juce::Justification::centred, false);
}

juce::Rectangle<int> panReadoutRect (juce::Rectangle<float> area) noexcept
{
    // Stessa geometria di drawPanKnob: fascia inferiore, meno 12px per lato
    // (dove cadono le etichette "L" e "R"); cio' che resta e' il readout centrale.
    auto labelRow = StripKnobGeometry::readoutBounds (area);
    labelRow.removeFromLeft (12.0f);
    labelRow.removeFromRight (12.0f);
    return labelRow.toNearestInt();
}

juce::String panValueToText (float panValue)
{
    const float pan = juce::jlimit (-1.0f, 1.0f, panValue);
    if (std::abs (pan) < 0.02f) { return "C"; }
    return juce::String (pan < 0.0f ? "L" : "R") + juce::String (juce::roundToInt (std::abs (pan) * 100.0f));
}

bool parsePanText (const juce::String& text, float& outPan) noexcept
{
    auto t = text.trim().toUpperCase();
    if (t.isEmpty()) { return false; }
    if (t == "C") { outPan = 0.0f; return true; }

    float sign = 0.0f;
    if      (t.startsWithChar ('L')) { sign = -1.0f; t = t.substring (1).trim(); }
    else if (t.startsWithChar ('R')) { sign =  1.0f; t = t.substring (1).trim(); }

    if (! t.containsAnyOf ("0123456789")) { return false; }   // "L"/"R" nudo o garbage
    const double n = t.getDoubleValue();

    if (sign != 0.0f)
        outPan = juce::jlimit (-1.0f, 1.0f, sign * (float) (std::abs (n) / 100.0));   // L##/R## = percentuale
    else
        outPan = juce::jlimit (-1.0f, 1.0f, std::abs (n) <= 1.0 ? (float) n : (float) (n / 100.0));
    return true;
}

void drawCollapseChip (juce::Graphics& g, juce::Rectangle<float> area,
                       bool collapsed, bool highlighted)
{
    const float d = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (d, d);
    const float corner = d * 0.30f;

    fillRecessedSurface (g, box, corner);
    if (highlighted)
    {
        g.setColour (tokens::accent().withAlpha (0.18f));
        g.fillRoundedRectangle (box, corner);
    }
    g.setColour (tokens::borderHairline());
    g.drawRoundedRectangle (box.reduced (0.5f), corner, 1.0f);

    // Chevron: ripiegato = freccia a DESTRA ("apri, dispiega verso destra"),
    // aperto = freccia a SINISTRA ("richiudi verso il nome"). Coerente col layout
    // orizzontale dei gruppi (chip alla destra del nome).
    const auto c = box.getCentre();
    const float r = d * 0.18f;
    juce::Path p;
    if (collapsed)
    {
        p.startNewSubPath (c.x - r * 0.5f, c.y - r);
        p.lineTo (c.x + r * 0.5f, c.y);
        p.lineTo (c.x - r * 0.5f, c.y + r);
    }
    else
    {
        p.startNewSubPath (c.x + r * 0.5f, c.y - r);
        p.lineTo (c.x - r * 0.5f, c.y);
        p.lineTo (c.x + r * 0.5f, c.y + r);
    }
    g.setColour (highlighted ? tokens::textPrimary() : tokens::textSecondary());
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void drawDbChip (juce::Graphics& g, juce::Rectangle<int> area,
                 const juce::String& text, float alpha)
{
    if (area.isEmpty()) { return; }

    auto chip = area.toFloat();
    chip = chip.withSizeKeepingCentre (juce::jmin (chip.getWidth() - 2.0f, 56.0f),
                                       juce::jmin (chip.getHeight() - 1.0f, 14.0f));
    const float corner = chip.getHeight() * 0.5f;

    fillRecessedSurface (g, chip, corner);
    g.setColour (tokens::borderHairline().withAlpha (0.8f * alpha));
    g.drawRoundedRectangle (chip.reduced (0.5f), corner, 1.0f);

    g.setColour (tokens::textValue().withAlpha (0.95f * alpha));
    const juce::Font f = tokens::type::value().withHeight (10.0f);
    g.setFont (f);
    // Centratura OTTICA: drawFittedText centrato riserva lo spazio del descent in
    // basso, ma un readout di soli digit (nessun discendente) resta alto di ~descent/2.
    // Spostiamo il box in giu' di descent/2 cosi' i numeri risultano centrati.
    const auto textBox = chip.reduced (4.0f, 0.0f).translated (0.0f, f.getDescent() * 0.5f);
    g.drawFittedText (text, textBox.toNearestInt(), juce::Justification::centred, 1);
}

namespace
{
void drawStripFaderScaleMark (
    juce::Graphics& g, const juce::Rectangle<float> area,
    const StripFaderGeometry& geometry, const StripOrientation orientation,
    const float position, const juce::String& label, const bool emphasized)
{
    const bool horizontal = orientation == StripOrientation::horizontal;
    const float travel = geometry.travel();
    const bool major = label.isNotEmpty();
    const auto tickInk = emphasized
        ? tokens::accent().withAlpha (horizontal ? 1.0f : 0.9f)
        : tokens::textSecondary().withAlpha (major ? 0.45f : 0.28f);
    const float length = major ? 6.0f : 3.5f;
    const float centre = geometry.centre (juce::jlimit (0.0f, 1.0f, position));

    g.setColour (tickInk);
    if (horizontal)
    {
        g.fillRect (juce::Rectangle<float> (
            centre - 0.5f, area.getCentreY() - 5.0f - length, 1.0f, length));
        g.fillRect (juce::Rectangle<float> (
            centre - 0.5f, area.getCentreY() + 5.0f, 1.0f, length));
        if (major && travel >= 180.0f && area.getHeight() >= 36.0f)
        {
            g.setColour (emphasized ? tokens::accent()
                                    : tokens::textSecondary().withAlpha (0.55f));
            g.drawText (label, juce::Rectangle<float> (
                centre - 10.0f, area.getY(), 20.0f, 9.0f),
                juce::Justification::centred, false);
        }
        return;
    }

    const float cx = area.getCentreX();
    g.fillRect (juce::Rectangle<float> (
        cx - 5.0f - length, centre - 0.5f, length, 1.0f));
    g.fillRect (juce::Rectangle<float> (
        cx + 5.0f, centre - 0.5f, length, 1.0f));

    if (major && travel >= 90.0f && area.getWidth() >= 50.0f)
    {
        g.setColour (emphasized ? tokens::accent()
                                : tokens::textSecondary().withAlpha (0.55f));
        g.drawText (label, juce::Rectangle<float> (
            area.getX(), centre - 4.5f,
            cx - 13.0f - area.getX(), 9.0f).toNearestInt(),
            juce::Justification::centredRight, false);
    }
}
} // namespace

void drawStripFaderScale (juce::Graphics& g, juce::Rectangle<int> faderArea,
                          const StripOrientation orientation, const float maximumDb)
{
    const bool horizontal = orientation == StripOrientation::horizontal;
    const int length = horizontal ? faderArea.getWidth() : faderArea.getHeight();
    if (faderArea.isEmpty() || length < 60) { return; }

    struct Mark { float db; bool major; };
    const Mark marks[] =
    {
        { maximumDb, true }, { maximumDb * 0.5f, false },
        {   0.0f, true  },
        {  -3.0f, false }, {  -6.0f, false }, {  -9.0f, false },
        { -12.0f, true  }, { -18.0f, false },
        { -24.0f, true  }, { -30.0f, false },
        { -36.0f, true  }, { -42.0f, false },
        { -48.0f, true  }, { -54.0f, false },
        { -60.0f, true  },
    };

    const auto area = faderArea.toFloat();
    const StripFaderGeometry geometry { area, orientation };
    if (geometry.travel() <= 0.0f) { return; }
    g.setFont (tokens::type::value().withHeight (7.0f));

    for (const auto& mark : marks)
    {
        const auto label = mark.major
            ? (mark.db > 0.0f ? "+" + juce::String ((int) mark.db)
                              : juce::String ((int) -mark.db))
            : juce::String {};
        drawStripFaderScaleMark (
            g, area, geometry, orientation,
            faderDbToPos (mark.db, maximumDb), label, mark.db == 0.0f);
    }
}

void drawStripFaderScale (
    juce::Graphics& g, juce::Rectangle<int> faderArea,
    const std::span<const StripFaderScaleMark> marks,
    const StripOrientation orientation)
{
    const bool horizontal = orientation == StripOrientation::horizontal;
    const int length = horizontal ? faderArea.getWidth() : faderArea.getHeight();
    if (faderArea.isEmpty() || length < 60 || marks.empty()) { return; }

    const auto area = faderArea.toFloat();
    const StripFaderGeometry geometry { area, orientation };
    if (geometry.travel() <= 0.0f) { return; }
    g.setFont (tokens::type::value().withHeight (7.0f));

    for (const auto& mark : marks)
        drawStripFaderScaleMark (
            g, area, geometry, orientation,
            mark.position, mark.label, mark.emphasized);
}

void drawStripFader (juce::Graphics& g, const juce::Rectangle<int> area,
                     const float position, const juce::Colour tint,
                     juce::Component& component, const StripOrientation orientation)
{
    if (area.isEmpty()) { return; }
    if (orientation == StripOrientation::vertical)
    {
        getCommonLAF (component).drawFader (g, area.toFloat(), position, tint);
        return;
    }
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform (0.0f, -1.0f, (float) area.getRight(),
                                           1.0f, 0.0f, (float) area.getY()));
    getCommonLAF (component).drawFader (
        g, { 0.0f, 0.0f, (float) area.getHeight(), (float) area.getWidth() }, position, tint);
}

} // namespace ayra
