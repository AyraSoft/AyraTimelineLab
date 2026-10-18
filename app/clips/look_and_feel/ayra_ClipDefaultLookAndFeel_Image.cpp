/*
  ==============================================================================
   Ayra Timeline Lab / clips / default look and feel — IMAGE SEQUENCE CLIP
  ==============================================================================
*/

namespace ayra::lab::clips
{

namespace tk = ayra::tokens;

namespace
{

void drawImageClipBadge (juce::Graphics& g,
                         const juce::Rectangle<float> bounds,
                         const juce::String& text,
                         const juce::Colour tint)
{
    juce::Path pill;
    pill.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    g.setColour (juce::Colour (0xd80b171e));
    g.fillPath (pill);

    g.setColour (tint);
    g.strokePath (pill, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colours::white);
    g.setFont (tk::type::caption().boldened().withHeight (9.5f));
    g.drawText (text, bounds.toNearestInt(), juce::Justification::centred, false);
}

/** Disegna una miniatura procedurale stilizzata coerente col design system Ayra
    quando l'applicazione host non fornisce una bitmap `juce::Image` reale. */
void drawProceduralFramePreview (juce::Graphics& g,
                                 const juce::Rectangle<float> tileRect,
                                 const int sourceFrameIndex,
                                 const int seed,
                                 const juce::Colour tint,
                                 const float effectiveOpacity)
{
    const float alpha = juce::jlimit (0.08f, 1.0f, effectiveOpacity);

    // Fase animata deterministica in base all'indice del fotogramma sorgente
    const float phase = static_cast<float> ((sourceFrameIndex * 17 + seed * 31) % 360)
                      * (juce::MathConstants<float>::twoPi / 360.0f);

    // 1. Cielo / sfondo del fotogramma
    const auto skyTop = tint.darker (0.65f).withAlpha (0.85f * alpha);
    const auto skyBot = tint.brighter (0.15f).withAlpha (0.72f * alpha);
    {
        juce::ColourGradient skyGrad (skyTop, tileRect.getCentreX(), tileRect.getY(),
                                      skyBot, tileRect.getCentreX(), tileRect.getBottom(), false);
        g.setGradientFill (skyGrad);
        g.fillRect (tileRect);
    }

    // 2. Sole / Astro luminoso che si muove col numero di frame (rende visibile il Warp e lo Slip!)
    const float sunNormX = 0.25f + 0.50f * (0.5f + 0.5f * std::sin (phase * 0.7f));
    const float sunNormY = 0.34f + 0.14f * std::cos (phase);
    const float sunRadius = juce::jmin (tileRect.getWidth(), tileRect.getHeight()) * 0.16f;
    if (sunRadius >= 2.5f)
    {
        const float sx = tileRect.getX() + sunNormX * tileRect.getWidth();
        const float sy = tileRect.getY() + sunNormY * tileRect.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.78f * alpha));
        g.fillEllipse (sx - sunRadius, sy - sunRadius, sunRadius * 2.0f, sunRadius * 2.0f);
    }

    // 3. Profilo montuoso / geometrico del fotogramma
    const float h = tileRect.getHeight();
    const float w = tileRect.getWidth();
    const float yBase = tileRect.getBottom();
    const float yPeak1 = tileRect.getY() + h * (0.58f - 0.12f * std::sin (phase));
    const float yPeak2 = tileRect.getY() + h * (0.50f + 0.12f * std::cos (phase * 1.3f));

    juce::Path horizon;
    horizon.startNewSubPath (tileRect.getX(), yBase);
    horizon.lineTo (tileRect.getX(), tileRect.getY() + h * 0.72f);
    horizon.lineTo (tileRect.getX() + w * 0.35f, yPeak1);
    horizon.lineTo (tileRect.getX() + w * 0.68f, yPeak2);
    horizon.lineTo (tileRect.getRight(), tileRect.getY() + h * 0.66f);
    horizon.lineTo (tileRect.getRight(), yBase);
    horizon.closeSubPath();

    g.setColour (juce::Colour (0xff0b171e).withAlpha (0.68f * alpha));
    g.fillPath (horizon);
}

} // namespace

//==============================================================================
void ClipDefaultLookAndFeel::drawImageClip (
    juce::Graphics& g, const ImageClipVisual& v, ImageClipComponent& component)
{
    juce::ignoreUnused (component);

    if (v.bounds.getWidth() < 4.0f || v.bounds.getHeight() < 8.0f)
        return;

    const float corner = 4.0f;
    const auto  tint   = v.suspended ? v.tint.darker().darker() : v.tint;

    juce::Path bodyPath;
    bodyPath.addRoundedRectangle (v.bounds, corner);

    // 1. Fondo base della Clip in tinta (identico a drawAudioClip: emerge quando l'opacita' scende)
    const auto topCol = v.selected ? tint.interpolatedWith (juce::Colour (0xff0b1a24), 0.35f)
                                   : tint.withAlpha (0.92f);
    const auto botCol = v.selected ? tint.interpolatedWith (juce::Colour (0xff0b1a24), 0.55f)
                                   : tint.darker (0.25f).withAlpha (0.88f);
    {
        juce::ColourGradient grad (topCol, v.bounds.getX(), v.bounds.getY(),
                                   botCol, v.bounds.getX(), v.bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (bodyPath);
    }

    // Tutto il contenuto interno viene clippato al rettangolo arrotondato della clip
    {
        const juce::Graphics::ScopedSaveState clipScope (g);
        g.reduceClipRegion (bodyPath);

        const auto ink = v.selected ? tint.interpolatedWith (juce::Colour (0xffe2f6ef), 0.27f)
                                    : tint.interpolatedWith (juce::Colour (0xff0b171e), 0.82f);

        constexpr float kTitleAreaHeight = 16.0f;
        const float envelopeTop    = v.bounds.getY() + kTitleAreaHeight;
        const float envelopeHeight = juce::jmax (10.0f, v.bounds.getHeight() - kTitleAreaHeight);
        const juce::Rectangle<float> curveBounds (
            v.bounds.getX(), v.bounds.getBottom() - envelopeHeight,
            v.bounds.getWidth(), envelopeHeight);

        // 2. Filmstrip Fotogrammi A TUTTA ALTEZZA (Full-Bleed da top a bottom della clip)
        if (v.frames != nullptr && v.numFrames > 0)
        {
            for (int i = 0; i < v.numFrames; ++i)
            {
                const auto& fr = v.frames[i];
                if (fr.endNormX <= 0.0f || fr.startNormX >= 1.0f)
                    continue;

                const float x0 = v.bounds.getX() + juce::jlimit (0.0f, 1.0f, fr.startNormX) * v.bounds.getWidth();
                const float x1 = v.bounds.getX() + juce::jlimit (0.0f, 1.0f, fr.endNormX)   * v.bounds.getWidth();
                const float tileW = std::max (1.0f, x1 - x0);
                const juce::Rectangle<float> tileRect (x0, v.bounds.getY(), tileW, v.bounds.getHeight());

                // Valuta l'opacita' al centro del fotogramma per modulare in tempo reale la visibilita'
                const float midNormX = juce::jlimit (0.0f, 1.0f, (fr.startNormX + fr.endNormX) * 0.5f);
                const float frameOpacity = clip_opacity_law::evaluateOpacityAt (
                    v.opacityPoints, v.numOpacityPoints, v.baseOpacity, midNormX,
                    v.fadeInControl, v.fadeOutControl);

                if (fr.thumbnail.isValid())
                {
                    const juce::Graphics::ScopedSaveState tileClip (g);
                    g.reduceClipRegion (tileRect.toNearestInt());
                    g.setOpacity (juce::jlimit (0.08f, 1.0f, frameOpacity));
                    g.drawImage (fr.thumbnail, tileRect,
                                 juce::RectanglePlacement::fillDestination);
                    g.setOpacity (1.0f);

                    // Leggero glaze cromatico per legare la foto al colore della clip
                    g.setColour (tint.withAlpha (v.selected ? 0.10f : 0.16f));
                    g.fillRect (tileRect);
                }
                else
                {
                    drawProceduralFramePreview (g, tileRect, fr.sourceFrameIndex,
                                                v.visualSeed, tint, frameOpacity);
                }

                // Linea divisoria verticale tra un fotogramma e l'altro (in tinta!)
                if (i > 0 && x0 > v.bounds.getX() + 1.0f)
                {
                    g.setColour (tint.brighter (0.30f).withAlpha (0.55f));
                    g.drawVerticalLine (juce::roundToInt (x0), v.bounds.getY(), v.bounds.getBottom());
                }

                // Numero del fotogramma (#001) visibile quando il fotogramma e' abbastanza largo e alto
                if (tileW >= 34.0f && v.bounds.getHeight() >= 54.0f)
                {
                    const juce::String frmStr = "#" + juce::String (fr.sourceFrameIndex + 1).paddedLeft ('0', 3);
                    g.setColour (juce::Colours::white.withAlpha (0.72f));
                    g.setFont (tk::type::value().withHeight (9.0f));
                    g.drawText (frmStr,
                                juce::Rectangle<float> (x0 + 4.0f, v.bounds.getBottom() - 14.0f, tileW - 6.0f, 12.0f).toNearestInt(),
                                juce::Justification::centredLeft, false);
                }
            }
        }

        // 2b. GRADIENTE DI IDENTITA' CROMATICA SUPERIORE E INFERIORE (Vignettatura in Tinta)
        //     Sfuma il bordo alto e basso dei fotogrammi nel colore `tint` della clip,
        //     preservando l'identita' colore della traccia e garantendo contrasto al titolo!
        {
            const float topBleedH = juce::jmin (26.0f, v.bounds.getHeight() * 0.38f);
            juce::ColourGradient topBleed (topCol.withAlpha (0.92f), v.bounds.getX(), v.bounds.getY(),
                                           topCol.withAlpha (0.0f),  v.bounds.getX(), v.bounds.getY() + topBleedH, false);
            g.setGradientFill (topBleed);
            g.fillRect (v.bounds.getX(), v.bounds.getY(), v.bounds.getWidth(), topBleedH);

            const float botBleedH = juce::jmin (22.0f, v.bounds.getHeight() * 0.32f);
            juce::ColourGradient botBleed (botCol.withAlpha (0.0f),  v.bounds.getX(), v.bounds.getBottom() - botBleedH,
                                           botCol.withAlpha (0.90f), v.bounds.getX(), v.bounds.getBottom(), false);
            g.setGradientFill (botBleed);
            g.fillRect (v.bounds.getX(), v.bounds.getBottom() - botBleedH, v.bounds.getWidth(), botBleedH);
        }

        // Badge formato sequenza in basso a destra (es. "24 fps / 120 frm")
        if (v.sequenceFormatText.isNotEmpty() && v.bounds.getWidth() > 110.0f && v.bounds.getHeight() >= 46.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.82f));
            g.setFont (tk::type::caption().withHeight (10.0f));
            g.drawText (v.sequenceFormatText,
                        juce::Rectangle<float> (v.bounds.getRight() - 105.0f, v.bounds.getBottom() - 15.0f, 100.0f, 13.0f).toNearestInt(),
                        juce::Justification::centredRight);
        }

        // 3. Titolo della clip direttamente sui fotogrammi in TOP CENTER + badge scostamento stretch/slip
        const auto titleFont = tk::type::body().boldened().withHeight (12.0f);
        g.setFont (titleFont);
        const float titleW = juce::GlyphArrangement::getStringWidth (titleFont, v.title);

        const bool showStretchBadge = (v.isStretchActive || v.stretchDeltaFrames != 0);
        juce::String stretchStr;
        if (showStretchBadge)
        {
            if (v.stretchDeltaFrames > 0)
                stretchStr = "+" + juce::String (v.stretchDeltaFrames) + " frm";
            else if (v.stretchDeltaFrames < 0)
                stretchStr = juce::String (v.stretchDeltaFrames) + " frm";
            else
                stretchStr = "0 frm";
        }

        const auto badgeFont = tk::type::caption().boldened().withHeight (9.5f);
        const float badgeTextW = showStretchBadge ? juce::GlyphArrangement::getStringWidth (badgeFont, stretchStr) : 0.0f;
        const float badgeW = showStretchBadge ? (badgeTextW + 8.0f) : 0.0f;
        const float spacing = 6.0f;
        const float totalHeaderContentW = titleW + (showStretchBadge ? (spacing + badgeW) : 0.0f);

        const float startX = juce::jlimit (v.bounds.getX() + 6.0f,
                                           v.bounds.getRight() - totalHeaderContentW - 6.0f,
                                           v.bounds.getCentreX() - totalHeaderContentW * 0.5f);

        const juce::Rectangle<float> titleRect (startX, v.bounds.getY() + 1.0f, titleW, kTitleAreaHeight - 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawText (v.title, titleRect.translated (0.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft, true);
        g.setColour (juce::Colours::white);
        g.drawText (v.title, titleRect.toNearestInt(), juce::Justification::centredLeft, true);

        if (showStretchBadge)
        {
            const float badgeH = 12.0f;
            const float badgeY = v.bounds.getY() + (kTitleAreaHeight - badgeH) * 0.5f;
            const juce::Rectangle<float> badgeRect (startX + titleW + spacing, badgeY, badgeW, badgeH);
            g.setColour (juce::Colour (0xd0101820));
            g.fillRoundedRectangle (badgeRect, 2.5f);
            g.setColour (v.isStretchActive ? juce::Colours::white : juce::Colour (0xbbff9800));
            g.drawRoundedRectangle (badgeRect, 2.5f, 0.8f);
            g.setFont (badgeFont);
            g.setColour (v.isStretchActive ? juce::Colours::white : juce::Colour (0xffffe0b2));
            g.drawText (stretchStr, badgeRect.toNearestInt(), juce::Justification::centred);
        }

        // 4. Linee verticali dei Warp Markers interne alla clip + badge scostamento in Fotogrammi ("+4 frm")
        if (v.features.showWarpRail && v.warpMarkersNorm != nullptr)
        {
            for (int i = 0; i < v.numWarpMarkers; ++i)
            {
                const float curNorm = v.warpMarkersNorm[i];
                if (curNorm < -0.001f || curNorm > 1.001f) continue;

                const float wx = v.bounds.getX() + curNorm * v.bounds.getWidth();
                const bool mHov = (v.hoveredZone == TimelineClipZone::warpMarker && v.hoveredMarkerIdx == i);
                g.setColour (mHov ? juce::Colours::white : juce::Colour (0xffff9800).withAlpha (0.88f));
                g.drawLine (wx, v.bounds.getY(), wx, v.bounds.getBottom(), mHov ? 1.8f : 1.2f);

                const float origNorm = (v.origWarpMarkersNorm != nullptr && i < v.numWarpMarkers)
                                     ? v.origWarpMarkersNorm[i]
                                     : (static_cast<float> (i + 1) / static_cast<float> (v.numWarpMarkers + 1));
                const float deltaNorm = curNorm - origNorm;

                juce::String dispStr = "0 frm";
                if (v.clipLengthSec > 0.0 && v.frameRate > 0.0f)
                {
                    const double deltaSec = static_cast<double> (deltaNorm) * v.clipLengthSec;
                    const std::int64_t deltaFrm = static_cast<std::int64_t> (std::round (deltaSec * static_cast<double> (v.frameRate)));
                    if (deltaFrm != 0)
                        dispStr = (deltaFrm > 0 ? "+" : "") + juce::String (deltaFrm) + " frm";
                }

                if (v.features.showDisplacementBadges && v.bounds.getHeight() >= 36.0f)
                {
                    g.setFont (badgeFont);
                    const float textW = juce::GlyphArrangement::getStringWidth (badgeFont, dispStr);
                    const float bW = textW + 8.0f;
                    const float bH = 14.0f;
                    const float bY = v.bounds.getBottom() - bH - 3.0f;

                    const bool hasSpaceOnRight = (wx + bW + 3.0f <= v.bounds.getRight());
                    float bX = hasSpaceOnRight ? (wx + 3.0f) : (wx - bW - 3.0f);
                    bX = juce::jlimit (v.bounds.getX() + 2.0f, v.bounds.getRight() - bW - 2.0f, bX);

                    const juce::Rectangle<float> badgeRect (bX, bY, bW, bH);
                    g.setColour (juce::Colour (0xd0101820));
                    g.fillRoundedRectangle (badgeRect, 2.5f);
                    g.setColour (mHov ? juce::Colours::white : juce::Colour (0xbbff9800));
                    g.drawRoundedRectangle (badgeRect, 2.5f, 0.8f);
                    g.setColour (mHov ? juce::Colours::white : juce::Colour (0xffffe0b2));
                    g.drawText (dispStr, badgeRect.toNearestInt(), juce::Justification::centred);
                }
            }
        }

        // 5. Linea dell'Opacita' continua (0% .. 100%) — visibile a zoom medio e grande (showGainEnvelope)
        //    ZERO allocazioni heap durante il paint (buffer inline su stack).
        if (v.features.showGainEnvelope)
        {
            auto opacityToY = [&] (float op) -> float
            {
                const float norm = clip_opacity_law::opacityToNorm (op);
                return v.bounds.getBottom() - norm * envelopeHeight;
            };

            struct EnvNode
            {
                float origX;
                float renderX;
                float y;
                float opacity;
                float tension;
                float controlX;
                float controlY;
                float controlWeight;
                int ptIdx;
            };

            juce::Array<EnvNode, juce::DummyCriticalSection, 32> nodes;
            if (v.opacityPoints != nullptr && v.numOpacityPoints >= 2)
            {
                for (int p = 0; p < v.numOpacityPoints; ++p)
                {
                    const float normX  = juce::jlimit (0.0f, 1.0f, v.opacityPoints[p].normX);
                    const float exactX = v.bounds.getX() + normX * v.bounds.getWidth();
                    const float rendX = automationPointRenderX (
                        curveBounds, p, v.numOpacityPoints, normX);

                    nodes.add ({ exactX, rendX, opacityToY (v.opacityPoints[p].opacity),
                                 v.opacityPoints[p].opacity, v.opacityPoints[p].tension,
                                 v.opacityPoints[p].controlX,
                                 v.opacityPoints[p].controlY,
                                 v.opacityPoints[p].controlWeight, p });
                }
            }
            else
            {
                const float baseY = opacityToY (v.baseOpacity);
                nodes.add ({ v.bounds.getX(), automationPointRenderX (
                                 curveBounds, 0, 2, 0.0f),
                             baseY, v.baseOpacity, 0.5f, -1.0f, -1.0f,
                             -1.0f, 0 });
                nodes.add ({ v.bounds.getRight(), automationPointRenderX (
                                 curveBounds, 1, 2, 1.0f),
                             baseY, v.baseOpacity, 0.5f, -1.0f, -1.0f,
                             -1.0f, 1 });
            }

            struct SegmentCp
            {
                juce::Point<float> cp;
                int segIdx;
            };
            juce::Array<SegmentCp, juce::DummyCriticalSection, 32> segCps;
            juce::Point<float> fadeInCp;
            juce::Point<float> fadeOutCp;
            ayra::curves::CurveBoundarySpec boundary;
            boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
            boundary.fadeInControl = v.fadeInControl;
            boundary.fadeOutControl = v.fadeOutControl;
            const auto boundaryGeometry = computeProjectedAutomationBoundaryGeometry (
                curveBounds, v.opacityPoints, v.numOpacityPoints, boundary,
                [] (const OpacityPoint& point) noexcept
                {
                    return opacityPointAsAutomationPoint (point);
                });
            const bool hasFadeIn = boundaryGeometry.hasFadeIn;
            const bool hasFadeOut = boundaryGeometry.hasFadeOut;

            juce::Path envLine;
            if (hasFadeIn)
            {
                envLine.startNewSubPath (boundaryGeometry.fadeIn.start);
                fadeInCp = boundaryGeometry.fadeIn.control;
                appendBoundedCurvePixelSegment (
                    envLine, boundaryGeometry.fadeIn);
            }
            else
            {
                envLine.startNewSubPath (nodes.getFirst().renderX, nodes.getFirst().y);
            }

            for (int s = 0; s + 1 < nodes.size(); ++s)
            {
                const auto& n0 = nodes.getReference (s);
                const auto& n1 = nodes.getReference (s + 1);
                const AutomationPoint start {
                    (n0.origX - v.bounds.getX()) / v.bounds.getWidth(),
                    n0.opacity, n0.tension, false, n0.controlX, n0.controlY,
                    n0.controlWeight };
                const AutomationPoint end {
                    (n1.origX - v.bounds.getX()) / v.bounds.getWidth(),
                    n1.opacity, n1.tension, false, n1.controlX, n1.controlY,
                    n1.controlWeight };
                const auto cp = automationControlPixelPosition (
                    curveBounds, start, end, s, nodes.size());
                segCps.add ({ cp, s });
                const ayra::curves::QuadraticControl control =
                    hasExplicitAutomationControl (start)
                        ? ayra::curves::QuadraticControl {
                              start.controlX, start.controlY,
                              start.controlWeight }
                        : ayra::curves::QuadraticControl {
                              0.5, start.tension };
                appendBoundedCurvePixelSegment (
                    envLine,
                    makeBoundedCurvePixelSegment (
                        { n0.renderX, n0.y }, { n1.renderX, n1.y },
                        control));
            }

            if (hasFadeOut)
            {
                fadeOutCp = boundaryGeometry.fadeOut.control;
                appendBoundedCurvePixelSegment (
                    envLine, boundaryGeometry.fadeOut);
            }

            // A) Sfumatura morbida sotto la linea di opacita' + Velo in tinta SOPRA la linea (dove l'opacita' e' ridotta)
            {
                juce::Path envTopMask = envLine;
                envTopMask.lineTo (v.bounds.getRight(), envelopeTop);
                envTopMask.lineTo (v.bounds.getX(), envelopeTop);
                envTopMask.closeSubPath();
                g.setColour (ink.withAlpha (0.42f));
                g.fillPath (envTopMask);

                juce::Path envBottomShade = envLine;
                envBottomShade.lineTo (v.bounds.getRight(), v.bounds.getBottom());
                envBottomShade.lineTo (v.bounds.getX(), v.bounds.getBottom());
                envBottomShade.closeSubPath();
                juce::ColourGradient shadeGrad (juce::Colour (0x10000000), v.bounds.getX(), v.bounds.getY(),
                                                juce::Colour (0x45000000), v.bounds.getX(), v.bounds.getBottom(), false);
                g.setGradientFill (shadeGrad);
                g.fillPath (envBottomShade);
            }

            // B) Tratteggio verticale dal nodo fino al fondo (per drag orizzontale dei Fade)
            for (const auto& nd : nodes)
            {
                const bool isHov = (v.hoveredZone == TimelineClipZone::gainEnvelopePoint && v.hoveredOpacityPointIdx == nd.ptIdx);
                const bool isDashedHov = (v.hoveredZone == TimelineClipZone::gainPointDashedLine && v.hoveredOpacityPointIdx == nd.ptIdx);
                const bool isSelected = (v.opacityPoints != nullptr && nd.ptIdx < v.numOpacityPoints && v.opacityPoints[nd.ptIdx].selected);
                const bool active = isHov || isDashedHov || isSelected;

                g.setColour (active ? tk::accent() : juce::Colours::white.withAlpha (0.65f));
                float cy = nd.y + 2.0f;
                const float yEndMax = v.bounds.getBottom() - 1.0f;
                while (cy < yEndMax)
                {
                    const float yEnd = std::min (cy + 4.0f, yEndMax);
                    if (active)
                        g.drawLine (nd.renderX, cy, nd.renderX, yEnd, 1.8f);
                    else
                        g.drawVerticalLine (juce::roundToInt (nd.renderX), cy, yEnd);
                    cy += 7.0f;
                }
            }

            // C) Disegno continuo della linea di Opacita' (alone scuro + linea crisp bianca)
            g.setColour (juce::Colour (0x65000000));
            g.strokePath (envLine, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.strokePath (envLine, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // D) Handle di curvatura (pallini vuoti Multiverse)
            for (const auto& sc : segCps)
            {
                const bool isHovered = (v.hoveredZone == TimelineClipZone::gainSegmentCurveHandle && v.hoveredCurveSegmentIdx == sc.segIdx);
                const auto tintCol = tk::accent().brighter (0.25f);
                const float r = isHovered ? 4.5f : 3.5f;
                const juce::Rectangle<float> dot (sc.cp.x - r, sc.cp.y - r, 2.0f * r, 2.0f * r);
                if (isHovered)
                {
                    g.setColour (tintCol.withAlpha (0.35f));
                    g.fillEllipse (dot.expanded (3.0f));
                    g.setColour (tintCol);
                    g.fillEllipse (dot);
                }
                g.setColour (tintCol);
                g.drawEllipse (dot, 1.4f);
            }

            const auto drawFadeControl = [&] (const juce::Point<float> cp,
                                               const TimelineClipZone zone)
            {
                const bool hovered = v.hoveredZone == zone;
                const auto colour = tk::accent().brighter (0.25f);
                const float radius = hovered ? 4.5f : 3.5f;
                const auto dot = juce::Rectangle<float> (
                    cp.x - radius, cp.y - radius, radius * 2.0f, radius * 2.0f);
                if (hovered)
                {
                    g.setColour (colour.withAlpha (0.35f));
                    g.fillEllipse (dot.expanded (3.0f));
                }
                g.setColour (colour);
                g.drawEllipse (dot, 1.4f);
            };
            if (hasFadeIn)  drawFadeControl (fadeInCp, TimelineClipZone::fadeInCurveHandle);
            if (hasFadeOut) drawFadeControl (fadeOutCp, TimelineClipZone::fadeOutCurveHandle);

            // E) Handle Punti di Opacita' + badge percentuale ("85%")
            for (const auto& nd : nodes)
            {
                const bool pHov = (v.hoveredZone == TimelineClipZone::gainEnvelopePoint && v.hoveredOpacityPointIdx == nd.ptIdx);
                const bool isDashedHov = (v.hoveredZone == TimelineClipZone::gainPointDashedLine && v.hoveredOpacityPointIdx == nd.ptIdx);
                const bool isSelected = (v.opacityPoints != nullptr && nd.ptIdx < v.numOpacityPoints && v.opacityPoints[nd.ptIdx].selected);
                const bool active = pHov || isDashedHov || isSelected;

                const auto fillCol = active ? tk::accent() : juce::Colours::white;
                const float r = active ? 5.5f : 4.0f;
                const juce::Rectangle<float> dot (nd.renderX - r, nd.y - r, 2.0f * r, 2.0f * r);

                if (isSelected)
                {
                    g.setColour (tk::accent().withAlpha (0.40f));
                    g.fillEllipse (dot.expanded (3.0f));
                }
                else if (active)
                {
                    g.setColour (fillCol.withAlpha (0.32f));
                    g.fillEllipse (dot.expanded (3.5f));
                }

                g.setColour (fillCol);
                g.fillEllipse (dot);
                g.setColour (isSelected ? juce::Colours::white : juce::Colour (0x60000000));
                g.drawEllipse (dot.reduced (0.5f), isSelected ? 1.5f : 1.0f);

                if (pHov || isDashedHov)
                {
                    const juce::String label = "Op: " + clip_opacity_law::formatOpacityPercentage (nd.opacity);
                    const float labelLeft = juce::jlimit (v.bounds.getX() + 4.0f, v.bounds.getRight() - 62.0f, nd.renderX - 29.0f);
                    const float labelTop  = juce::jmax (v.bounds.getY() + 18.0f, nd.y - 19.0f);
                    drawImageClipBadge (g, juce::Rectangle<float> (labelLeft, labelTop, 58.0f, 15.0f), label, tk::accent());
                }
            }

            if (v.isDraggingAllOpacity || (v.hoveredZone == TimelineClipZone::gainEnvelopeLine && v.baseOpacity < 0.995f))
            {
                const float cx = v.bounds.getCentreX();
                const float cy = opacityToY (v.baseOpacity);
                const juce::String badgeStr = clip_opacity_law::formatOpacityPercentage (v.baseOpacity)
                                            + (v.isDraggingAllOpacity ? " (Tutti)" : "");
                drawImageClipBadge (g, juce::Rectangle<float> (cx - 38.0f, cy - 20.0f, 76.0f, 16.0f), badgeStr, tk::accent());
            }
        }

        // Riga laser verticale bianca in hover sul taglio
        const bool cutHover = v.features.showCutRail && (v.hoveredZone == TimelineClipZone::bottomCutRail);
        if (cutHover && v.hoverMouseX >= v.bounds.getX() && v.hoverMouseX <= v.bounds.getRight())
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawVerticalLine (juce::roundToInt (v.hoverMouseX), v.bounds.getY(), v.bounds.getBottom());
        }
    }

    // 6. Bordo esterno della clip in tinta (1.5px se selezionata, 1.0px normale)
    g.setColour (v.selected ? tint : tint.interpolatedWith (juce::Colour (0xff101820), 0.45f));
    g.drawRoundedRectangle (v.bounds.reduced (0.5f), corner, v.selected ? 1.5f : 1.0f);

    // 7. Triangolini Warp Markers esterni superiori
    if (v.features.showWarpRail && v.warpMarkersNorm != nullptr)
    {
        for (int i = 0; i < v.numWarpMarkers; ++i)
        {
            const float curNorm = v.warpMarkersNorm[i];
            if (curNorm < -0.001f || curNorm > 1.001f) continue;

            const float wx = v.bounds.getX() + curNorm * v.bounds.getWidth();
            const bool mHov = (v.hoveredZone == TimelineClipZone::warpMarker && v.hoveredMarkerIdx == i);
            g.setColour (mHov ? juce::Colours::white : juce::Colour (0xffff9800));

            juce::Path tri;
            tri.addTriangle (wx - 5.0f, v.bounds.getY() - 7.5f,
                             wx + 5.0f, v.bounds.getY() - 7.5f,
                             wx,        v.bounds.getY());
            g.fillPath (tri);
            g.setColour (juce::Colour (0x65000000));
            g.strokePath (tri, juce::PathStrokeType (1.0f));
        }
    }

    // 7b. Indicatore "+" per aggiunta nuovo Warp Marker
    if (v.features.showWarpRail && v.showWarpAddGhost && v.warpAddGhostX >= v.bounds.getX() && v.warpAddGhostX <= v.bounds.getRight())
    {
        const float gx = v.warpAddGhostX;
        const float gy = v.bounds.getY() - 4.0f;
        const juce::Rectangle<float> btn (gx - 4.0f, gy - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xeeff9800));
        g.fillEllipse (btn);
        g.setColour (juce::Colour (0x60000000));
        g.drawEllipse (btn, 0.8f);
        g.setColour (juce::Colours::white);
        g.setFont (tk::type::caption().boldened().withHeight (8.5f));
        g.drawText ("+", btn.toNearestInt(), juce::Justification::centred);
    }

    // 8. Barra inferiore di taglio (Cut Rail)
    if (v.features.showCutRail)
    {
        const auto bottomBar = juce::Rectangle<float> (v.bounds.getX(), v.bounds.getBottom() + 2.0f,
                                                       v.bounds.getWidth(), 4.0f);
        const bool cutHover = (v.hoveredZone == TimelineClipZone::bottomCutRail);
        g.setColour (cutHover ? juce::Colours::white : tint.brighter (0.45f).withAlpha (0.95f));
        g.fillRoundedRectangle (bottomBar, 1.5f);
    }

    // 9. Parentesi HUD laterali (Loop / Slip / Trim / Stretch)
    clips::drawClipEdgeBracket (g, v.bounds, v.hoveredZone);
}

} // namespace ayra::lab::clips
