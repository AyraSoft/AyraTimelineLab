/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

/**
 * @file ayra_CommonDefaultLookAndFeel_Form.cpp
 * @brief Ricette FORM / BROWSER (Frosted Lab): switch, badge, search field,
 *        param slider, plugin card.
 *
 * Partial di `CommonDefaultLookAndFeel`: NESSUN #include qui.
 */

namespace ayra
{

namespace tk = tokens;

//==============================================================================
void CommonDefaultLookAndFeel::drawSwitch (juce::Graphics& g, juce::Rectangle<float> r, bool on,
                                           juce::Colour accentColour, juce::Colour offColour)
{
    const juce::Colour accent = accentColour.isTransparent() ? accentOr (tk::accent()) : accentColour;
    const bool hasSemanticOff = ! offColour.isTransparent();

    const float trackH = juce::jmin (22.0f, r.getHeight());
    const float trackW = juce::jmin (r.getWidth(), trackH * 1.9f);
    auto track = juce::Rectangle<float> (r.getX(), r.getCentreY() - trackH * 0.5f, trackW, trackH);
    const float rad = trackH * 0.5f;
    juce::Path pill; pill.addRoundedRectangle (track, rad);
    g.setColour (on ? accent : (hasSemanticOff ? offColour : tk::surfaceRecessed()));
    g.fillPath (pill);
    g.setColour (on ? accent.withAlpha (0.35f)
                    : (hasSemanticOff ? offColour.withAlpha (0.45f) : tk::borderHairline()));
    g.strokePath (pill, juce::PathStrokeType (on ? 1.5f : 1.0f));

    const float knobD = trackH - 5.0f;
    const float knobX = on ? track.getRight() - knobD - 2.5f : track.getX() + 2.5f;
    auto knob = juce::Rectangle<float> (knobX, track.getY() + 2.5f, knobD, knobD);
    g.setColour (juce::Colours::white); g.fillEllipse (knob);
    g.setColour (juce::Colours::black.withAlpha (0.12f)); g.drawEllipse (knob, 0.75f);
}

void CommonDefaultLookAndFeel::drawBadge (juce::Graphics& g, juce::Rectangle<float> r,
                                          const juce::String& t, juce::Colour tint)
{
    juce::Path pill; pill.addRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (tint.withAlpha (tk::detail::dark() ? 0.22f : 0.16f));
    g.fillPath (pill);
    g.setColour (tint);
    g.strokePath (pill, juce::PathStrokeType (1.0f));
    g.setColour (tint);
    g.setFont (tk::type::caption().withHeight (9.5f));
    g.drawText (t, r.toNearestInt(), juce::Justification::centred, false);
}

void CommonDefaultLookAndFeel::drawSearchField (juce::Graphics& g, juce::Rectangle<float> r,
                                                const juce::String& placeholder)
{
    drawAppPanel (g, r, false, r.getHeight() * 0.5f);
    // lente.
    const float cy = r.getCentreY();
    const float cx = r.getX() + 14.0f;
    g.setColour (tk::textSecondary());
    g.drawEllipse (cx - 5.0f, cy - 5.0f, 9.0f, 9.0f, 1.4f);
    g.drawLine (cx + 3.5f, cy + 3.5f, cx + 7.0f, cy + 7.0f, 1.4f);
    g.setColour (tk::textDisabled());
    g.setFont (tk::type::body());
    g.drawText (placeholder, r.withTrimmedLeft (28).toNearestInt(), juce::Justification::centredLeft, true);
}

void CommonDefaultLookAndFeel::drawParamSlider (juce::Graphics& g, juce::Rectangle<float> r,
                                                float value01, const juce::String& name,
                                                const juce::String& valueText, juce::Colour tint)
{
    // Riga param stile LinearBar: well recessed (ricetta coerente) + fill tinta
    // + nome a sx, valore a dx.
    const float corner = juce::jmin (6.0f, r.getHeight() * 0.4f);
    fillRecessedSurface (g, r, corner);

    const float v = juce::jlimit (0.0f, 1.0f, value01);
    if (v > 0.001f)
    {
        auto fill = r.reduced (1.5f); fill = fill.removeFromLeft (fill.getWidth() * v);
        juce::Path fp; fp.addRoundedRectangle (fill, juce::jmax (0.5f, corner - 1.0f));
        juce::ColourGradient grad (tint.withAlpha (0.85f), fill.getX(), fill.getY(),
                                   tint.withAlpha (0.55f), fill.getX(), fill.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fp);
    }

    auto txt = r.toNearestInt().reduced (10, 0);
    g.setColour (tk::textPrimary());
    g.setFont (tk::type::body());
    g.drawText (name, txt, juce::Justification::centredLeft, true);
    g.setColour (tk::textValue());
    g.setFont (tk::type::value().withHeight (11.0f));
    g.drawText (valueText, txt, juce::Justification::centredRight, true);
}

void CommonDefaultLookAndFeel::drawPluginCard (juce::Graphics& g, juce::Rectangle<float> b,
                                               const juce::String& name, const juce::String& vendor,
                                               const juce::String& fmt, StreamType kind, bool selected)
{
    const float corner = tk::radius::card * 0.7f;
    const auto tint = tk::streamColour (kind);

    // RICETTA COERENTE: la card del browser e' tipicamente un juce::Component in
    // una lista -> niente drop-shadow (verrebbe clippata ai bounds = piastra
    // squadrata). Rilievo da gradiente + bevel; enfasi "selected" da rim accent
    // (non da glow drop-shadow, anch'esso clippabile in un alone squadrato).
    tk::fillRaisedSurface (g, b, corner, tk::surfaceRaised(), selected ? 0.08f : 0.04f);

    // rail tinta in alto.
    juce::Path rail; rail.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), 3.5f, corner, corner, true, true, false, false);
    g.setColour (tint); g.fillPath (rail);

    if (selected)
    {
        juce::Path body; body.addRoundedRectangle (b, corner);
        g.setColour (tint.withAlpha (0.85f));
        g.strokePath (body, juce::PathStrokeType (1.4f));
    }

    auto in = b.reduced (12.0f, 10.0f);
    g.setColour (tk::textPrimary());
    g.setFont (tk::type::title().withHeight (14.0f));
    g.drawFittedText (name, in.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 2);
    g.setColour (tk::textSecondary());
    g.setFont (tk::type::caption());
    g.drawText (vendor, in.removeFromTop (16.0f).toNearestInt(), juce::Justification::topLeft, true);

    // badge formato + tipo.
    auto bottom = in.removeFromBottom (16.0f);
    const juce::Colour fmtCol = fmt.containsIgnoreCase ("VST3") ? tk::streamOsc()
                              : fmt.containsIgnoreCase ("AU")   ? tk::streamVideo() : tk::textSecondary();
    drawBadge (g, bottom.removeFromLeft (46.0f), fmt, fmtCol);
    bottom.removeFromLeft (6.0f);
    const juce::String knd = (kind == StreamType::audio) ? "FX" : (kind == StreamType::midi ? "MIDI" : "INSTR");
    drawBadge (g, bottom.removeFromLeft (44.0f), knd, tint);
}

} // namespace ayra
