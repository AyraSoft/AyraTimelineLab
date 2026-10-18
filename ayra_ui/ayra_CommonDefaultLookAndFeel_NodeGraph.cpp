/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / DefaultLookAndFeel — partial NODE GRAPH
   --------------------------------------------------------------------
 * @brief Ricette NODE GRAPH (Dark Cosmic): drawNodeCard / drawNodePin /
 *        drawConnectionCable. Riproduzione FEDELE di NodeComponent /
 *        PinComponent / ConnectorComponent di ayra_performer.
 *
 * Il tint atteso viene da `tokens::nodeColour(StreamType)` (palette satura su
 * nero), non da streamColour(). Partial di `CommonDefaultLookAndFeel`: NESSUN
 * #include qui.
 *
 * Gli stati visivi arrivano via gli struct aggregati `NodeCardVisual` /
 * `NodePinVisual` (vedi ayra_CommonLookAndFeelMethods.h): aggiungere un nuovo
 * attributo (es. un meter, un badge) e' un campo nello struct, non una nuova
 * firma. Qui sotto disegniamo TUTTI gli stati portati dal NodeComponent reale:
 *  - card: selezione, SOSPESO (scurita), HOVER (HUD corner-brackets), arco CPU;
 *  - pin: pieno/colore stream, ring scuro, "buco" se NON connesso, VU per-pin.
 */

namespace ayra
{

namespace tk = tokens;
namespace gl = glyphs;

//==============================================================================
void CommonDefaultLookAndFeel::drawNodeCard (juce::Graphics& g, const NodeCardVisual& v)
{
    // FOOTPRINT -> BOX -> CARD, 1:1 con NodeComponent::paint di ayra_performer.
    // `v.bounds` e' il FOOTPRINT del nodo (card + margini per pin/bottoni/cpu/logo,
    // disegnati a parte). Il `box` = footprint.reduced(pinSize=20) e' il "bounds"
    // del paint reale; la CARD vera (rounded rect con fill/bordo) = box.reduced(6).
    // Tutte le costanti px sono scalate da `v.scale` (zoom del canvas).
    const float s = juce::jmax (0.25f, v.scale);

    // La vista gerarchica riusa la STESSA superficie Dark Cosmic senza fingere
    // di essere un plugin vivo: niente console, ID, CPU o bottoni del processor.
    // Titolo e azioni del contenitore vengono sovrapposti dal consumer.
    if (v.compact)
    {
        const auto card = v.bounds.reduced (1.0f * s);
        if (card.getWidth() < 2.0f || card.getHeight() < 2.0f) { return; }

        const auto tint = v.suspended ? v.tint.darker().darker() : v.tint;
        const float corner = 12.0f * s;
        juce::Path body;
        body.addRoundedRectangle (card, corner);

        g.setColour (tk::node::shadowReactor().withAlpha (0.52f));
        g.fillRoundedRectangle (
            juce::Rectangle<float> (card.getX() - 4.0f * s, card.getY(),
                                    card.getWidth() + 4.0f * s,
                                    card.getHeight() + 4.0f * s),
            corner);

        if (! v.suspended)
        {
            juce::ColourGradient linear (
                juce::Colours::transparentBlack, card.getCentreX(), card.getBottom(),
                tint.withAlpha (v.selected ? 0.90f : 0.48f),
                card.getRight(), card.getY(), false);
            g.setGradientFill (linear);
            g.fillPath (body);
        }

        const auto centre = card.getCentre();
        const float radius = juce::jmax (card.getWidth(), card.getHeight()) * 0.5f;
        juce::ColourGradient radial (
            juce::Colours::transparentBlack, centre,
            tk::node::outerVoid().withAlpha (v.selected ? 0.44f : 0.76f),
            centre.translated (0.0f, radius), true);
        g.setGradientFill (radial);
        g.fillPath (body);

        g.setColour (v.suspended ? tint.darker() : tint);
        g.strokePath (body, juce::PathStrokeType (1.0f * s));
        if (v.selected)
        {
            g.setColour (accentOr (juce::Colours::white));
            g.drawRoundedRectangle (card.expanded (2.0f * s), corner,
                                    1.5f * s);
        }
        return;
    }

    const auto box  = v.bounds.reduced (20.0f * s);
    const auto card = box.reduced (6.0f * s);
    if (card.getWidth() < 2.0f || card.getHeight() < 2.0f) { return; }

    const bool selected = v.selected;
    const bool off      = v.suspended;   // bypass/shutdown -> card + testo scuri.

    const juce::Colour tint     = off ? v.tint.darker().darker() : v.tint;
    const juce::Colour titleCol = off ? tk::node::title().darker().darker() : tk::node::title();
    const float corner = 16.0f * s;

    juce::Path body; body.addRoundedRectangle (card, corner);

    // Ombra dura: offset 6px a sinistra + 6px in basso (shadowReactor @0.6).
    g.setColour (tk::node::shadowReactor().withAlpha (0.6f));
    g.fillRoundedRectangle (juce::Rectangle<float> (card.getX() - 6.0f * s, card.getY(),
                                                    card.getWidth() + 6.0f * s, card.getHeight() + 6.0f * s),
                            corner);

    // Fill obliquo (solo se NON sospeso): trasparente in basso-centro -> tint in
    // alto-destra; alpha pieno se selezionato, altrimenti 0.5.
    if (! off)
    {
        juce::ColourGradient gLin (juce::Colours::transparentBlack, card.getCentreX(), card.getBottom(),
                                   tint.withAlpha (selected ? 1.0f : 0.5f), card.getRight(), card.getY(), false);
        g.setGradientFill (gLin);
        g.fillPath (body);
    }

    // Vignette radiale verso outerVoid (sempre, anche sospeso).
    const auto centre = card.getCentre();
    const float radius = juce::jmax (card.getWidth(), card.getHeight()) * 0.5f;
    juce::ColourGradient gRad (juce::Colours::transparentBlack, centre,
                               tk::node::outerVoid().withAlpha (selected ? 0.5f : 0.8f),
                               centre.translated (0.0f, radius), true);
    g.setGradientFill (gRad);
    g.fillPath (body);

    // Bordo categoria 1px (scuro se sospeso).
    g.setColour (off ? tint.darker() : tint);
    g.strokePath (body, juce::PathStrokeType (1.0f * s));

    // Ring di selezione: box.reduced(4) (2px piu' esterno della card). Bianco di
    // default; vira al colore-traccia per-istanza se l'app ha settato un accent
    // (vedi CommonDefaultLookAndFeel::accentOr) -> il nodo selezionato "appartiene"
    // visivamente a questa traccia.
    if (selected)
    {
        g.setColour (accentOr (juce::Colours::white));
        g.drawRoundedRectangle (box.reduced (4.0f * s), corner, 1.0f * s);
    }

    //--- Helper di coordinate: native (pre-zoom) -> canvas (post-zoom). Tutte le
    //    posizioni di bottoni/decori del NodeComponent reale sono in px nativi.
    const float oX = v.bounds.getX(), oY = v.bounds.getY();
    const float nW = v.bounds.getWidth()  / s;   // footprint nativo (pre-zoom).
    const float nH = v.bounds.getHeight() / s;
    auto P    = [&] (float x, float y)                     { return juce::Point<float>    (oX + x * s, oY + y * s); };
    auto RECT = [&] (float x, float y, float w, float h)   { return juce::Rectangle<float> (oX + x * s, oY + y * s, w * s, h * s); };

    const auto grey     = juce::Colours::lightgrey;
    const bool isIO     = v.isInputNode || v.isOutputNode;
    const bool hasAudio = v.acceptsAudio || v.producesAudio;

    //--- TESTI (relativi al box, come il paint reale) -------------------------
    const juce::Font nameFont (ayra::tokens::type::nodeName().withHeight   (ayra::tokens::type::kNodeNameSize   * s));
    const juce::Font idFont   (ayra::tokens::type::nodeDetail().withHeight (ayra::tokens::type::kNodeDetailSize * s));
    const float nameH = nameFont.getHeight();

    // Titolo: banda centrata nel box (larghezza meta', altezza = fontH). Uso
    // GlyphArrangement come il reale -> stessa resa E misura nameW per il sotto-testo.
    auto textBounds = box.reduced (box.getWidth() * 0.25f, box.getHeight() * 0.5f - nameH * 0.5f);
    int nameW = (int) (40.0f * s);
    {
        juce::GlyphArrangement ga;
        ga.addFittedText (nameFont, v.title, textBounds.getX(), textBounds.getY(),
                          textBounds.getWidth(), nameH, juce::Justification::centred, 1);
        g.setColour (titleCol);
        ga.draw (g);
        nameW = juce::jmax (nameW, (int) ga.getBoundingBox (0, -1, true).getWidth());
    }

    // Manufacturer / sottotitolo: subito sotto il titolo, allineato al suo bordo sx.
    if (v.subtitle.isNotEmpty())
    {
        juce::Rectangle<int> manuRect ((int) (box.getCentreX() - (float) nameW * 0.5f),
                                       (int) (box.getCentreY() + 4.0f * s),
                                       nameW, (int) (20.0f * s));
        g.setColour (titleCol);
        g.setFont (idFont);
        g.drawFittedText (v.subtitle, manuRect, juce::Justification::bottomLeft, true);
    }

    // "ID" + identificativo: colonna a destra di box.reduced(16), giustificata in
    // basso (come il reale). idText derivato dallo uuid dal canvas.
    if (v.idText.isNotEmpty())
    {
        auto idCol  = box.reduced (16.0f * s).removeFromRight (30.0f * s);
        auto numCol = idCol.removeFromRight (20.0f * s);
        idCol.removeFromBottom (1.0f * s);

        g.setColour (titleCol);
        g.setFont (idFont);
        g.drawFittedText ("ID", idCol.toNearestInt(), juce::Justification::bottomRight, true);
        g.setFont (nameFont);
        g.drawFittedText (v.idText, numCol.toNearestInt(), juce::Justification::centredBottom, true);
    }

    // Bottone REMOVE (cestino) + outline, accanto al titolo (a destra del nome).
    if (v.canRemove)
    {
        juce::Rectangle<float> removeRect (textBounds.getCentreX() + (float) nameW * 0.5f + 8.0f * s,
                                           textBounds.getY() - 8.0f * s, 8.0f * s, 8.0f * s);
        auto rA = removeRect.expanded (8.0f * s);
        rA.setTop (rA.getY() + 1.0f * s);
        juce::Path c;
        g.setColour (titleCol);
        if (v.hovered) { gl::drawCirclePortion (c, rA, 0, 10);  gl::drawCirclePortion (c, rA, 90, 100); gl::drawCirclePortion (c, rA, 40, 60); }
        else           { gl::drawCirclePortion (c, rA, 80, 100); gl::drawCirclePortion (c, rA, 30, 50); }
        g.strokePath (c, juce::PathStrokeType (1.0f * s));
        auto rE = rA.reduced (4.0f * s);
        g.drawEllipse (rE.getX(), rE.getY() - 1.0f * s, rE.getWidth(), rE.getHeight(), 0.5f * s);
        gl::drawGlyph (g, gl::basket, removeRect, grey);
    }

    // HUD corner-brackets in hover (box.reduced(16/24)): L top-right + L bottom-left,
    // 1:1 con il NodeComponent reale (colore titolo, stroke 1).
    if (v.hovered)
    {
        const auto  hr  = box.reduced ((v.pressed ? 24.0f : 16.0f) * s);
        const float arm = 12.0f * s;
        juce::Path path;
        path.startNewSubPath (hr.getTopRight());
        path.lineTo          (hr.getTopRight().translated (0.0f, arm));
        path.startNewSubPath (hr.getTopRight());
        path.lineTo          (hr.getTopRight().translated (-arm, 0.0f));
        path.startNewSubPath (hr.getBottomLeft());
        path.lineTo          (hr.getBottomLeft().translated (arm, 0.0f));
        path.startNewSubPath (hr.getBottomLeft());
        path.lineTo          (hr.getBottomLeft().translated (0.0f, -arm));
        g.setColour (titleCol);
        g.strokePath (path, juce::PathStrokeType (1.0f * s));
    }

    //--- DECORI "circuito" (sotto i bottoni), colore titolo, 1:1 col paint reale --
    g.setColour (titleCol);
    const juce::PathStrokeType deco (1.0f * s);

    // Collegamento + outline del logo formato (basso a dx).
    { juce::Path p; p.startNewSubPath (P (nW - 24, nH - 24)); p.lineTo (P (nW - 28, nH - 28)); g.strokePath (p, deco); }
    {
        const auto la = RECT (nW - 26, nH - 26, 24, 24);
        juce::Path c;
        if (v.hovered) { gl::drawCirclePortion (c, la, 40, 60); gl::drawCirclePortion (c, la, 0, 10); gl::drawCirclePortion (c, la, 90, 100); }
        else           { gl::drawCirclePortion (c, la, 20, 40); gl::drawCirclePortion (c, la, 70, 90); }
        g.strokePath (c, deco);
        const auto le = RECT (nW - 23, nH - 23, 18, 18);
        g.drawEllipse (le, 0.5f * s);
    }
    // Collegamento CPU (basso a sx).
    { juce::Path p; p.startNewSubPath (P (26, nH - 26)); p.lineTo (P (28, nH - 28)); g.strokePath (p, deco); }

    // Outline shutdown (anello attorno al bottone shutdown).
    {
        const auto sa = RECT (29, 29, 28, 28);
        juce::Path c;
        if (v.hovered) { gl::drawCirclePortion (c, sa.reduced (2.0f * s), 0, 10); gl::drawCirclePortion (c, sa.reduced (2.0f * s), 90, 100); gl::drawCirclePortion (c, sa.reduced (2.0f * s), 40, 60); }
        else           { gl::drawCirclePortion (c, sa.reduced (2.0f * s), 0, 20); gl::drawCirclePortion (c, sa.reduced (2.0f * s), 50, 70); }
        g.strokePath (c, deco);
        g.drawEllipse (sa.reduced (5.0f * s), 0.5f * s);
    }
    // Outline bypass (solo se non IO).
    if (! isIO)
    {
        auto ba = RECT (57, 29, 18, 18);
        juce::Path c;
        if (v.hovered) { gl::drawCirclePortion (c, ba.withX (ba.getX() + 2.0f * s), 75, 95); }
        else           { gl::drawCirclePortion (c, ba, 0, 10); gl::drawCirclePortion (c, ba, 90, 100); }
        g.strokePath (c, deco);
        auto be = ba.reduced (2.0f * s);
        g.drawEllipse (be.getX(), be.getY() - 1.0f * s, be.getWidth(), be.getHeight(), 0.5f * s);
        if (v.hovered) g.drawLine (be.getRight() + 4.0f * s, be.getCentreY(), be.getRight() + be.getWidth() * 0.5f + 4.0f * s, be.getCentreY(), 1.0f * s);
        else           g.drawLine (be.getRight(), be.getBottom(), be.getRight() + be.getWidth() * 0.35f, be.getBottom() + be.getHeight() * 0.35f, 1.0f * s);
    }

    // Linee in alto a dx (1:1 col NodeComponent, griglia 2x2): parameters list /
    // panic (colonna sx) + settings / params (decori polilinea).
    if (! isIO && v.hasParams) { juce::Path p; p.startNewSubPath (P (nW - 30, 18)); p.lineTo (P (nW - 24, 12)); g.strokePath (p, deco); }
    if (v.acceptsMidi)         { juce::Path p; p.startNewSubPath (P (nW - 30, 33)); p.lineTo (P (nW - 24, 27)); g.strokePath (p, deco); }
    { juce::Path p; p.startNewSubPath (P (nW - 26, 31)); p.lineTo (P (nW - 20, 25)); p.lineTo (P (nW - 14, 25)); p.lineTo (P (nW - 10, 22)); g.strokePath (p, deco); }
    { juce::Path p; p.startNewSubPath (P (nW - 24, 35)); p.lineTo (P (nW - 19, 30)); p.lineTo (P (nW - 13, 30)); g.strokePath (p, deco); }

    // Linee in alto a sx: oversample / buffer / compensation / window.
    if (! isIO && hasAudio) { juce::Path p; p.startNewSubPath (P (12, 30)); p.lineTo (P (18, 30)); p.lineTo (P (24, 34)); g.strokePath (p, deco); }
    if (! isIO && hasAudio) { juce::Path p; p.startNewSubPath (P (12, 16)); p.lineTo (P (18, 16)); p.lineTo (P (20, 18)); p.lineTo (P (20, 24)); p.lineTo (P (28, 30)); g.strokePath (p, deco); }
    if (! v.isInputNode)    { juce::Path p; p.startNewSubPath (P (23, 12)); p.lineTo (P (25, 14)); p.lineTo (P (25, 22)); p.lineTo (P (31, 27)); g.strokePath (p, deco); }
    if (v.hasEditor)        { juce::Path p; p.startNewSubPath (P (38, 14)); p.lineTo (P (38, 18)); p.lineTo (P (34, 21)); p.lineTo (P (34, 26)); g.strokePath (p, deco); }

    // Readout latenza (in alto, font nodeID 5px), se fornito dall'app.
    if (v.latencyText.isNotEmpty())
    {
        g.setColour (titleCol);
        g.setFont (ayra::tokens::type::nodeId().withHeight (ayra::tokens::type::kNodeIdSize * s));
        g.drawFittedText (v.latencyText, v.bounds.reduced (20.0f * s).toNearestInt(),
                          juce::Justification::centredTop, true);
    }

    //--- BOTTONI (glyph sopra i decori). Posizioni E visibilita' dalla fonte di
    //    verita' unica condivisa col hit-test del canvas (ayra_NodeButtons.h).
    const auto B = computeNodeButtonLayout (v.bounds, s);
    const NodeButtonFlags fl { v.isInputNode, v.isOutputNode, hasAudio, v.acceptsMidi, v.hasEditor, v.hasParams, v.canRemove };
    auto vis = [&] (NodeButton id) { return isNodeButtonVisible (id, fl); };

    // Il power mantiene la stessa tinta semantica su glifo, chip e contorno.
    const auto powerColour = nodePowerColour (v.shutdown);
    const auto powerChip = B.shutdown.expanded (2.0f * s);
    g.setColour (tokens::powerBackground (powerColour));
    g.fillRoundedRectangle (powerChip, 2.5f * s);
    g.setColour (tokens::powerOutline (powerColour));
    g.drawRoundedRectangle (powerChip, 2.5f * s, 1.0f);
    gl::drawGlyph (g, gl::shutdown, B.shutdown, powerColour);

    // Gli altri flag mantengono il colore della tipologia solo nello stato ON.
    if (vis (NodeButton::bypass))     gl::drawGlyph (g, gl::bypass,        B.bypass,     v.bypassed   ? nodeButtonActiveColour (NodeButton::bypass)     : grey);
    if (vis (NodeButton::window))     gl::drawGlyph (g, gl::window,        B.window,     v.windowOpen ? nodeButtonActiveColour (NodeButton::window)     : grey);
    if (vis (NodeButton::latency))    gl::drawGlyph (g, gl::clock,         B.latency,    v.latencyComp? nodeButtonActiveColour (NodeButton::latency)    : grey);
    if (vis (NodeButton::buffer))     gl::drawGlyph (g, gl::stack,         B.buffer,     v.bufferized ? nodeButtonActiveColour (NodeButton::buffer)     : grey);
    if (vis (NodeButton::oversample)) gl::drawGlyph (g, gl::thunderCircle, B.oversample, v.oversampled? nodeButtonActiveColour (NodeButton::oversample) : grey);
    gl::drawGlyph (g, gl::settings, B.settings, grey);
    if (vis (NodeButton::panic))      gl::drawGlyph (g, gl::midiPanic,     B.panic,      grey);
    gl::drawGlyph (g, gl::sliders,  B.params, grey);

    // Logo formato (basso a dx): glyph in base a formatLabel ("" / "Internal" -> Ayra).
    {
        const auto fl = v.formatLabel;
        gl::Glyph fg = gl::ayraLogo;
        if      (fl == "VST3")      { fg = gl::vst3FormatLogo; }
        else if (fl == "VST")       { fg = gl::vst2FormatLogo; }
        else if (fl == "AudioUnit") { fg = gl::auFormatLogo;   }
        else if (fl == "LV2")       { fg = gl::lv2FormatLogo;  }
        else if (! fl.isEmpty() && fl != "Internal") { fg = gl::juceLogo; }
        gl::drawGlyph (g, fg, B.format, grey);
    }

    //--- CPU METER (basso a sx), replica di CpuMeterComponent — solo se alimentato.
    if (v.cpuLevel01 >= 0.0f)
    {
        const int pct = juce::jlimit (0, 100, juce::roundToInt (v.cpuLevel01 * 100.0f));
        auto area = RECT (0, nH - 40, 40, 40).reduced (4.0f * s);
        auto shapeArea = area.removeFromTop (area.getHeight() / 3.0f * 2.0f + 6.0f * s);
        shapeArea.removeFromRight (shapeArea.getWidth() / 3.0f);

        juce::Path ring; gl::drawCirclePortion (ring, shapeArea.reduced (1.0f * s), 10, 90);
        g.setColour (tk::node::chromeAlloy()); g.strokePath (ring, juce::PathStrokeType (0.5f * s));

        const int minV = 10, maxSafe = 85, maxV = 90;
        int lvl = juce::jmap (pct, 0, 100, minV, maxV); lvl = juce::jmin (lvl, maxV);
        juce::Path safe; gl::drawCirclePortion (safe, shapeArea.reduced (1.0f * s), minV, lvl);
        g.setColour (tk::node::neonSurge()); g.strokePath (safe, juce::PathStrokeType (2.0f * s));
        if (lvl >= maxSafe)
        {
            juce::Path un; gl::drawCirclePortion (un, shapeArea.reduced (1.0f * s), maxSafe, lvl);
            g.setColour (tk::node::quantumFlare()); g.strokePath (un, juce::PathStrokeType (2.0f * s));
        }

        gl::drawGlyph (g, gl::chip, shapeArea.reduced (4.0f * s),
                       pct >= 100 ? tk::node::quantumFlare() : titleCol);

        g.setColour (titleCol);
        g.setFont (ayra::tokens::type::nodeId().withHeight (ayra::tokens::type::kNodeIdSize * s));
        g.drawFittedText ("CPU :", juce::Rectangle<float> (area.getX(), area.getY(), 10.0f * s, 10.0f * s).toNearestInt(),
                          juce::Justification::topLeft, true);
        g.setFont (ayra::tokens::type::nodeDetail().withHeight (ayra::tokens::type::kNodeDetailSize * s));
        g.drawFittedText (juce::String (pct) + " %",
                          juce::Rectangle<float> (area.getX() + 12.0f * s, area.getY() - 2.0f * s, 20.0f * s, 10.0f * s).toNearestInt(),
                          juce::Justification::topRight, true);
    }
}

//==============================================================================
void CommonDefaultLookAndFeel::drawNodePin (juce::Graphics& g, const NodePinVisual& v)
{
    // Cerchio pieno colore stream + ring scuro 2px (shadowReactor), 1:1 con il
    // PinComponent reale. Lo slot del pin e' pinSize=20; il cerchio = slot-4 = 16.
    const float s = juce::jmax (0.25f, v.scale);
    const auto  c = v.centre;
    const float d = 16.0f * s;
    const auto  e = juce::Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d);

    g.setColour (v.tint);
    g.fillEllipse (e);
    g.setColour (tk::node::shadowReactor());
    g.drawEllipse (e, 2.0f * s);

    // Pin NON connesso: "buco" scuro (shadowReactor) offset DIAGONALE verso
    // l'esterno del bordo, esattamente come il PinComponent reale (reduced(3.5),
    // traslazione +/-1 su entrambi gli assi) -> a colpo d'occhio un pin libero si
    // distingue da uno collegato.
    if (! v.connected)
    {
        juce::Point<float> off = v.isInput ? (v.audio ? juce::Point<float> (-1.0f,  1.0f)
                                                       : juce::Point<float> ( 1.0f, -1.0f))
                                           : (v.audio ? juce::Point<float> ( 1.0f,  1.0f)
                                                       : juce::Point<float> (-1.0f,  1.0f));
        off *= s;

        const float hd = d - 7.0f * s;   // ellipse.reduced(3.5) -> diametro - 7.
        g.setColour (tk::node::shadowReactor());
        g.fillEllipse (c.x + off.x - hd * 0.5f, c.y + off.y - hd * 0.5f, hd, hd);
    }

    // VU per-pin (audio): arco-gauge ~300 gradi proporzionale al livello, turchese
    // "safe" (neonSurge) -> rosso "danger" (quantumFlare) oltre 0.9. Solo se
    // alimentato da uno snapshot RCU (provider metriche), mai da polling.
    if (v.level01 >= 0.0f)
    {
        const float r      = d * 0.5f + 3.0f * s;
        const float startA = juce::MathConstants<float>::pi * -0.8333f;   // ~ -150 gradi
        const float spanA  = juce::MathConstants<float>::pi *  1.6666f;   // ~  300 gradi
        const float endA   = startA + spanA * juce::jlimit (0.0f, 1.0f, v.level01);

        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, startA, endA, true);
        g.setColour (v.level01 >= 0.9f ? tk::node::quantumFlare() : tk::node::neonSurge());
        g.strokePath (arc, juce::PathStrokeType (2.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void CommonDefaultLookAndFeel::drawConnectionCable (juce::Graphics& g, juce::Point<float> p1,
                                                    juce::Point<float> p2, juce::Colour c,
                                                    bool dashed, bool verticalFlow)
{
    // Bezier come ConnectorComponent reale: audio = flusso verticale (control
    // point verticali), dati = flusso orizzontale. Stroke 2.5; dashed se sel.
    juce::Path line; line.startNewSubPath (p1);

    if (verticalFlow)
    {
        if (p2.y > p1.y)
        {
            const float dy = p2.y - p1.y;
            line.cubicTo (p1.x, p1.y + dy * 0.7f, p2.x, p1.y + dy * 0.3f, p2.x, p2.y);
        }
        else
        {
            const float dy = std::abs (p2.y - p1.y);
            line.cubicTo (p1.x + dy * 0.5f, p1.y + dy * 0.5f, p1.x + dy, (p1.y + p2.y) * 0.5f, (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            line.cubicTo (p2.x - dy, (p1.y + p2.y) * 0.5f, p2.x - dy * 0.5f, p2.y - dy * 0.5f, p2.x, p2.y);
        }
    }
    else
    {
        const bool cubic = (std::abs (p2.x - p1.x) + 1.0f >= std::abs (p2.y - p1.y)) && (p2.x > p1.x);
        if (cubic)
        {
            line.cubicTo (p1.x - (p1.x - p2.x) * 0.7f, p1.y, p1.x - (p1.x - p2.x) * 0.3f, p2.y, p2.x, p2.y);
        }
        else
        {
            const float dy = std::abs (p2.y - p1.y);
            line.cubicTo (p1.x + dy * 0.5f, p1.y, p1.x + dy * 0.5f, (p1.y + p2.y) * 0.5f, (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            line.cubicTo (p2.x - dy * 0.5f, (p1.y + p2.y) * 0.5f, p2.x - dy * 0.5f, p2.y, p2.x, p2.y);
        }
    }

    // Selezione = cavo piu' luminoso (brighter.brighter come ConnectorComponent reale).
    g.setColour (dashed ? c.brighter().brighter() : c);
    if (dashed)
    {
        // Il tratteggio richiede il path stroked esplicito.
        juce::PathStrokeType stroke (2.5f);
        const float dl[] = { 8.0f, 4.0f, 2.0f, 4.0f };
        juce::Path stroked;
        stroke.createDashedStroke (stroked, line, dl, 4);
        g.fillPath (stroked);
    }
    else
    {
        // Caso comune (ogni cavo, ogni frame): strokePath evita di materializzare
        // un Path intermedio -> una allocazione in meno per cavo per frame.
        g.strokePath (line, juce::PathStrokeType (2.5f));
    }
}

} // namespace ayra
