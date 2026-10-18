/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

/**
 * @file ayra_CommonDefaultLookAndFeel_Mixer.cpp
 * @brief Ricette MIXER (Frosted Lab): channel strip, colour cap, fader,
 *        VU meter, macro knob.
 *
 * Partial di `CommonDefaultLookAndFeel`: NESSUN #include qui.
 */

namespace ayra
{

namespace tk = tokens;

//==============================================================================
void CommonDefaultLookAndFeel::drawChannelStripBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawAppPanel (g, r, true, tk::radius::card * 0.7f);
}

void CommonDefaultLookAndFeel::drawColourCap (juce::Graphics& g, juce::Rectangle<float> r,
                                              juce::Colour tint, const juce::String& name)
{
    juce::Path cap; cap.addRoundedRectangle (r, 6.0f);
    g.setColour (tint.withAlpha (0.88f));
    g.fillPath (cap);
    g.setColour (juce::Colours::white);
    g.setFont (tk::type::section().withHeight (11.0f));
    g.drawText (name, r.toNearestInt().reduced (6, 0), juce::Justification::centred, true);
}

void CommonDefaultLookAndFeel::drawFader (juce::Graphics& g, juce::Rectangle<float> area,
                                          float pos01, juce::Colour tint)
{
    // Track recessed verticale centrato + thumb raised tinto.
    const StripFaderGeometry geometry { area };
    auto track = area.withSizeKeepingCentre (8.0f, area.getHeight()).reduced (0, StripFaderGeometry::endInset);
    drawAppPanel (g, track, false, 4.0f);

    const float ty = geometry.centre (juce::jlimit (0.0f, 1.0f, pos01)) - StripFaderGeometry::thumbLength * 0.5f;
    auto thumb = juce::Rectangle<float> (area.getCentreX() - StripFaderGeometry::thumbWidth * 0.5f,
        ty, StripFaderGeometry::thumbWidth, StripFaderGeometry::thumbLength);
    juce::Path tp; tp.addRoundedRectangle (thumb, 5.0f);
    // Ombra FISSA, piccola e con offset solo verticale: la chiave di cache e'
    // costante (una sola entry, cache-ata anche durante il drag) e il raggio e'
    // abbastanza contenuto da restare nel contesto del fader. NB: NON usare un fit
    // position-dipendente qui -> cambierebbe la chiave a ogni frame (re-blur +
    // crescita della cache) e azzererebbe l'ombra su strip strette.
    const ShadowGeom gg { (int) thumb.getWidth(), (int) thumb.getHeight(), 5 };
    shadowCache().drop (gg, tk::shadowDark(), 6, { 0, 1 }).render (g, tp);
    juce::ColourGradient grad (tk::surfaceRaised().brighter (0.08f), thumb.getX(), thumb.getY(),
                               tk::surfaceRaised().darker (0.06f), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (tp);
    g.setColour (tint);
    g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 8.0f, 2.0f));
    g.setColour (tk::borderHairline());
    g.strokePath (tp, juce::PathStrokeType (1.0f));
}

void CommonDefaultLookAndFeel::drawVMeter (juce::Graphics& g, juce::Rectangle<float> r, float level)
{
    // Un VU meter e' un READOUT di precisione, non una superficie "soffice": niente
    // inner-shadow (offuscherebbe il fondo scala e renderebbe il livello basso
    // muddy). Fondo piatto recessed + fill gradiente verde->ambra->rosso + hairline.
    const float corner = juce::jmin (3.0f, r.getWidth() * 0.4f);
    juce::Path well; well.addRoundedRectangle (r, corner);
    g.setColour (tk::surfaceRecessed());
    g.fillPath (well);

    const float lv = juce::jlimit (0.0f, 1.0f, level);
    if (lv > 0.001f)
    {
        auto fill = r.reduced (1.0f);
        fill = fill.removeFromBottom (fill.getHeight() * lv);
        juce::Path fp; fp.addRoundedRectangle (fill, juce::jmax (0.5f, corner - 1.0f));
        juce::ColourGradient grad (tk::streamAudio(), r.getCentreX(), r.getBottom(),
                                   tk::streamMidi(), r.getCentreX(), r.getY(), false);
        grad.addColour (0.7, tk::accent());
        g.setGradientFill (grad);
        g.fillPath (fp);
    }

    // Hairline inset (vedi regola coerenza stroke): frame del well sopra il fill.
    const auto ri = r.reduced (0.5f);
    juce::Path hp; hp.addRoundedRectangle (ri, juce::jmax (0.0f, corner - 0.5f));
    g.setColour (tk::borderHairline());
    g.strokePath (hp, juce::PathStrokeType (1.0f));
}

void CommonDefaultLookAndFeel::drawMacroKnob (juce::Graphics& g, juce::Rectangle<float> knob,
                                              float value, bool bipolar)
{
    constexpr float start = juce::MathConstants<float>::pi * 1.25f;
    constexpr float end = juce::MathConstants<float>::pi * 2.75f;
    RotaryKnobStyle style;
    drawRotaryKnob (g, knob, value, start, end, bipolar ? 0.5f : 0.0f,
                    style, true, false, false);
}

} // namespace ayra
