/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

/**
 * @file ayra_CommonDefaultLookAndFeel_Inspector.cpp
 * @brief Ricette SESSION/TRANSPORT + INSPECTOR/SNAPSHOT/CPU: clip cell,
 *        transport button, CPU arc, snapshot launcher, node list item (hybrid:
 *        chrome con tint nodeColour), curve editor.
 *
 * Partial di `CommonDefaultLookAndFeel`: NESSUN #include qui.
 */

namespace ayra
{

namespace tk = tokens;

namespace
{
    /// Quadrato massimo centrato in `r` (lato = min(w,h)). Pattern ripetuto dai
    /// disegni a quadrante (gauge CPU, snapshot launcher) per inscrivere il dial.
    [[nodiscard]] inline juce::Rectangle<float> inscribeSquare (juce::Rectangle<float> r) noexcept
    {
        const float side = juce::jmin (r.getWidth(), r.getHeight());
        return r.withSizeKeepingCentre (side, side);
    }

    /// Riempie il glifo di trasporto (play=triangolo / stop=rounded rect / record=
    /// ellisse) DENTRO `a` con `colour`. Sorgente unica del vocabolario glifi-stato
    /// usato sia da drawTransportButton sia da drawClipCell. Il caller passa il
    /// rettangolo gia' "ridotto" cosi' il framing per-stato resta al call-site
    /// (pixel invariati). Rewind non e' qui: e' specifico del solo transport button.
    inline void fillTransportGlyph (juce::Graphics& g, juce::Rectangle<float> a,
                                    TransportKind kind, juce::Colour colour) noexcept
    {
        g.setColour (colour);
        switch (kind)
        {
            case TransportKind::Play:
            { juce::Path t; t.addTriangle (a.getX(), a.getY(), a.getX(), a.getBottom(), a.getRight(), a.getCentreY());
              g.fillPath (t); break; }
            case TransportKind::Stop:
                g.fillRoundedRectangle (a, 2.0f); break;
            case TransportKind::Record:
                g.fillEllipse (a); break;
            case TransportKind::Rewind:
            default:
                break;
        }
    }
}

//==============================================================================
// Session
//==============================================================================
void CommonDefaultLookAndFeel::drawClipCell (juce::Graphics& g, juce::Rectangle<float> r,
                                             juce::Colour tint, ClipState state, const juce::String& name)
{
    const float corner = 7.0f;
    juce::Path p; p.addRoundedRectangle (r, corner);

    if (state == ClipState::Empty)
    {
        g.setColour (tk::surfaceRecessed().withAlpha (0.6f));
        g.fillPath (p);
        g.setColour (tk::borderHairline().withAlpha (0.5f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        // "+" tenue al centro.
        const auto c = r.getCentre(); const float s = 4.0f;
        g.setColour (tk::textDisabled());
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.2f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.2f);
        return;
    }

    const bool active = (state == ClipState::Playing);
    const bool rec    = (state == ClipState::Recording);
    const juce::Colour base = rec ? tk::streamMidi() : tint;

    if (active || rec)
    {
        // Glow a piena estensione: INTENZIONALE. La clip cell e' una primitiva di
        // CANVAS (la SessionView disegna la griglia di celle in un'unica paint, non
        // un Component per cella) -> il glow rende sul canvas condiviso, niente clip
        // ai bounds. Se un giorno la cella diventasse un Component a se', spostare
        // il glow sul parent o sostituirlo con un rim accent (vedi drawPluginCard).
        const ShadowGeom gg { (int) r.getWidth(), (int) r.getHeight(), (int) corner };
        shadowCache().drop (gg, base.withAlpha (0.5f), tk::depth::glowBlur, { 0, 0 }, 1).render (g, p);
    }
    juce::ColourGradient grad (base.withAlpha (active || rec ? 0.85f : 0.20f), r.getX(), r.getY(),
                               base.withAlpha (active || rec ? 0.55f : 0.12f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (p);
    g.setColour (base.withAlpha (active || rec ? 1.0f : 0.6f));
    g.strokePath (p, juce::PathStrokeType (1.2f));

    // Glifo stato a sinistra.
    auto gly = juce::Rectangle<float> (r.getX() + 6, r.getCentreY() - 7, 14, 14);
    const juce::Colour glyCol = (active || rec) ? juce::Colours::white : base;
    if (active)      { fillTransportGlyph (g, gly.reduced (3.0f), TransportKind::Stop,   glyCol); }  // stop
    else if (rec)    { fillTransportGlyph (g, gly.reduced (2.0f), TransportKind::Record, glyCol); }
    else             { fillTransportGlyph (g, gly.reduced (2.0f), TransportKind::Play,   glyCol); }

    g.setColour ((active || rec) ? juce::Colours::white : tk::textPrimary());
    g.setFont (tk::type::body());
    g.drawText (name, r.withTrimmedLeft (26).toNearestInt(), juce::Justification::centredLeft, true);
}

//==============================================================================
// Transport
//==============================================================================
void CommonDefaultLookAndFeel::drawTransportButton (juce::Graphics& g, juce::Rectangle<float> b,
                                                    TransportKind kind, bool active)
{
    drawAppIconChip (g, b, active);
    const juce::Colour col = active ? juce::Colours::white
                                    : (kind == TransportKind::Record ? tk::streamMidi() : tk::textSecondary());
    auto a = b.reduced (b.getWidth() * 0.3f);
    switch (kind)
    {
        case TransportKind::Play:
        case TransportKind::Stop:
        case TransportKind::Record:
            fillTransportGlyph (g, a, kind, col); break;
        case TransportKind::Rewind:
        { juce::Path t; auto h = b.reduced (b.getWidth() * 0.28f);
          t.addTriangle (h.getRight(), h.getY(), h.getRight(), h.getBottom(), h.getCentreX(), h.getCentreY());
          t.addTriangle (h.getCentreX(), h.getY(), h.getCentreX(), h.getBottom(), h.getX(), h.getCentreY());
          g.setColour (col); g.fillPath (t); break; }
    }
}

//==============================================================================
// Node-graph inspectors / snapshots / CPU
//==============================================================================
namespace
{
    /// Segmento d'arco stroked (gradi, orario da ore-3 antiorario JUCE-style).
    inline void arcSeg (juce::Graphics& g, juce::Point<float> c, float r,
                        float a0deg, float a1deg, float thick, juce::Colour col)
    {
        const float d2r = juce::MathConstants<float>::pi / 180.0f;
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, a0deg * d2r, a1deg * d2r, true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}

void CommonDefaultLookAndFeel::drawCpuArc (juce::Graphics& g, juce::Rectangle<float> b,
                                           float level01, const juce::String& caption)
{
    const float lv = juce::jlimit (0.0f, 1.0f, level01);
    auto sq = inscribeSquare (b);
    const auto c = sq.getCentre();
    const float r = sq.getWidth() * 0.5f - 3.0f;

    // Gauge 3/4: da 135 a -135 (sweep 270) in convenzione JUCE (radianti, 0 = top).
    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float span  = juce::MathConstants<float>::pi * 1.5f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, start + span, true);
    g.setColour (tk::surfaceRecessed());
    g.strokePath (track, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const juce::Colour vc = lv >= 0.85f ? tk::streamMidi()
                          : lv >= 0.6f  ? tk::streamParam()
                                        : tk::streamAudio();
    if (lv > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, start, start + span * lv, true);
        g.setColour (vc);
        g.strokePath (val, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (tk::textPrimary());
    g.setFont (tk::type::value().withHeight (juce::jmax (10.0f, r * 0.55f)));
    g.drawText (juce::String (juce::roundToInt (lv * 100.0f)) + "%",
                sq.toNearestInt(), juce::Justification::centred, false);
    if (caption.isNotEmpty())
    {
        g.setColour (tk::textSecondary());
        g.setFont (tk::type::section());
        g.drawText (caption.toUpperCase(), b.removeFromBottom (12.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }
}

void CommonDefaultLookAndFeel::drawSnapshotLaunchButton (juce::Graphics& g, juce::Rectangle<float> b,
                                                         juce::Colour tint, ClipState state,
                                                         float ringProgress)
{
    auto sq = inscribeSquare (b);
    const auto c = sq.getCentre();
    const float R = sq.getWidth() * 0.5f - 1.5f;

    const bool empty  = (state == ClipState::Empty);
    const bool active = (state == ClipState::Playing);
    const bool rec    = (state == ClipState::Recording);
    const juce::Colour base = empty ? tk::textDisabled()
                            : (active || rec) ? tint : tint.withAlpha (0.7f);

    // Anello esterno.
    g.setColour (empty ? tk::borderHairline() : base.withAlpha (0.55f));
    g.drawEllipse (sq.reduced (1.0f), 2.0f);

    // Glifo "power/shutdown" (cerchio con gap in alto + asta verticale).
    auto ic = sq.reduced (sq.getWidth() * 0.30f);
    const float ir = ic.getWidth() * 0.5f;
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, ir, ir, 0.0f,
                        juce::MathConstants<float>::pi * 0.30f,
                        juce::MathConstants<float>::pi * 1.70f, true);
    g.setColour (empty ? base.withAlpha (0.35f) : base);
    g.strokePath (ring, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (c.x, c.y - ir - 1.0f, c.x, c.y + ir * 0.15f, 2.0f);

    // Arco countdown (pending).
    if (ringProgress >= 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, R, R, 0.0f, juce::MathConstants<float>::pi,
                           juce::MathConstants<float>::pi + juce::MathConstants<float>::twoPi
                               * juce::jlimit (0.0f, 1.0f, ringProgress), true);
        g.setColour (tint);
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void CommonDefaultLookAndFeel::drawNodeListItem (juce::Graphics& g, juce::Rectangle<float> b,
                                                 const juce::String& name, const juce::String& uid,
                                                 juce::Colour tint, bool selected, bool hover)
{
    auto card = (selected || hover) ? b.reduced (4.0f) : b;

    // Fondo: gradiente radiale dal colore nodo (alpha 0.8 attivo / 0.2 a riposo),
    // centrato e offsettato verso il basso di height/2 (come NodeItem reale).
    const float alpha = (selected || hover) ? 0.8f : 0.2f;
    const auto ctr = card.getCentre();
    juce::Path body; body.addRoundedRectangle (card, 6.0f);
    g.setColour (tk::node::outerVoid().withAlpha (0.92f));
    g.fillPath (body);
    juce::ColourGradient grad (tint.withAlpha (alpha), ctr.x, ctr.y + card.getHeight() * 0.5f,
                               juce::Colours::transparentBlack, ctr.x, ctr.y - card.getHeight(), true);
    g.setGradientFill (grad);
    g.fillPath (body);

    // Separatore in alto (bianco), esteso a tutta la larghezza se selezionato.
    g.setColour (juce::Colours::white.withAlpha (selected ? 0.5f : 0.25f));
    const float sepW = selected ? card.getWidth() : card.getWidth() * 0.6f;
    g.fillRect (card.getX() + 4.0f, card.getY() + 1.0f, sepW - 8.0f, 1.0f);

    // Indicatore colore a destra + dial segmentato rotante.
    const float ind = card.getHeight() * 0.5f;
    const auto ie = juce::Rectangle<float> (card.getRight() - ind - 6.0f, card.getCentreY() - ind * 0.5f, ind, ind);
    const auto dc = ie.getCentre();
    const float dr = ind * 0.85f;
    if (selected || hover)
    {
        arcSeg (g, dc, dr, 0.0f, 36.0f, 1.6f, tint.withAlpha (0.9f));
        arcSeg (g, dc, dr, 144.0f, 216.0f, 1.6f, tint.withAlpha (0.9f));
        arcSeg (g, dc, dr, 324.0f, 360.0f, 1.6f, tint.withAlpha (0.9f));
    }
    else
    {
        arcSeg (g, dc, dr, 0.0f, 72.0f, 1.4f, tint.withAlpha (0.7f));
        arcSeg (g, dc, dr, 180.0f, 252.0f, 1.4f, tint.withAlpha (0.7f));
    }
    g.setColour (tint);
    g.fillEllipse (ie.reduced (2.5f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawEllipse (ie.reduced (2.5f), 1.0f);

    // HUD corner-brackets (selected/hover).
    if (selected || hover)
    {
        g.setColour (tint.withAlpha (0.85f));
        const float L = 8.0f;
        juce::Path tl; tl.startNewSubPath (card.getX(), card.getY() + L);
        tl.lineTo (card.getX(), card.getY()); tl.lineTo (card.getX() + L, card.getY());
        juce::Path br; br.startNewSubPath (card.getRight(), card.getBottom() - L);
        br.lineTo (card.getRight(), card.getBottom()); br.lineTo (card.getRight() - L, card.getBottom());
        g.strokePath (tl, juce::PathStrokeType (1.5f));
        g.strokePath (br, juce::PathStrokeType (1.5f));
    }

    // Nome (Helvetica) + uid piccolo in basso-sinistra.
    auto txt = card.reduced (8.0f, 4.0f).withTrimmedRight (ind + 10.0f);
    g.setColour (juce::Colours::white.withAlpha (selected || hover ? 1.0f : 0.85f));
    g.setFont (ayra::tokens::type::nodeName().withHeight (13.0f));
    g.drawText (name, txt.toNearestInt(), juce::Justification::centredLeft, true);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (ayra::tokens::type::nodeDetail());
    g.drawText (uid, txt.removeFromBottom (10.0f).toNearestInt(), juce::Justification::bottomLeft, true);
}

void CommonDefaultLookAndFeel::drawCurveEditor (juce::Graphics& g, juce::Rectangle<float> b,
                                                int mode, juce::Colour tint)
{
    juce::Path well; well.addRoundedRectangle (b, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (tk::detail::dark() ? 0.6f : 0.10f));
    g.fillPath (well);
    g.setColour (tk::borderHairline());
    g.strokePath (well, juce::PathStrokeType (1.0f));

    // Griglia (4x4).
    g.setColour (tk::borderHairline().withAlpha (0.5f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = b.getX() + b.getWidth() * (i / 4.0f);
        const float y = b.getY() + b.getHeight() * (i / 4.0f);
        g.drawLine (x, b.getY(), x, b.getBottom(), 0.5f);
        g.drawLine (b.getX(), y, b.getRight(), y, 0.5f);
    }

    auto in = b.reduced (4.0f);
    const juce::Point<float> p0 (in.getX(), in.getBottom());
    const juce::Point<float> p1 (in.getRight(), in.getY());

    // Punti di controllo calcolati UNA volta: stessi valori per la curva e per i
    // dot ambra, cosi' una modifica delle proporzioni non desincronizza i due.
    const juce::Point<float> quadCp (in.getX() + in.getWidth() * 0.5f,  in.getY() + in.getHeight() * 0.15f);
    const juce::Point<float> cubic1 (in.getX() + in.getWidth() * 0.30f, in.getBottom() - in.getHeight() * 0.05f);
    const juce::Point<float> cubic2 (in.getX() + in.getWidth() * 0.70f, in.getY() + in.getHeight() * 0.05f);

    juce::Path curve; curve.startNewSubPath (p0);
    if (mode <= 0)
    {
        curve.lineTo (p1);
    }
    else if (mode == 1)
    {
        curve.quadraticTo (quadCp, p1);
    }
    else
    {
        curve.cubicTo (cubic1, cubic2, p1);
    }
    g.setColour (tint);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Control point(s) ambra.
    auto cp = [&] (juce::Point<float> p)
    {
        g.setColour (tk::accent());
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f, 1.0f);
    };
    cp (p0); cp (p1);
    if (mode == 1) cp (quadCp);
    if (mode == 2) { cp (cubic1); cp (cubic2); }
}

} // namespace ayra
