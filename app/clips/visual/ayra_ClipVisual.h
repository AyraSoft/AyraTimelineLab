#pragma once

namespace ayra::lab::clips
{

inline constexpr float clipTitleAreaHeight = 16.0f;
inline constexpr float audioClipTitleAreaHeight = clipTitleAreaHeight;

/** Zone semantiche dell'interazione clip.
    La geometria e il canvas usano lo stesso enum per paint e hit-test, cosi'
    l'aspetto hover non puo' divergere dalla gesture attiva. */
enum class TimelineClipZone
{
    none,
    clipBody,
    laneBackground,
    clipTitleMove,
    topWarpRail,
    warpMarker,
    bottomCutRail,
    gainHandle,
    gainEnvelopePoint,
    gainPointDashedLine,
    gainEnvelopeLine,
    gainSegmentCurveHandle,
    fadeInHandle,
    fadeInCurveHandle,
    fadeOutHandle,
    fadeOutCurveHandle,
    edgeLeftLoop,
    edgeLeftStretch,
    edgeLeftTrim,
    edgeLeftSlip,
    edgeRightLoop,
    edgeRightStretch,
    edgeRightTrim,
    edgeRightSlip,
    // --- Zone specifiche MIDI ---
    midiOverlayPill,
    midiProgramChangeFlag,
    midiNoteBody,
    midiNoteLeftEdge,
    midiNoteRightEdge,
    midiPitchScrollbar,
    midiVelocityStem,
    midiVelocityLane,
    keyframeBody,
    keyframeLeftEdge,
    keyframeRightEdge,
    keyframeCutRail
};

/** Intento semantico del cursore, indipendente dall'implementazione JUCE.
    La policy resta testabile senza confrontare handle nativi del mouse. */
enum class TimelineCursorRole
{
    normal,
    move,
    point,
    horizontalResize,
    verticalResize,
    twoDimensionalResize,
    cut,
    erase
};

/** Contesto minimo che specializza una zona senza duplicare la policy nei canvas. */
struct TimelineCursorContext
{
    TimelineClipZone zone = TimelineClipZone::none;
    bool altDown = false;
    bool shiftDown = false;
    bool canMoveHorizontally = true;
    bool canErase = false;
};

/** Risolve l'icona coerente con l'azione che un click sinistro eseguirebbe ora. */
[[nodiscard]] constexpr TimelineCursorRole resolveTimelineCursorRole (
    const TimelineCursorContext context) noexcept
{
    if (context.altDown && context.canErase)
        return TimelineCursorRole::erase;

    switch (context.zone)
    {
        case TimelineClipZone::clipTitleMove:          return TimelineCursorRole::move;
        case TimelineClipZone::topWarpRail:
        case TimelineClipZone::warpMarker:
        case TimelineClipZone::midiOverlayPill:
        case TimelineClipZone::midiProgramChangeFlag: return TimelineCursorRole::point;
        case TimelineClipZone::bottomCutRail:
        case TimelineClipZone::keyframeCutRail:        return TimelineCursorRole::cut;
        case TimelineClipZone::gainEnvelopeLine:
        case TimelineClipZone::gainHandle:
        case TimelineClipZone::midiPitchScrollbar:
        case TimelineClipZone::midiVelocityStem:
        case TimelineClipZone::midiVelocityLane:       return TimelineCursorRole::verticalResize;
        case TimelineClipZone::gainEnvelopePoint:
            return (! context.canMoveHorizontally || context.shiftDown)
                ? TimelineCursorRole::verticalResize
                : TimelineCursorRole::twoDimensionalResize;
        case TimelineClipZone::gainPointDashedLine:
            return context.canMoveHorizontally
                ? TimelineCursorRole::horizontalResize
                : TimelineCursorRole::verticalResize;
        case TimelineClipZone::gainSegmentCurveHandle:
        case TimelineClipZone::fadeInCurveHandle:
        case TimelineClipZone::fadeOutCurveHandle:     return TimelineCursorRole::twoDimensionalResize;
        case TimelineClipZone::edgeLeftLoop:
        case TimelineClipZone::edgeLeftStretch:
        case TimelineClipZone::edgeLeftTrim:
        case TimelineClipZone::edgeLeftSlip:
        case TimelineClipZone::edgeRightLoop:
        case TimelineClipZone::edgeRightStretch:
        case TimelineClipZone::edgeRightTrim:
        case TimelineClipZone::edgeRightSlip:
        case TimelineClipZone::midiNoteLeftEdge:
        case TimelineClipZone::midiNoteRightEdge:
        case TimelineClipZone::keyframeLeftEdge:
        case TimelineClipZone::keyframeRightEdge:      return TimelineCursorRole::horizontalResize;
        case TimelineClipZone::midiNoteBody:
        case TimelineClipZone::keyframeBody:           return context.shiftDown
                                                            ? TimelineCursorRole::horizontalResize
                                                            : TimelineCursorRole::move;
        default:                                       return TimelineCursorRole::normal;
    }
}

[[nodiscard]] inline juce::MouseCursor timelineMouseCursor (
    const TimelineCursorRole role)
{
    if (role == TimelineCursorRole::cut || role == TimelineCursorRole::erase)
    {
        static const juce::MouseCursor cutCursor = []
        {
            juce::Image image (juce::Image::ARGB, 24, 24, true);
            juce::Graphics g (image);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.drawLine (5.0f, 5.0f, 19.0f, 19.0f, 4.0f);
            g.drawLine (19.0f, 5.0f, 5.0f, 19.0f, 4.0f);
            g.setColour (juce::Colours::white);
            g.drawLine (5.0f, 5.0f, 19.0f, 19.0f, 1.5f);
            g.drawLine (19.0f, 5.0f, 5.0f, 19.0f, 1.5f);
            g.drawEllipse (2.0f, 1.5f, 7.0f, 7.0f, 1.5f);
            g.drawEllipse (15.0f, 1.5f, 7.0f, 7.0f, 1.5f);
            return juce::MouseCursor (image, 12, 12);
        }();
        static const juce::MouseCursor eraseCursor = []
        {
            juce::Image image (juce::Image::ARGB, 24, 24, true);
            juce::Graphics g (image);
            juce::Path eraser;
            eraser.addRoundedRectangle (
                juce::Rectangle<float> { 4.0f, 8.0f, 16.0f, 9.0f }, 2.0f);
            eraser.applyTransform (juce::AffineTransform::rotation (
                -0.55f, 12.0f, 12.0f));
            g.setColour (juce::Colours::black.withAlpha (0.9f));
            g.strokePath (eraser, juce::PathStrokeType (4.0f));
            g.setColour (juce::Colours::white);
            g.strokePath (eraser, juce::PathStrokeType (2.0f));
            return juce::MouseCursor (image, 7, 17);
        }();
        return role == TimelineCursorRole::cut ? cutCursor : eraseCursor;
    }

    switch (role)
    {
        case TimelineCursorRole::move:                 return juce::MouseCursor::DraggingHandCursor;
        case TimelineCursorRole::point:                return juce::MouseCursor::PointingHandCursor;
        case TimelineCursorRole::horizontalResize:     return juce::MouseCursor::LeftRightResizeCursor;
        case TimelineCursorRole::verticalResize:       return juce::MouseCursor::UpDownResizeCursor;
        case TimelineCursorRole::twoDimensionalResize: return juce::MouseCursor::UpDownLeftRightResizeCursor;
        default:                                       return juce::MouseCursor::NormalCursor;
    }
}

/** Solo l'header ammette la cancellazione rapida dell'intera clip.
    Corpo e feature interne mantengono le rispettive gesture. */
[[nodiscard]] constexpr bool isClipDeletionZone (
    const TimelineClipZone zone) noexcept
{
    return zone == TimelineClipZone::clipTitleMove;
}

/** Normalizza il vecchio sentinel `none` in una zona esplicita per callback. */
[[nodiscard]] constexpr TimelineClipZone semanticClipZone (
    const TimelineClipZone zone) noexcept
{
    return zone == TimelineClipZone::none ? TimelineClipZone::clipBody : zone;
}

/** Proiezione temporale del canvas demo (32 secondi visibili). */
[[nodiscard]] inline double timelineSecondsFromCanvasX (
    const float x, const int canvasWidth) noexcept
{
    return static_cast<double> (x) * 32.0
        / static_cast<double> (std::max (1, canvasWidth));
}

/** Verifica se la zona corrisponde a uno dei tool di bordo (Loop / Slip / Trim / Stretch). */
[[nodiscard]] inline bool isClipEdgeZone (const TimelineClipZone zone) noexcept
{
    switch (zone)
    {
        case TimelineClipZone::edgeLeftLoop:
        case TimelineClipZone::edgeLeftSlip:
        case TimelineClipZone::edgeLeftTrim:
        case TimelineClipZone::edgeLeftStretch:
        case TimelineClipZone::edgeRightLoop:
        case TimelineClipZone::edgeRightSlip:
        case TimelineClipZone::edgeRightTrim:
        case TimelineClipZone::edgeRightStretch:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] inline bool isLeftClipEdgeZone (const TimelineClipZone zone) noexcept
{
    return zone == TimelineClipZone::edgeLeftLoop
        || zone == TimelineClipZone::edgeLeftSlip
        || zone == TimelineClipZone::edgeLeftTrim
        || zone == TimelineClipZone::edgeLeftStretch;
}

[[nodiscard]] inline bool isRightClipEdgeZone (const TimelineClipZone zone) noexcept
{
    return zone == TimelineClipZone::edgeRightLoop
        || zone == TimelineClipZone::edgeRightSlip
        || zone == TimelineClipZone::edgeRightTrim
        || zone == TimelineClipZone::edgeRightStretch;
}

/** Risolve la zona semantica del bordo in 4 parti uguali di 1/4 (25% ciascuna):
    - [0.00 .. 0.25]: Loop (nuovo controllo sopra)
    - [0.25 .. 0.50]: Slip ("secondo trim", quello che prima era sopra)
    - [0.50 .. 0.75]: Trim ("primo trim", quello che prima era 2/4)
    - [0.75 .. 1.00]: Stretch / Warp (1/4 sotto) */
[[nodiscard]] inline TimelineClipZone computeClipEdgeZone (const bool isLeft, const float relY) noexcept
{
    if (relY <= 0.25f)
        return isLeft ? TimelineClipZone::edgeLeftLoop : TimelineClipZone::edgeRightLoop;
    if (relY <= 0.50f)
        return isLeft ? TimelineClipZone::edgeLeftSlip : TimelineClipZone::edgeRightSlip;
    if (relY <= 0.75f)
        return isLeft ? TimelineClipZone::edgeLeftTrim : TimelineClipZone::edgeRightTrim;
    return isLeft ? TimelineClipZone::edgeLeftStretch : TimelineClipZone::edgeRightStretch;
}

/** Stato transiente durante il drag di loop / duplicate sulle estremità della clip. */
struct ClipLoopDragState
{
    bool active = false;
    std::string clipId;
    bool isRightEdge = true;
    double originalStart = 0.0;
    double originalLength = 1.0;
    int duplicateCount = 0;
    double trimDelta = 0.0;
};

/** Calcola deterministicamente il numero di clip duplicate da creare o visualizzare in preview.
    - Se l'utente trascina verso l'interno della clip (delta <= 0 a destra o delta >= 0 a sinistra),
      restituisce 0 (comportamento di trim).
    - Se l'utente trascina verso l'esterno, restituisce ceil(distanza / originalLength),
      estendendo la fine per avere anche l'ultima clip completa (stile Cubase). */
[[nodiscard]] inline int computeLoopDuplicateCount (
    const bool isRightEdge,
    const double deltaSec,
    const double originalStart,
    const double originalLength) noexcept
{
    if (originalLength <= 0.001)
        return 0;

    if (isRightEdge)
    {
        if (deltaSec <= 0.0)
            return 0;
        return std::max (1, static_cast<int> (std::ceil (deltaSec / originalLength)));
    }
    else
    {
        if (deltaSec >= 0.0)
            return 0;
        const double dragDistance = -deltaSec;
        const int requested = std::max (1, static_cast<int> (std::ceil (dragDistance / originalLength)));
        const int maxAvailable = std::max (0, static_cast<int> (std::floor ((originalStart + 1.0e-6) / originalLength)));
        return std::min (requested, maxAvailable);
    }
}

/** Calcola la coordinata start in secondi per il duplicato zero-based `duplicateIndex`. */
[[nodiscard]] inline double computeLoopDuplicateStart (
    const bool isRightEdge,
    const int duplicateIndexZeroBased,
    const double originalStart,
    const double originalLength) noexcept
{
    const double offset = static_cast<double> (duplicateIndexZeroBased + 1) * originalLength;
    return isRightEdge ? (originalStart + offset) : (originalStart - offset);
}

/** Calcola i bounds pixel per il duplicato zero-based `duplicateIndex`. */
[[nodiscard]] inline juce::Rectangle<float> computeLoopDuplicateBounds (
    const juce::Rectangle<float>& originalClipBounds,
    const bool isRightEdge,
    const int duplicateIndexZeroBased) noexcept
{
    const float w = originalClipBounds.getWidth();
    const float offset = static_cast<float> (duplicateIndexZeroBased + 1) * w;
    const float x = isRightEdge ? (originalClipBounds.getX() + offset)
                                : (originalClipBounds.getX() - offset);
    return { x, originalClipBounds.getY(), w, originalClipBounds.getHeight() };
}

/** Disegna la parentesi HUD laterale per i 4 controlli di bordo (Loop / Slip / Trim / Stretch). */
inline void drawClipEdgeBracket (
    juce::Graphics& g,
    const juce::Rectangle<float>& bounds,
    const TimelineClipZone hz)
{
    if (! isClipEdgeZone (hz))
        return;

    namespace tk = ayra::tokens;

    const bool isLeft = isLeftClipEdgeZone (hz);
    const float edgeX = isLeft ? bounds.getX() : bounds.getRight();
    const float arm   = isLeft ? 2.5f : -2.5f;

    float y0 = bounds.getY(), y1 = bounds.getBottom();
    juce::Colour bracketCol = juce::Colours::white;

    if (hz == TimelineClipZone::edgeLeftLoop || hz == TimelineClipZone::edgeRightLoop)
    {
        y0 = bounds.getY() + 2.0f;
        y1 = bounds.getY() + bounds.getHeight() * 0.25f;
        bracketCol = tk::streamAudio(); // Verde vivo per Loop
    }
    else if (hz == TimelineClipZone::edgeLeftSlip || hz == TimelineClipZone::edgeRightSlip)
    {
        y0 = bounds.getY() + bounds.getHeight() * 0.25f;
        y1 = bounds.getY() + bounds.getHeight() * 0.50f;
        bracketCol = tk::streamVideo(); // Cyan per Slip ("secondo trim")
    }
    else if (hz == TimelineClipZone::edgeLeftTrim || hz == TimelineClipZone::edgeRightTrim)
    {
        y0 = bounds.getY() + bounds.getHeight() * 0.50f;
        y1 = bounds.getY() + bounds.getHeight() * 0.75f;
        bracketCol = juce::Colours::white; // Bianco per Trim normale ("primo trim")
    }
    else // Stretch / Warp
    {
        y0 = bounds.getY() + bounds.getHeight() * 0.75f;
        y1 = bounds.getBottom() - 2.0f;
        bracketCol = tk::accent(); // Ambra / Orange per Warp
    }

    juce::Path bracket;
    bracket.startNewSubPath (edgeX + arm, y0);
    bracket.lineTo          (edgeX,       y0);
    bracket.lineTo          (edgeX,       y1);
    bracket.lineTo          (edgeX + arm, y1);
    g.setColour (bracketCol);
    g.strokePath (bracket, juce::PathStrokeType (2.5f));
}

/** Calcola i bounds di un badge fluttuante centrato su un punto target,
    con padding dinamico e clamp ai confini del contenitore. */
[[nodiscard]] inline juce::Rectangle<float> computeClipBadgeBounds (
    const juce::Point<float> targetCentre,
    const juce::String& text,
    const juce::Rectangle<float>& containerBounds,
    const float minTopYOffset = 18.0f) noexcept
{
    namespace tk = ayra::tokens;
    const auto font = tk::type::caption().boldened().withHeight (9.5f);
    const float textW = juce::GlyphArrangement::getStringWidth (font, text);
    const float badgeW = juce::jmax (48.0f, textW + 16.0f);
    constexpr float badgeH = 15.0f;

    const float minX = containerBounds.getX() + 4.0f;
    const float maxX = juce::jmax (minX, containerBounds.getRight() - badgeW - 4.0f);
    const float minY = containerBounds.getY() + minTopYOffset;
    const float maxY = juce::jmax (minY, containerBounds.getBottom() - badgeH - 4.0f);

    const float bx = juce::jlimit (minX, maxX, targetCentre.x - badgeW * 0.5f);
    const float by = juce::jlimit (minY, maxY, targetCentre.y - badgeH * 0.5f);
    return { bx, by, badgeW, badgeH };
}

/** Disegna il badge pill fluttuante unificato (valore in tempo reale / feedback HUD). */
inline void drawClipBadge (
    juce::Graphics& g,
    const juce::Rectangle<float> bounds,
    const juce::String& text,
    const juce::Colour tint)
{
    namespace tk = ayra::tokens;

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

/** Disegna un badge fluttuante ancorato a un punto target con calcolo automatico dei bounds. */
inline void drawFloatingClipBadge (
    juce::Graphics& g,
    const juce::Point<float> targetCentre,
    const juce::String& text,
    const juce::Colour tint,
    const juce::Rectangle<float>& containerBounds,
    const float minTopYOffset = 18.0f)
{
    const auto rect = computeClipBadgeBounds (targetCentre, text, containerBounds, minTopYOffset);
    drawClipBadge (g, rect, text, tint);
}

/** Disegna il ghost preview di una clip duplicata durante la gesture di Loop drag. */
inline void drawLoopDuplicatePreview (
    juce::Graphics& g,
    const juce::Rectangle<float>& bounds,
    const juce::String& title,
    const int duplicateNumber,
    const bool isRightEdge,
    const juce::Colour tint,
    const juce::Path* waveformPath = nullptr)
{
    if (bounds.getWidth() < 4.0f || bounds.getHeight() < 8.0f)
        return;

    namespace tk = ayra::tokens;

    // 1. Fondo semitrasparente con gradiente soft
    const juce::Colour loopAccent = tk::streamAudio();
    juce::ColourGradient grad (
        loopAccent.withAlpha (0.22f), bounds.getX(), bounds.getY(),
        loopAccent.withAlpha (0.08f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 3.0f);

    // 2. Waveform ghost preview (se presente)
    if (waveformPath != nullptr)
    {
        const float waveformHeight = juce::jmax (10.0f, bounds.getHeight() - clipTitleAreaHeight);
        const auto transform = juce::AffineTransform::scale (
            bounds.getWidth() / 1000.0f, waveformHeight / 100.0f)
            .translated (bounds.getX(), bounds.getBottom() - waveformHeight);
        g.setColour (tint.withAlpha (0.35f));
        g.fillPath (*waveformPath, transform);
    }

    // 3. Contorno tratteggiato in verde loop
    juce::Path borderPath;
    borderPath.addRoundedRectangle (bounds.reduced (0.5f), 3.0f);
    juce::Path dashed;
    float dashes[] = { 5.0f, 3.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, borderPath, dashes, 2);
    g.setColour (loopAccent.withAlpha (0.85f));
    g.fillPath (dashed);

    // 4. Header badge con indicatore del duplicato
    if (bounds.getHeight() >= 24.0f && bounds.getWidth() >= 36.0f)
    {
        const float headerH = juce::jmin (16.0f, bounds.getHeight() * 0.35f);
        const auto headerRect = juce::Rectangle<float> (
            bounds.getX() + 2.0f, bounds.getY() + 2.0f,
            bounds.getWidth() - 4.0f, headerH);

        g.setColour (juce::Colour (0xd0101820));
        g.fillRoundedRectangle (headerRect, 2.5f);
        g.setColour (loopAccent.withAlpha (0.70f));
        g.drawRoundedRectangle (headerRect, 2.5f, 0.8f);

        const juce::String badge = (isRightEdge ? "[Loop +" : "[Loop -")
                                 + juce::String (duplicateNumber) + "]";
        const juce::String text = (bounds.getWidth() > 100.0f && title.isNotEmpty())
                                ? (title + "  " + badge) : badge;

        g.setFont (tk::type::caption().boldened().withHeight (juce::jmin (10.0f, headerH - 4.0f)));
        g.setColour (juce::Colours::white);
        g.drawText (text, headerRect.toNearestInt(), juce::Justification::centred, true);
    }

    // 5. Linea di demarcazione del bordo di ripetizione
    const float seamX = isRightEdge ? bounds.getX() : bounds.getRight();
    g.setColour (loopAccent.withAlpha (0.90f));
    g.drawVerticalLine (juce::roundToInt (seamX), bounds.getY(), bounds.getBottom());
}

/** Proiezione pixel di un segmento quadratico bounded. Paint e hit-test
    consumano la stessa geometria per evitare handle disallineati dal path. */
struct BoundedCurvePixelSegment
{
    juce::Point<float> start;
    juce::Point<float> control;
    juce::Point<float> end;
    ayra::curves::QuadraticControl shape;
};

[[nodiscard]] inline BoundedCurvePixelSegment makeBoundedCurvePixelSegment (
    const juce::Point<float> start,
    const juce::Point<float> end,
    const ayra::curves::QuadraticControl control) noexcept
{
    const auto projected = ayra::curves::boundedQuadraticControlPoint ({
        start.x, start.y, end.x, end.y, control });
    return {
        start,
        { static_cast<float> (projected.x), static_cast<float> (projected.y) },
        end,
        control
    };
}

/** Aggiunge al path la stessa conica bounded usata da valutazione e hit-test.

    `juce::Path::quadraticTo` non e' sufficiente: un Bezier quadratico ordinario
    non passa per il controllo e non puo' raggiungere il limite a gradino. La
    discretizzazione bounded e adattiva appartiene a `ayra_curves`, quindi i
    consumer non duplicano ne' la legge geometrica ne' la policy di sampling.
*/
inline void appendBoundedCurvePixelSegment (
    juce::Path& path,
    const BoundedCurvePixelSegment& segment)
{
    ayra::curves::forEachBoundedQuadraticPathPoint (
        segment.shape, [&path, &segment] (const auto point)
        {
            path.lineTo (
                segment.start.x + static_cast<float> (point.x)
                    * (segment.end.x - segment.start.x),
                segment.start.y + static_cast<float> (point.y)
                    * (segment.end.y - segment.start.y));
        });
}

/** Identificatore semantico di una feature / decorazione della clip.
    Ispirato al modello a proprieta' dei nodi di un grafo: ogni tipo di clip
    (Audio, MIDI, OSC, Automation) attiva solo il set pertinente. */
enum class ClipFeatureId : std::uint16_t
{
    header,
    bodyVisual,       // Waveform (Audio), PianoRoll (MIDI), Envelope (Automation)
    warpRail,         // Top rail con marker di warp
    gainEnvelope,     // Linea di gain (Audio) o Curva CC/Velocity (MIDI)
    fadeHandles,      // Triangolini e curve di fade in/out
    edgeTools,        // 3 tool di edge drag (stretch/trim/slip)
    cutRail           // Bottom rail per lo split tool
};

namespace clip_height_law
{
    /** Altezze canoniche condivise da tutte le clip (Single Source of Truth).
        Rappresentano la legge comune di dimensionamento verticale e Level of Detail (LOD):
        - compact:  96 px (preview / compatta)
        - medium:  256 px (editing principale / in-place)
        - full:    400 px (editing esteso / automazione / velocity) */
    static constexpr int compactTrackHeight = 96;
    static constexpr int mediumTrackHeight  = 256;
    static constexpr int fullTrackHeight    = 400;

    [[nodiscard]] inline int nextCanonicalHeight (const float currentVisualHeight) noexcept
    {
        if (currentVisualHeight < 150.0f) return mediumTrackHeight;
        if (currentVisualHeight < 320.0f) return fullTrackHeight;
        return compactTrackHeight;
    }

    [[nodiscard]] inline int nextModelHeight (const float currentVisualHeight, const float verticalZoom) noexcept
    {
        const float vZoom = std::max (0.01f, verticalZoom);
        return juce::roundToInt (static_cast<float> (nextCanonicalHeight (currentVisualHeight)) / vZoom);
    }

    /** Verifica se l'interazione del mouse avviene nell'header della clip o se la clip
        e' talmente compressa verticalmente (< 48px) che l'intera superficie risponde al
        doppio click di riapertura/espansione (eccetto bracket laterali estremi). */
    [[nodiscard]] inline bool isHeaderAreaOrClosedClip (
        const TimelineClipZone zone,
        const float clipVisualHeight,
        const int markerIdx = -1) noexcept
    {
        if (zone == TimelineClipZone::clipTitleMove) return true;
        if (zone == TimelineClipZone::topWarpRail && markerIdx < 0) return true;
        if (clipVisualHeight < 48.0f)
        {
            return ! isClipEdgeZone (zone);
        }
        return false;
    }
}

/** Slot geometrico in cui risiede la decorazione rispetto al corpo della clip. */
enum class DecorationSlot : std::uint8_t
{
    headerBand,       // Fascia in cima al corpo
    topRail,          // Overflow sopra la clip (es. triangolini warp)
    bodyOverlay,      // Disegnato sul corpo (es. linea gain, fade, CC)
    bottomRail,       // Overflow sotto la clip (es. barra di taglio)
    edgeLeft,         // Bordo sinistro
    edgeRight         // Bordo destro
};

/** Proprieta' e visibilita' dinamiche delle decorazioni per frame (LOD). */
struct ClipFeatures
{
    bool warpEnabled = true;
    bool gainEnabled = true;
    bool cutEnabled = true;
    bool fadesEnabled = true;
    bool edgeToolsEnabled = true;

    // Flag materializzati per il frame corrente (in base all'altezza e alle gesture)
    bool showWarpRail = true;
    bool showGainEnvelope = true;
    bool showCutRail = true;
    bool showDisplacementBadges = true;
};

/** Calcola la visibilita' Level of Detail (LOD) in base all'altezza della clip
    e all'eventuale gesture attiva (se in drag, la feature resta visibile). */
[[nodiscard]] inline ClipFeatures computeClipFeaturesLOD (
    const ClipFeatures& baseFeatures,
    const float clipHeight,
    const bool isDraggingWarp = false,
    const bool isDraggingGain = false) noexcept
{
    juce::ignoreUnused (isDraggingWarp);
    ClipFeatures f = baseFeatures;

    // Warp Rail: non deve mai nascondersi con lo zoom verticale (visibile finche' abilitato)
    f.showWarpRail = f.warpEnabled;

    // Gain / MIDI CC Envelope: si nasconde sotto i 46px di altezza della clip, salvo se in drag attivo
    f.showGainEnvelope = f.gainEnabled && (clipHeight >= 46.0f || isDraggingGain);

    // Cut Rail: non deve mai nascondersi con lo zoom verticale (visibile finche' abilitato)
    f.showCutRail = f.cutEnabled;

    // Badge numerici di spostamento (samples / ticks / dB): visibili per altezza >= 52px
    f.showDisplacementBadges = (clipHeight >= 52.0f);

    return f;
}

/** Metriche di overflow richieste dalle decorazioni attive per la Space Reclamation. */
struct ClipOverflowMetrics
{
    float top = 0.0f;
    float bottom = 0.0f;
    float horizontal = 9.0f;
};

[[nodiscard]] inline ClipOverflowMetrics computeClipOverflows (const ClipFeatures& f) noexcept
{
    ClipOverflowMetrics m;
    m.top = f.showWarpRail ? 8.0f : 0.0f;
    m.bottom = f.showCutRail ? 6.0f : 0.0f;
    m.horizontal = 9.0f;
    return m;
}

/** Politica di ancoraggio dei punti di confine dell'automazione. */
using AutomationBoundaryPolicy = ayra::curves::CurveBoundaryMode;

/** Verifica se un punto di automazione può essere spostato orizzontalmente.
    Single Source of Truth per rendering del tratteggio e hit-testing.
    Principio: non mostriamo la linea tratteggiata sotto se il punto non può muoversi lateralmente. */
[[nodiscard]] inline bool canAutomationPointMoveHorizontally (
    const AutomationBoundaryPolicy policy,
    const int pointIndex,
    const int totalPoints) noexcept
{
    return ayra::curves::curvePointCanMoveHorizontally (
        policy, pointIndex, totalPoints);
}

/** Gli endpoint sono permanenti; soltanto i punti interni sono eliminabili. */
[[nodiscard]] inline bool canAutomationPointBeRemoved (
    const int pointIndex, const int totalPoints) noexcept
{
    return ayra::curves::curvePointCanBeRemoved (pointIndex, totalPoints);
}

/** Primitiva geometrica normalizzata canonica per tutti i punti di automazione
    (Audio Gain, CC, Pitch Bend, Line Clip, Continuous Lines). */
struct AutomationPoint
{
    float normX = 0.0f;     // [0.0 .. 1.0] lungo la durata della clip o della timeline
    union
    {
        float normY = 0.5f;     // [0.0 .. 1.0] valore normalizzato
        float normValue;        // alias per compatibilità con il codice Midi/Continuous esistente
    };
    float tension = 0.5f;   // [0..1] controllo Y relativo legacy del segmento uscente
    bool selected = false;  // Stato di selezione per editing multi-punto
    float controlX = -1.0f; // [0..1] nel segmento; <0 usa la tensione legacy
    float controlY = -1.0f; // [0..1] nel rettangolo degli endpoint; <0 usa tension
    float controlWeight = -1.0f; // <=0 usa la sharpness automatica canonica
};

using MidiEnvelopePoint = AutomationPoint;

/** Proietta gli X del primo/ultimo punto nelle durate fade compatibili col
    modello `Clip`. La sequenza di punti resta l'unica source of truth UI. */
template <typename PointType, typename Projection>
[[nodiscard]] inline ayra::curves::FadeDurations
projectAutomationFadeDurations (
    const PointType* points,
    const int numPoints,
    const double clipLength,
    Projection&& project) noexcept
{
    if (points == nullptr || numPoints < 2
        || ! std::isfinite (clipLength) || clipLength <= 0.0)
        return {};

    const auto first = project (points[0]);
    const auto last = project (points[numPoints - 1]);
    return ayra::curves::fadeDurationsFromEndpointPositions (
        clipLength, first.normX, last.normX);
}

/** Ripristina deterministicamente gli X richiesti da `pinnedEndpoints`. */
inline void enforceAutomationBoundaryPositions (
    AutomationPoint* points,
    const int totalPoints,
    const AutomationBoundaryPolicy policy) noexcept
{
    if (points == nullptr || totalPoints <= 0
        || ayra::curves::curveEndpointsCanMove (policy))
        return;

    points[0].normX = 0.0f;
    if (totalPoints > 1)
        points[totalPoints - 1].normX = 1.0f;
}

/** Calcolo canonico del control point quadratico Bezier della tensione. */
[[nodiscard]] inline float automationControlValue (
    const float v0, const float v1, const float tension) noexcept
{
    return v0 + juce::jlimit (0.0f, 1.0f, tension) * (v1 - v0);
}

[[nodiscard]] inline bool hasExplicitAutomationControl (
    const AutomationPoint& point) noexcept
{
    return std::isfinite (point.controlX) && std::isfinite (point.controlY)
        && point.controlX >= 0.0f;
}

/** Restituisce il vero punto di controllo nel dominio normalizzato della lane. */
[[nodiscard]] inline juce::Point<float> automationControlPoint (
    const AutomationPoint& start, const AutomationPoint& end) noexcept
{
    const ayra::curves::QuadraticControl control = hasExplicitAutomationControl (start)
        ? ayra::curves::QuadraticControl {
              start.controlX, start.controlY, start.controlWeight }
        : ayra::curves::QuadraticControl { 0.5, start.tension };
    const auto projected = ayra::curves::boundedQuadraticControlPoint ({
        start.normX, start.normY, end.normX, end.normY, control });
    return { static_cast<float> (projected.x), static_cast<float> (projected.y) };
}

/** Proiezione X condivisa da path, punti, tratteggio e hit-test.

    L'inset evita di tagliare a meta' gli anchor estremi, ma viene applicato a
    tutta la geometria visiva: la curva non puo' quindi precedere il punto. */
[[nodiscard]] inline float automationPointRenderX (
    const juce::Rectangle<float> bounds,
    const int pointIndex,
    const int totalPoints,
    const float normX) noexcept
{
    const float exactX = bounds.getX()
        + juce::jlimit (0.0f, 1.0f, normX) * bounds.getWidth();
    const float endpointInset = juce::jmin (
        4.0f, juce::jmax (0.0f, bounds.getWidth() * 0.5f));
    if (pointIndex == 0)
        return juce::jmax (bounds.getX() + endpointInset, exactX);
    if (pointIndex == totalPoints - 1)
        return juce::jmin (bounds.getRight() - endpointInset, exactX);
    return exactX;
}

[[nodiscard]] inline juce::Point<float> automationPointPixelPosition (
    const juce::Rectangle<float> bounds,
    const AutomationPoint& point,
    const int pointIndex,
    const int totalPoints) noexcept
{
    return {
        automationPointRenderX (bounds, pointIndex, totalPoints, point.normX),
        bounds.getBottom() - juce::jlimit (0.0f, 1.0f, point.normY)
            * bounds.getHeight()
    };
}

[[nodiscard]] inline juce::Point<float> automationControlPixelPosition (
    const juce::Rectangle<float> bounds,
    const AutomationPoint& start,
    const AutomationPoint& end,
    const int segmentIndex,
    const int totalPoints) noexcept
{
    const auto startPixel = automationPointPixelPosition (
        bounds, start, segmentIndex, totalPoints);
    const auto endPixel = automationPointPixelPosition (
        bounds, end, segmentIndex + 1, totalPoints);
    const float controlX = hasExplicitAutomationControl (start)
        ? juce::jlimit (0.0f, 1.0f, start.controlX) : 0.5f;
    const float controlY = hasExplicitAutomationControl (start)
        ? juce::jlimit (0.0f, 1.0f, start.controlY)
        : juce::jlimit (0.0f, 1.0f, start.tension);
    const auto projected = ayra::curves::boundedQuadraticControlPoint ({
        startPixel.x, startPixel.y, endPixel.x, endPixel.y,
        { controlX, controlY, start.controlWeight } });
    return { static_cast<float> (projected.x), static_cast<float> (projected.y) };
}

/** Geometria derivata degli anchor al bordo e degli eventuali fade impliciti.

    Il risultato e' consumato sia dal paint sia dall'hit-test. Nessuna coordinata
    dei fade viene quindi ricalcolata nei singoli widget. */
struct AutomationBoundaryGeometry
{
    juce::Point<float> startAnchor;
    juce::Point<float> endAnchor;
    BoundedCurvePixelSegment fadeIn;
    BoundedCurvePixelSegment fadeOut;
    bool hasFadeIn = false;
    bool hasFadeOut = false;
    bool showGuide = false;
};

template <typename PointType, typename Projection>
[[nodiscard]] inline AutomationBoundaryGeometry computeProjectedAutomationBoundaryGeometry (
    const juce::Rectangle<float> bounds,
    const PointType* points,
    const int numPoints,
    const ayra::curves::CurveBoundarySpec boundary,
    Projection&& project) noexcept
{
    AutomationBoundaryGeometry geometry;
    const auto toPixelY = [&bounds] (const double value) noexcept
    {
        return bounds.getBottom() - static_cast<float> (
            ayra::curves::clamp01 (value)) * bounds.getHeight();
    };

    geometry.startAnchor = { bounds.getX(), toPixelY (boundary.startValue) };
    geometry.endAnchor = { bounds.getRight(), toPixelY (boundary.endValue) };
    geometry.showGuide = ayra::curves::curveEndpointsCanMove (boundary.mode)
        && ((boundary.startValue > 0.0001 && boundary.startValue < 0.9999)
            || (boundary.endValue > 0.0001 && boundary.endValue < 0.9999));

    if (points == nullptr || numPoints <= 0
        || ! ayra::curves::curveEndpointsCanMove (boundary.mode))
        return geometry;

    const auto first = project (points[0]);
    const auto firstPixel = automationPointPixelPosition (
        bounds, first, 0, numPoints);
    const float firstExactX = bounds.getX()
        + juce::jlimit (0.0f, 1.0f, first.normX) * bounds.getWidth();
    geometry.hasFadeIn = firstExactX > bounds.getX() + 1.0f;
    geometry.fadeIn = makeBoundedCurvePixelSegment (
        geometry.startAnchor, firstPixel, boundary.fadeInControl);

    const int lastIndex = numPoints - 1;
    const auto last = project (points[lastIndex]);
    const auto lastPixel = automationPointPixelPosition (
        bounds, last, lastIndex, numPoints);
    const float lastExactX = bounds.getX()
        + juce::jlimit (0.0f, 1.0f, last.normX) * bounds.getWidth();
    geometry.hasFadeOut = lastExactX < bounds.getRight() - 1.0f;
    geometry.fadeOut = makeBoundedCurvePixelSegment (
        lastPixel, geometry.endAnchor, boundary.fadeOutControl);
    return geometry;
}

[[nodiscard]] inline AutomationBoundaryGeometry computeAutomationBoundaryGeometry (
    const juce::Rectangle<float> bounds,
    const AutomationPoint* points,
    const int numPoints,
    const ayra::curves::CurveBoundarySpec boundary) noexcept
{
    return computeProjectedAutomationBoundaryGeometry (
        bounds, points, numPoints, boundary,
        [] (const AutomationPoint& point) noexcept { return point; });
}

/** Imposta il punto di controllo esplicito dalla posizione normalizzata del mouse. */
inline void setAutomationControlPoint (
    AutomationPoint& start, const AutomationPoint& end,
    const float normX, const float normY) noexcept
{
    const auto control = ayra::curves::makeBoundedQuadraticControl (
        { start.normX, start.normY, end.normX, end.normY },
        normX, juce::jlimit (0.0f, 1.0f, normY));
    start.controlX = static_cast<float> (control.x);
    start.controlY = static_cast<float> (control.y);
    start.controlWeight = -1.0f;
    start.tension = 0.5f;
}

inline void resetAutomationControlPoint (
    AutomationPoint& start, const AutomationPoint& end) noexcept
{
    start.controlX = 0.5f;
    start.controlY = 0.5f;
    start.controlWeight = -1.0f;
    start.tension = 0.5f;
}

[[nodiscard]] inline float automationControlValue (
    const AutomationPoint& start, const AutomationPoint& end) noexcept
{
    return automationControlValue (start.normY, end.normY, start.tension);
}

/** Valuta un segmento quadratico Bezier in un punto normX arbitrario. */
[[nodiscard]] inline float evaluateAutomationSegment (
    const AutomationPoint& start,
    const AutomationPoint& end,
    const float normX) noexcept
{
    const float span = std::max (0.0001f, end.normX - start.normX);
    const float u = juce::jlimit (0.0f, 1.0f, (normX - start.normX) / span);
    const ayra::curves::QuadraticControl control = hasExplicitAutomationControl (start)
        ? ayra::curves::QuadraticControl {
              start.controlX, start.controlY, start.controlWeight }
        : ayra::curves::QuadraticControl { 0.5, start.tension };
    return juce::jlimit (0.0f, 1.0f, static_cast<float> (
        ayra::curves::evaluateBoundedQuadraticValue (
            u, start.normY, control, end.normY)));
}

/** Valuta una sequenza completa includendo gli anchor impliciti dei fade.

    `project` adatta i point type specifici dei consumer alla rappresentazione
    canonica senza creare vettori temporanei o duplicare l'algoritmo. */
template <typename PointType, typename Projection>
[[nodiscard]] inline float evaluateProjectedAutomationCurveAt (
    const PointType* points,
    const int numPoints,
    const float defaultValue,
    const ayra::curves::CurveBoundarySpec boundary,
    const float normX,
    Projection&& project) noexcept
{
    const float x = juce::jlimit (0.0f, 1.0f, normX);
    if (points == nullptr || numPoints <= 0)
        return juce::jlimit (0.0f, 1.0f, defaultValue);

    const auto first = project (points[0]);
    if (x < first.normX)
    {
        if (! ayra::curves::curveEndpointsCanMove (boundary.mode))
            return juce::jlimit (0.0f, 1.0f, first.normY);

        const float span = std::max (0.0001f, first.normX);
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (
            ayra::curves::evaluateBoundedQuadraticValue (
                x / span, boundary.startValue,
                boundary.fadeInControl, first.normY)));
    }

    const auto last = project (points[numPoints - 1]);
    if (x > last.normX)
    {
        if (! ayra::curves::curveEndpointsCanMove (boundary.mode))
            return juce::jlimit (0.0f, 1.0f, last.normY);

        const float span = std::max (0.0001f, 1.0f - last.normX);
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (
            ayra::curves::evaluateBoundedQuadraticValue (
                (x - last.normX) / span, last.normY,
                boundary.fadeOutControl, boundary.endValue)));
    }

    int segment = 0;
    while (segment + 1 < numPoints
           && project (points[segment + 1]).normX < x)
        ++segment;
    if (segment + 1 >= numPoints)
        return juce::jlimit (0.0f, 1.0f, last.normY);
    return evaluateAutomationSegment (
        project (points[segment]), project (points[segment + 1]), x);
}

[[nodiscard]] inline float evaluateAutomationCurveAt (
    const AutomationPoint* points,
    const int numPoints,
    const float defaultValue,
    const ayra::curves::CurveBoundarySpec boundary,
    const float normX) noexcept
{
    return evaluateProjectedAutomationCurveAt (
        points, numPoints, defaultValue, boundary, normX,
        [] (const AutomationPoint& point) noexcept { return point; });
}

/** Divisione esatta di De Casteljau per un segmento quadratico di Bezier in splitNormX.
    Calcola il valore esatto di taglio `outCutValue` e le due nuove curvature Bezier equivalenti. */
inline void splitQuadraticSegmentDeCasteljau (
    const AutomationPoint& p0,
    const AutomationPoint& p1,
    const float splitNormX,
    float& outCutValue,
    float& outLeftTension,
    float& outRightTension) noexcept
{
    const ayra::curves::QuadraticControl control = hasExplicitAutomationControl (p0)
        ? ayra::curves::QuadraticControl {
              p0.controlX, p0.controlY, p0.controlWeight }
        : ayra::curves::QuadraticControl { 0.5, p0.tension };
    const auto split = ayra::curves::splitBoundedQuadraticAtX (
        { p0.normX, p0.normY, p1.normX, p1.normY, control },
        splitNormX);
    outCutValue = juce::jlimit (0.0f, 1.0f, static_cast<float> (split.y));
    outLeftTension = static_cast<float> (split.leftControl.y);
    outRightTension = static_cast<float> (split.rightControl.y);
}

/** Geometria completa dello split, senza riconversione loss-y a tensione. */
struct AutomationSegmentSplit
{
    float cutValue = 0.5f;
    ayra::curves::QuadraticControl leftControl {};
    ayra::curves::QuadraticControl rightControl {};
};

/** Risultato della divisione di una sequenza di automazione con la policy di
    confine gia' adattata per ciascuna clip figlia. */
struct AutomationPointSequenceSplit
{
    std::vector<AutomationPoint> left;
    std::vector<AutomationPoint> right;
    ayra::curves::CurveBoundarySpec leftBoundary;
    ayra::curves::CurveBoundarySpec rightBoundary;
};

/** Divide esattamente il segmento nel punto X richiesto.

    Entrambe le coordinate dei controlli sono relative ai rettangoli dei
    rispettivi sottosegmenti e possono essere scritte direttamente nei punti
    di partenza.
*/
[[nodiscard]] inline AutomationSegmentSplit splitAutomationSegment (
    const AutomationPoint& start,
    const AutomationPoint& end,
    const float splitNormX) noexcept
{
    const ayra::curves::QuadraticControl control = hasExplicitAutomationControl (start)
        ? ayra::curves::QuadraticControl {
              start.controlX, start.controlY, start.controlWeight }
        : ayra::curves::QuadraticControl { 0.5, start.tension };
    const auto split = ayra::curves::splitBoundedQuadraticAtX (
        { start.normX, start.normY, end.normX, end.normY, control },
        splitNormX);

    AutomationSegmentSplit result;
    result.cutValue = static_cast<float> (split.y);
    result.leftControl = split.leftControl;
    result.rightControl = split.rightControl;
    return result;
}

/** Divide una sequenza preservando la geometria, inclusi gli eventuali fade
    impliciti verso gli anchor di confine.

    Precondizione: `input` e' ordinato per `normX`, come garantito dai model
    clip. La funzione non riordina per evitare lavoro e allocazioni ridondanti.
    Se il taglio coincide con un punto esistente, quel punto termina la parte
    sinistra e viene copiato all'inizio della destra con il proprio controllo
    uscente. Per un taglio interno al segmento usa De Casteljau e materializza i
    due controlli figli. Funzione da Message Thread: alloca i due vettori.
*/
[[nodiscard]] inline AutomationPointSequenceSplit splitAutomationPointSequence (
    const std::vector<AutomationPoint>& input,
    const float defaultValue,
    const float splitNormX,
    const ayra::curves::CurveBoundarySpec boundary = {})
{
    AutomationPointSequenceSplit result;
    if (! std::isfinite (splitNormX)
        || splitNormX <= 0.0f || splitNormX >= 1.0f)
        return result;

    std::vector<AutomationPoint> source = input;
    bool materializedStartAnchor = false;
    bool materializedEndAnchor = false;
    if (source.empty())
    {
        source.push_back ({ 0.0f, defaultValue, 0.5f, false });
        source.push_back ({ 1.0f, defaultValue, 0.5f, false });
    }
    else
    {
        if (source.front().normX > 0.0001f)
        {
            const bool hasFade = ayra::curves::curveEndpointsCanMove (
                boundary.mode);
            AutomationPoint start {
                0.0f,
                hasFade ? static_cast<float> (ayra::curves::clamp01 (
                              boundary.startValue))
                        : source.front().normY,
                0.5f, false };
            if (hasFade)
            {
                materializedStartAnchor = true;
                start.controlX = static_cast<float> (
                    ayra::curves::clamp01 (boundary.fadeInControl.x));
                start.controlY = static_cast<float> (
                    ayra::curves::clamp01 (boundary.fadeInControl.y));
                start.controlWeight = static_cast<float> (
                    boundary.fadeInControl.weight);
            }
            source.insert (source.begin(), start);
        }
        if (source.back().normX < 0.9999f)
        {
            const bool hasFade = ayra::curves::curveEndpointsCanMove (
                boundary.mode);
            if (hasFade)
            {
                materializedEndAnchor = true;
                source.back().controlX = static_cast<float> (
                    ayra::curves::clamp01 (boundary.fadeOutControl.x));
                source.back().controlY = static_cast<float> (
                    ayra::curves::clamp01 (boundary.fadeOutControl.y));
                source.back().controlWeight = static_cast<float> (
                    boundary.fadeOutControl.weight);
            }
            source.push_back ({
                1.0f,
                hasFade ? static_cast<float> (ayra::curves::clamp01 (
                              boundary.endValue))
                        : source.back().normY,
                0.5f, false });
        }
    }

    size_t segment = 0;
    while (segment + 1 < source.size()
           && source[segment + 1].normX < splitNormX)
        ++segment;

    const bool splitOnExistingPoint = segment + 1 < source.size()
        && std::abs (source[segment + 1].normX - splitNormX) <= 0.0001f;
    const size_t splitPointIndex = splitOnExistingPoint ? segment + 1 : segment;

    AutomationSegmentSplit geometry;
    float cutValue = source[splitPointIndex].normY;
    if (! splitOnExistingPoint && segment + 1 < source.size())
    {
        geometry = splitAutomationSegment (
            source[segment], source[segment + 1], splitNormX);
        cutValue = geometry.cutValue;
    }

    result.left.reserve (source.size() + 1);
    result.right.reserve (source.size() + 1);
    const size_t leftLast = splitOnExistingPoint ? splitPointIndex : segment;
    for (size_t index = 0; index <= leftLast; ++index)
    {
        auto point = source[index];
        point.normX /= splitNormX;
        point.selected = false;
        result.left.push_back (point);
    }

    if (splitOnExistingPoint)
    {
        result.left.back().normX = 1.0f;
    }
    else
    {
        auto& outgoing = result.left.back();
        outgoing.controlX = static_cast<float> (geometry.leftControl.x);
        outgoing.controlY = static_cast<float> (geometry.leftControl.y);
        outgoing.controlWeight = static_cast<float> (
            geometry.leftControl.weight);
        outgoing.tension = 0.5f;
        result.left.push_back ({ 1.0f, cutValue, 0.5f, false });
    }

    AutomationPoint rightStart { 0.0f, cutValue, 0.5f, false };
    if (splitOnExistingPoint)
    {
        rightStart = source[splitPointIndex];
        rightStart.normX = 0.0f;
    }
    else
    {
        rightStart.controlX = static_cast<float> (geometry.rightControl.x);
        rightStart.controlY = static_cast<float> (geometry.rightControl.y);
        rightStart.controlWeight = static_cast<float> (
            geometry.rightControl.weight);
    }
    rightStart.selected = false;
    result.right.push_back (rightStart);

    for (size_t index = splitPointIndex + 1; index < source.size(); ++index)
    {
        auto point = source[index];
        point.normX = (point.normX - splitNormX) / (1.0f - splitNormX);
        point.selected = false;
        result.right.push_back (point);
    }

    // Gli anchor dei fade servono soltanto per dividere esattamente la curva:
    // non diventano nodi editabili nei figli. Restano materializzati soltanto
    // quando il taglio cade dentro il fade e rimuoverli lascerebbe meno dei due
    // punti necessari a rappresentare senza perdita quel frammento.
    if (materializedStartAnchor && result.left.size() > 2)
        result.left.erase (result.left.begin());
    if (materializedEndAnchor && result.right.size() > 2)
        result.right.pop_back();

    result.leftBoundary = boundary;
    result.leftBoundary.endValue = cutValue;
    result.leftBoundary.fadeOutControl = {};
    result.rightBoundary = boundary;
    result.rightBoundary.startValue = cutValue;
    result.rightBoundary.fadeInControl = {};

    if (boundary.mode == ayra::curves::CurveBoundaryMode::pinnedEndpoints)
    {
        if (! result.left.empty())
        {
            result.left.front().normX = 0.0f;
            result.left.back().normX = 1.0f;
        }
        if (! result.right.empty())
        {
            result.right.front().normX = 0.0f;
            result.right.back().normX = 1.0f;
        }
    }
    return result;
}

/** Taglia o estende una sequenza di automazione su una finestra temporale [windowStartSec, windowEndSec]
    rispetto alla clip sorgente definita su [sourceStartSec, sourceStartSec + sourceLengthSec].

    Supporta sia `pinnedEndpoints` che `fadeEndpoints`:
    - `pinnedEndpoints`:
      - Riduzione: l'endpoint assume il valore esatto della curva nel punto di taglio,
        mantenendo la geometria Bezier tramite split De Casteljau.
      - Espansione entro la partenza: l'endpoint continua ad assumere il valore della curva.
      - Espansione oltre l'endpoint di partenza: l'endpoint originale viene fissato e viene
        creato un nuovo punto al limite della clip con lo stesso valore Y, generando una linea orizzontale costante.
    - `fadeEndpoints`:
      - Mantiene la durata temporale del fade-in e del fade-out.
      - Riduzione: il punto di fade assume il valore della curva nel punto in cui si posiziona.
      - Espansione entro il punto di fade di partenza: assume il valore della curva.
      - Espansione oltre il punto di fade originale: il punto di fade originale viene fissato
        e viene creato un nuovo punto di fade con lo stesso valore Y, generando una linea orizzontale costante,
        seguita dal fade verso il valore di confine (startValue / endValue).
*/
[[nodiscard]] inline std::vector<AutomationPoint> trimAutomationPointsWindow (
    const std::vector<AutomationPoint>& input,
    const ayra::curves::CurveBoundarySpec boundary,
    const float defaultValue,
    const double sourceStartSec,
    const double sourceLengthSec,
    const double windowStartSec,
    const double windowEndSec)
{
    if (! std::isfinite (sourceLengthSec) || sourceLengthSec <= 0.0001
        || ! std::isfinite (windowStartSec) || ! std::isfinite (windowEndSec)
        || windowEndSec <= windowStartSec + 0.0001)
        return input;

    const double L0 = sourceLengthSec;
    const double S0 = sourceStartSec;
    const double E0 = S0 + L0;

    const double L_new = windowEndSec - windowStartSec;
    const double T_left = windowStartSec;
    const double T_right = windowEndSec;

    std::vector<AutomationPoint> source = input;
    if (source.empty())
    {
        source.push_back ({ 0.0f, defaultValue, 0.5f, false });
        source.push_back ({ 1.0f, defaultValue, 0.5f, false });
    }
    else if (source.size() == 1)
    {
        const auto pt = source.front();
        source.clear();
        source.push_back ({ 0.0f, pt.normY, 0.5f, false });
        source.push_back ({ 1.0f, pt.normY, 0.5f, false });
    }

    const bool isFadeMode = ayra::curves::curveEndpointsCanMove (boundary.mode);
    if (! isFadeMode)
    {
        enforceAutomationBoundaryPositions (
            source.data(), static_cast<int> (source.size()), boundary.mode);
    }

    double origFadeInDur = 0.0;
    double origFadeOutDur = 0.0;
    if (isFadeMode)
    {
        origFadeInDur  = std::max (0.0, static_cast<double> (source.front().normX) * L0);
        origFadeOutDur = std::max (0.0, static_cast<double> (1.0f - source.back().normX) * L0);
    }

    const double maxFadeDur = L_new * 0.49;
    const double fadeInDur  = std::min (origFadeInDur, maxFadeDur);
    const double fadeOutDur = std::min (origFadeOutDur, maxFadeDur);

    const double T_left_target  = T_left + fadeInDur;
    const double T_right_target = T_right - fadeOutDur;

    const double T_orig_left  = S0 + origFadeInDur;
    const double T_orig_right = E0 - origFadeOutDur;

    const float xTargetLeft  = isFadeMode ? static_cast<float> (fadeInDur / L_new) : 0.0f;
    const float xTargetRight = isFadeMode ? static_cast<float> (1.0 - (fadeOutDur / L_new)) : 1.0f;

    auto timeToClipNormX = [T_left, L_new] (double t) noexcept -> float
    {
        return juce::jlimit (0.0f, 1.0f, static_cast<float> ((t - T_left) / L_new));
    };

    auto timeToSourceNormX = [S0, L0] (double t) noexcept -> float
    {
        return juce::jlimit (0.0f, 1.0f, static_cast<float> ((t - S0) / L0));
    };

    auto evalSourceCurveAtTime = [&] (double t) noexcept -> float
    {
        const float u = timeToSourceNormX (t);
        return evaluateAutomationCurveAt (
            source.data(), static_cast<int> (source.size()), defaultValue, boundary, u);
    };

    if (T_left_target >= T_orig_right - 0.0001)
    {
        const float y = source.back().normY;
        return { { xTargetLeft, y, 0.5f, false }, { xTargetRight, y, 0.5f, false } };
    }
    if (T_right_target <= T_orig_left + 0.0001)
    {
        const float y = source.front().normY;
        return { { xTargetLeft, y, 0.5f, false }, { xTargetRight, y, 0.5f, false } };
    }

    const double safeRightTarget = std::max (T_right_target, T_left_target + 0.0001);
    const double tA = std::max (T_left_target, T_orig_left);
    const double tB = std::min (safeRightTarget, T_orig_right);

    const float u_A = timeToSourceNormX (tA);
    const float u_B = timeToSourceNormX (tB);

    AutomationPoint midStart { 0.0f, source.front().normY, 0.5f, false };
    AutomationPoint midEnd   { 0.0f, source.back().normY, 0.5f, false };
    std::vector<AutomationPoint> midIntermediates;

    if (u_B <= u_A + 0.00001f)
    {
        const float y = evalSourceCurveAtTime (tA);
        midStart.normY = y;
        midEnd.normY   = y;
    }
    else
    {
        size_t idxA = 0;
        while (idxA + 1 < source.size() && source[idxA + 1].normX < u_A)
            ++idxA;

        size_t idxB = 0;
        while (idxB + 1 < source.size() && source[idxB + 1].normX < u_B)
            ++idxB;

        if (idxA == idxB)
        {
            const auto& s0 = source[idxA];
            const auto& s1 = source[idxA + 1];

            const bool cutAtStart = (u_A <= s0.normX + 0.0001f);
            const bool cutAtEnd   = (u_B >= s1.normX - 0.0001f);

            if (cutAtStart && cutAtEnd)
            {
                midStart = s0;
                midEnd   = s1;
            }
            else if (cutAtStart)
            {
                midStart = s0;
                const auto splitB = splitAutomationSegment (s0, s1, u_B);
                midStart.controlX      = static_cast<float> (splitB.leftControl.x);
                midStart.controlY      = static_cast<float> (splitB.leftControl.y);
                midStart.controlWeight = static_cast<float> (splitB.leftControl.weight);
                midStart.tension       = 0.5f;
                midEnd.normY           = splitB.cutValue;
            }
            else if (cutAtEnd)
            {
                const auto splitA = splitAutomationSegment (s0, s1, u_A);
                midStart.normY         = splitA.cutValue;
                midStart.controlX      = static_cast<float> (splitA.rightControl.x);
                midStart.controlY      = static_cast<float> (splitA.rightControl.y);
                midStart.controlWeight = static_cast<float> (splitA.rightControl.weight);
                midStart.tension       = 0.5f;
                midEnd                 = s1;
            }
            else
            {
                const auto splitB = splitAutomationSegment (s0, s1, u_B);
                AutomationPoint tempStart = s0;
                tempStart.controlX      = static_cast<float> (splitB.leftControl.x);
                tempStart.controlY      = static_cast<float> (splitB.leftControl.y);
                tempStart.controlWeight = static_cast<float> (splitB.leftControl.weight);
                tempStart.tension       = 0.5f;
                AutomationPoint tempEnd { u_B, splitB.cutValue, 0.5f, false };

                const auto splitA = splitAutomationSegment (tempStart, tempEnd, u_A);
                midStart.normY         = splitA.cutValue;
                midStart.controlX      = static_cast<float> (splitA.rightControl.x);
                midStart.controlY      = static_cast<float> (splitA.rightControl.y);
                midStart.controlWeight = static_cast<float> (splitA.rightControl.weight);
                midStart.tension       = 0.5f;
                midEnd.normY           = splitB.cutValue;
            }
        }
        else
        {
            // Start cut in segment idxA
            if (u_A <= source[idxA].normX + 0.0001f)
            {
                midStart = source[idxA];
            }
            else if (u_A >= source[idxA + 1].normX - 0.0001f)
            {
                midStart = source[idxA + 1];
            }
            else
            {
                const auto splitA = splitAutomationSegment (source[idxA], source[idxA + 1], u_A);
                midStart.normY         = splitA.cutValue;
                midStart.controlX      = static_cast<float> (splitA.rightControl.x);
                midStart.controlY      = static_cast<float> (splitA.rightControl.y);
                midStart.controlWeight = static_cast<float> (splitA.rightControl.weight);
                midStart.tension       = 0.5f;
            }

            // Intermediate points strictly between u_A and u_B
            for (size_t k = idxA + 1; k <= idxB; ++k)
            {
                if (source[k].normX > u_A + 0.0001f && source[k].normX < u_B - 0.0001f)
                    midIntermediates.push_back (source[k]);
            }

            // End cut in segment idxB
            if (u_B >= source[idxB + 1].normX - 0.0001f)
            {
                midEnd = source[idxB + 1];
            }
            else if (u_B <= source[idxB].normX + 0.0001f)
            {
                midEnd = source[idxB];
            }
            else
            {
                const auto splitB = splitAutomationSegment (source[idxB], source[idxB + 1], u_B);
                if (! midIntermediates.empty())
                {
                    midIntermediates.back().controlX      = static_cast<float> (splitB.leftControl.x);
                    midIntermediates.back().controlY      = static_cast<float> (splitB.leftControl.y);
                    midIntermediates.back().controlWeight = static_cast<float> (splitB.leftControl.weight);
                    midIntermediates.back().tension       = 0.5f;
                }
                else
                {
                    midStart.controlX      = static_cast<float> (splitB.leftControl.x);
                    midStart.controlY      = static_cast<float> (splitB.leftControl.y);
                    midStart.controlWeight = static_cast<float> (splitB.leftControl.weight);
                    midStart.tension       = 0.5f;
                }
                midEnd.normY = splitB.cutValue;
            }
        }
    }

    std::vector<AutomationPoint> result;
    result.reserve (source.size() + 4);

    // --- LEFT EDGE ---
    if (T_left_target < T_orig_left - 0.0001)
    {
        const float yStart = source.front().normY;
        AutomationPoint pLeft { xTargetLeft, yStart, 0.5f, false };
        pLeft.controlX      = 0.5f;
        pLeft.controlY      = 0.5f;
        pLeft.controlWeight = -1.0f;
        result.push_back (pLeft);

        const float xOrigStart = timeToClipNormX (T_orig_left);
        midStart.normX = xOrigStart;
        result.push_back (midStart);
    }
    else
    {
        midStart.normX = xTargetLeft;
        result.push_back (midStart);
    }

    // --- INTERMEDIATE POINTS ---
    for (auto pt : midIntermediates)
    {
        const double t_k = S0 + static_cast<double> (pt.normX) * L0;
        pt.normX = timeToClipNormX (t_k);
        pt.selected = false;
        result.push_back (pt);
    }

    // --- RIGHT EDGE ---
    if (T_right_target > T_orig_right + 0.0001)
    {
        const float xOrigEnd = timeToClipNormX (T_orig_right);
        midEnd.normX         = xOrigEnd;
        midEnd.tension       = 0.5f;
        midEnd.controlX      = 0.5f;
        midEnd.controlY      = 0.5f;
        midEnd.controlWeight = -1.0f;
        result.push_back (midEnd);

        const float yEnd = source.back().normY;
        AutomationPoint pRight { xTargetRight, yEnd, 0.5f, false };
        result.push_back (pRight);
    }
    else
    {
        midEnd.normX = xTargetRight;
        midEnd.selected = false;
        result.push_back (midEnd);
    }

    // Ensure strict X monotonicity and clean up near-duplicates
    for (size_t i = 1; i < result.size(); )
    {
        if (result[i].normX <= result[i - 1].normX + 0.0001f)
        {
            if (i + 1 < result.size())
                result.erase (result.begin() + static_cast<ptrdiff_t> (i));
            else
                result.erase (result.begin() + static_cast<ptrdiff_t> (i - 1));
        }
        else
        {
            ++i;
        }
    }

    if (! isFadeMode && result.size() >= 2)
    {
        result.front().normX = 0.0f;
        result.back().normX  = 1.0f;
    }
    else if (isFadeMode && result.size() >= 2)
    {
        result.front().normX = xTargetLeft;
        result.back().normX  = xTargetRight;
    }

    if (result.size() < 2)
    {
        result.clear();
        const float y = evalSourceCurveAtTime ((T_left + T_right) * 0.5);
        result.push_back ({ xTargetLeft, y, 0.5f, false });
        result.push_back ({ xTargetRight, y, 0.5f, false });
    }

    return result;
}

/** Prepara il drag verticale "pinned" di un segmento.

    Gli endpoint originali restano invariati. Due loro copie vengono inserite
    alle stesse X; il controllo uscente originale passa alla prima copia e
    descrive la curva tra le copie, mentre i due raccordi verticali usano il
    controllo lineare di default. Ritorna l'indice del segmento duplicato.
*/
template <typename PointType>
[[nodiscard]] inline int pinAutomationSegmentForVerticalDrag (
    std::vector<PointType>& points,
    const int segmentIndex)
{
    if (segmentIndex < 0
        || segmentIndex + 1 >= static_cast<int> (points.size()))
        return -1;

    const auto index = static_cast<size_t> (segmentIndex);
    auto liftedStart = points[index];
    auto liftedEnd = points[index + 1];
    liftedStart.selected = false;
    liftedEnd.selected = false;

    points[index].tension = 0.5f;
    points[index].controlX = 0.5f;
    points[index].controlY = 0.5f;
    points[index].controlWeight = -1.0f;

    liftedEnd.tension = 0.5f;
    liftedEnd.controlX = 0.5f;
    liftedEnd.controlY = 0.5f;
    liftedEnd.controlWeight = -1.0f;

    const auto insertion = points.begin()
        + static_cast<typename std::vector<PointType>::difference_type> (index + 1);
    points.insert (insertion, { liftedStart, liftedEnd });
    return segmentIndex + 1;
}

[[nodiscard]] inline float midiEnvelopeControlValue (
    const AutomationPoint& start, const AutomationPoint& end) noexcept
{
    return automationControlValue (start, end);
}

[[nodiscard]] inline float evaluateMidiEnvelopeSegment (
    const AutomationPoint& start,
    const AutomationPoint& end,
    const float normX) noexcept
{
    return evaluateAutomationSegment (start, end, normX);
}

/** Parametri di rendering per la funzione LookAndFeel unificata drawAutomationCurve. */
struct AutomationCurveParams
{
    juce::Rectangle<float> bounds;
    AutomationBoundaryPolicy boundaryPolicy = AutomationBoundaryPolicy::pinnedEndpoints;
    float boundaryStartNormValue = 0.0f;
    float boundaryEndNormValue = 0.0f;
    ayra::curves::QuadraticControl fadeInControl {};
    ayra::curves::QuadraticControl fadeOutControl {};
    juce::Colour curveColour { juce::Colours::white };
    juce::Colour accentColour { juce::Colour (0xff00d084) };
    ayra::TimelineCurveUnderlayStyle underlayStyle {
        ayra::TimelineCurveUnderlayStyle::darkBelow };

    const AutomationPoint* points = nullptr;
    int numPoints = 0;

    TimelineClipZone hoveredZone = TimelineClipZone::none;
    int hoveredPointIdx = -1;
    int hoveredCurveSegmentIdx = -1;
    bool isDraggingAll = false;

    std::function<juce::String (int ptIdx, float normY)> formatValue = nullptr;
    juce::String draggingAllBadgeText;
    float draggingAllNormY = 0.5f;
    float dragAnchorX = -1.0f;
};

/** Punto dell'inviluppo audio materializzato per la vista.
    `selected` e' stato di interazione del Lab, non stato documentale. */
struct GainPoint
{
    float normX = 0.5f;
    float gainDb = 0.0f;
    float tension = 0.5f;
    bool selected = false;
    float controlX = -1.0f; ///< X relativa del controllo nel segmento.
    float controlY = -1.0f; ///< Y relativa nel rettangolo definito dagli endpoint.
    float controlWeight = -1.0f; ///< <=0 usa la sharpness automatica.
};

namespace clip_gain_law
{
    [[nodiscard]] inline juce::String formatGainDb (const float db)
    {
        if (db <= -59.0f) { return "-inf dB"; }
        return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    [[nodiscard]] inline float dbToNorm (const float db) noexcept
    {
        if (db <= -59.0f) { return 0.0f; }
        if (db >= 0.0f)
            return juce::jlimit (0.50f, 1.0f, 0.50f + (db / 48.0f) * 0.50f);
        return juce::jlimit (0.0f, 0.50f, 0.50f * ((db + 60.0f) / 60.0f));
    }

    [[nodiscard]] inline float normToDb (float norm) noexcept
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        if (norm <= 0.015f) { return -60.0f; }
        if (norm >= 0.50f)
            return (norm - 0.50f) / 0.50f * 48.0f;
        return -60.0f + (norm / 0.50f) * 60.0f;
    }
}

[[nodiscard]] inline AutomationPoint gainPointAsAutomationPoint (
    const GainPoint& point) noexcept
{
    AutomationPoint result;
    result.normX = point.normX;
    result.normY = clip_gain_law::dbToNorm (point.gainDb);
    result.tension = point.tension;
    result.selected = point.selected;
    result.controlX = point.controlX;
    result.controlY = point.controlY;
    result.controlWeight = point.controlWeight;
    return result;
}

[[nodiscard]] inline GainPoint automationPointAsGainPoint (
    const AutomationPoint& point) noexcept
{
    return {
        point.normX, clip_gain_law::normToDb (point.normY), point.tension,
        point.selected, point.controlX, point.controlY, point.controlWeight
    };
}

inline void setGainControlPoint (
    GainPoint& start, const GainPoint& end,
    const float normX, const float normY) noexcept
{
    auto startAutomation = gainPointAsAutomationPoint (start);
    const auto endAutomation = gainPointAsAutomationPoint (end);
    setAutomationControlPoint (startAutomation, endAutomation, normX, normY);
    start.controlX = startAutomation.controlX;
    start.controlY = startAutomation.controlY;
    start.controlWeight = startAutomation.controlWeight;
    start.tension = startAutomation.tension;
}

inline void resetGainControlPoint (
    GainPoint& start, const GainPoint& end) noexcept
{
    auto startAutomation = gainPointAsAutomationPoint (start);
    const auto endAutomation = gainPointAsAutomationPoint (end);
    resetAutomationControlPoint (startAutomation, endAutomation);
    start.controlX = startAutomation.controlX;
    start.controlY = startAutomation.controlY;
    start.controlWeight = startAutomation.controlWeight;
    start.tension = startAutomation.tension;
}

namespace clip_gain_law
{
    /** Valuta la stessa curva normalizzata usata da paint e hit-test.

        Con `fadeEndpoints` il primo e l'ultimo `GainPoint` sono anche i fade
        endpoint; non esiste una seconda moltiplicazione di fade separata. */
    [[nodiscard]] inline float evaluateGainDbAt (
        const GainPoint* points,
        const int numPoints,
        const float baseGainDb,
        const float normX,
        const ayra::curves::CurveBoundarySpec boundary) noexcept
    {
        return normToDb (evaluateProjectedAutomationCurveAt (
            points, numPoints, dbToNorm (baseGainDb), boundary, normX,
            [] (const GainPoint& point) noexcept
            {
                return gainPointAsAutomationPoint (point);
            }));
    }

    [[nodiscard]] inline float evaluateGainDbAt (
        const GainPoint* points,
        const int numPoints,
        const float baseGainDb,
        const float normX) noexcept
    {
        ayra::curves::CurveBoundarySpec boundary;
        boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
        return evaluateGainDbAt (
            points, numPoints, baseGainDb, normX, boundary);
    }

    [[nodiscard]] inline float dbToGainScale (const float gainDb) noexcept
    {
        if (gainDb <= -59.0f) { return 0.0f; }
        return juce::jlimit (0.0f, 3.5f, std::pow (10.0f, gainDb / 20.0f));
    }
}


//==============================================================================
// OVERLAP & CROSSFADE (XFADE v1: Audio Gain, Line Param, Image Alpha, MIDI Merge/Replace)
//==============================================================================

/** Modalita' di sovrapposizione per le clip MIDI. */
enum class MidiOverlapMode : std::uint8_t
{
    replace = 0, ///< La clip successiva vince (default comune a tutte le clip)
    merge        ///< Le note si fondono; la clip sopra (editabile nell'overlap) e' l'ultima selezionata
};

struct MidiNoteVisual;

/** Proiezione visuale di una regione di Overlap o Crossfade (XFade) su una clip. */
struct ClipOverlapVisual
{
    bool hasOverlap = false;
    bool isCrossfade = false;
    bool isTopClip = true;

    float startNormX = 0.0f; ///< Inizio dell'overlap in coordinate normalizzate [0..1] della clip
    float endNormX   = 0.0f; ///< Fine dell'overlap in coordinate normalizzate [0..1] della clip

    juce::Colour partnerTint { juce::Colours::cyan };
    juce::String partnerTitle;

    // Proiezione della curva della clip sottostante nell'intervallo [startNormX .. endNormX]
    bool showPartnerCurve = false;
    float partnerStartNormY = 0.0f;
    float partnerEndNormY   = 0.0f;
    ayra::curves::QuadraticControl partnerCurveControl {};
    bool partnerPointSelected = false;
    bool partnerPointHovered  = false;
    bool partnerCurveHovered  = false;

    // Per LineClip in modalita' Overlap senza XFade: mostra il salto verticale a inizio overlap
    bool showStepJumpAtStart = false;
    float topClipStartNormY  = 0.0f;

    // Per MidiClip: modalita' replace vs merge e note della clip sottostante
    MidiOverlapMode midiMode = MidiOverlapMode::replace;
    const MidiNoteVisual* partnerNotes = nullptr;
    int numPartnerNotes = 0;
};

/** Proiezione completa, non autorevole, necessaria a un paint audio.
    I puntatori sono view valide per la sola chiamata di paint. Il provider ne
    garantisce il lifetime e il widget non li conserva oltre il frame. */
struct AudioClipVisual
{
    juce::Rectangle<float> bounds;
    ClipFeatures features;
    juce::String title;
    juce::Colour tint { juce::Colours::cyan };
    juce::Colour curveColour { juce::Colours::white };
    ayra::TimelineCurveUnderlayStyle curveUnderlayStyle {
        ayra::TimelineCurveUnderlayStyle::darkBelow };
    bool selected = false;
    bool suspended = false;

    TimelineClipZone hoveredZone { TimelineClipZone::none };
    int hoveredMarkerIdx = -1;
    float hoverMouseX = -1.0f;
    bool showWarpAddGhost = false;
    float warpAddGhostX = -1.0f;
    bool isDraggingAllGain = false;

    int numChannels = 2;
    juce::String channelFormatText;
    float gainDb = 0.0f;
    // Proiezioni di compatibilita' derivate dagli X del primo/ultimo GainPoint.
    // Non sono una seconda source of truth e non vengono editate dal widget.
    float fadeInPx = 0.0f;
    float fadeOutPx = 0.0f;
    float fadeInCurve = 0.5f;
    float fadeOutCurve = 0.5f;
    float fadeInControlX = 0.5f;
    float fadeOutControlX = 0.5f;
    float fadeInControlWeight = -1.0f;
    float fadeOutControlWeight = -1.0f;

    const float* warpMarkersNorm = nullptr;
    const float* origWarpMarkersNorm = nullptr;
    int numWarpMarkers = 0;
    double clipLengthSec = 0.0;

    const juce::Path* waveformPaths = nullptr;
    int numWaveformPaths = 0;

    const GainPoint* gainPoints = nullptr;
    int numGainPoints = 0;
    int hoveredGainPointIdx = -1;
    int hoveredCurveSegmentIdx = -1;

    std::int64_t stretchDeltaSamples = 0;
    bool isStretchActive = false;
    std::int64_t slipDeltaSamples = 0;
    bool isSlipActive = false;
    ClipOverlapVisual overlap;
};

//==============================================================================
// PROIEZIONE VISUALE CLIP MIDI
//==============================================================================

/** Parametro attualmente selezionato nell'overlay continuo della clip MIDI. */
enum class MidiOverlayTarget : std::uint8_t
{
    velocityEnvelope = 0, // Scala dinamica 0% .. 200% (0.5 = 100%)
    cc1Modulation,        // CC 1 (0 .. 127)
    cc11Expression,       // CC 11 (0 .. 127)
    pitchBend             // Pitch Bend (-12 .. +12 st, 0.5 = 0)
};

namespace clip_midi_law
{
    /** I livelli usano altezze volutamente generose: il livello Note Edit deve
        mostrare due ottave con righe utilizzabili, mentre il livello Velocity
        riserva una sub-lane senza comprimere nuovamente il piano roll. */
    enum class ZoomLod : std::uint8_t
    {
        preview = 0,
        noteEdit,
        noteAndVelocityEdit
    };

    static constexpr int previewTrackHeight = clip_height_law::compactTrackHeight;
    static constexpr int noteEditTrackHeight = clip_height_law::mediumTrackHeight;
    static constexpr int noteAndVelocityTrackHeight = clip_height_law::fullTrackHeight;
    static constexpr int editablePitchCount = 25;
    static constexpr int centreC3Pitch = 48;

    struct PitchRange
    {
        std::uint8_t minPitch = 36;
        std::uint8_t maxPitch = 60;
    };

    [[nodiscard]] inline PitchRange pitchRangeCentredAt (const int centrePitch) noexcept
    {
        constexpr int halfRange = (editablePitchCount - 1) / 2;
        const int minPitch = juce::jlimit (0, 128 - editablePitchCount,
                                           centrePitch - halfRange);
        return { static_cast<std::uint8_t> (minPitch),
                 static_cast<std::uint8_t> (minPitch + editablePitchCount - 1) };
    }

    [[nodiscard]] inline ZoomLod zoomLodForTrackHeight (const float trackHeight) noexcept
    {
        if (trackHeight < 120.0f) { return ZoomLod::preview; }
        if (trackHeight < 300.0f) { return ZoomLod::noteEdit; }
        return ZoomLod::noteAndVelocityEdit;
    }

    [[nodiscard]] inline int canonicalTrackHeight (const ZoomLod lod) noexcept
    {
        switch (lod)
        {
            case ZoomLod::preview:                 return previewTrackHeight;
            case ZoomLod::noteEdit:                return noteEditTrackHeight;
            case ZoomLod::noteAndVelocityEdit:     return noteAndVelocityTrackHeight;
        }
        return previewTrackHeight;
    }

    [[nodiscard]] inline ZoomLod nextZoomLod (const ZoomLod lod) noexcept
    {
        switch (lod)
        {
            case ZoomLod::preview:                 return ZoomLod::noteEdit;
            case ZoomLod::noteEdit:                return ZoomLod::noteAndVelocityEdit;
            case ZoomLod::noteAndVelocityEdit:     return ZoomLod::preview;
        }
        return ZoomLod::preview;
    }

    [[nodiscard]] inline juce::Colour colourForVelocity (const std::uint8_t velocity) noexcept
    {
        const float value = juce::jlimit (1.0f, 127.0f, static_cast<float> (velocity));
        if (value <= 45.0f)
            return juce::Colour (0xff38bdf8).interpolatedWith (
                juce::Colour (0xff2dd4bf), (value - 1.0f) / 44.0f);
        if (value <= 95.0f)
            return juce::Colour (0xff34d399).interpolatedWith (
                juce::Colour (0xfffbbf24), (value - 46.0f) / 49.0f);
        return juce::Colour (0xfff97316).interpolatedWith (
            juce::Colour (0xfff43f5e), (value - 96.0f) / 31.0f);
    }

    [[nodiscard]] inline juce::String shortLabelForTarget (const MidiOverlayTarget t)
    {
        switch (t)
        {
            case MidiOverlayTarget::velocityEnvelope: return "Vel Env";
            case MidiOverlayTarget::cc1Modulation:    return "CC1 Mod";
            case MidiOverlayTarget::cc11Expression:   return "CC11 Expr";
            case MidiOverlayTarget::pitchBend:        return "Pitch Bnd";
        }
        return "Vel Env";
    }

    [[nodiscard]] inline juce::Colour colourForTarget (const MidiOverlayTarget t) noexcept
    {
        switch (t)
        {
            case MidiOverlayTarget::velocityEnvelope: return juce::Colours::white;
            case MidiOverlayTarget::cc1Modulation:    return juce::Colour (0xff38bdf8); // Sky cyan
            case MidiOverlayTarget::cc11Expression:   return juce::Colour (0xffa78bfa); // Violet
            case MidiOverlayTarget::pitchBend:        return juce::Colour (0xfffbbf24); // Amber
        }
        return juce::Colours::white;
    }

    [[nodiscard]] inline float defaultNormForTarget (const MidiOverlayTarget t) noexcept
    {
        switch (t)
        {
            case MidiOverlayTarget::velocityEnvelope: return 0.5f;  // 100%
            case MidiOverlayTarget::cc1Modulation:    return 0.0f;  // 0
            case MidiOverlayTarget::cc11Expression:   return 1.0f;  // 127
            case MidiOverlayTarget::pitchBend:        return 0.5f;  // Center (0)
        }
        return 0.5f;
    }

    [[nodiscard]] inline juce::String formatOverlayValue (const MidiOverlayTarget t, float norm)
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        switch (t)
        {
            case MidiOverlayTarget::velocityEnvelope:
            {
                const int pct = juce::roundToInt (norm * 200.0f);
                return juce::String (pct) + "% Vel";
            }
            case MidiOverlayTarget::cc1Modulation:
                return "CC1: " + juce::String (juce::roundToInt (norm * 127.0f));
            case MidiOverlayTarget::cc11Expression:
                return "CC11: " + juce::String (juce::roundToInt (norm * 127.0f));
            case MidiOverlayTarget::pitchBend:
            {
                const float st = (norm - 0.5f) * 24.0f;
                return (st > 0.05f ? "+" : "") + juce::String (st, 1) + " st";
            }
        }
        return {};
    }

    [[nodiscard]] inline MidiOverlayTarget nextTarget (const MidiOverlayTarget t) noexcept
    {
        switch (t)
        {
            case MidiOverlayTarget::velocityEnvelope: return MidiOverlayTarget::cc1Modulation;
            case MidiOverlayTarget::cc1Modulation:    return MidiOverlayTarget::cc11Expression;
            case MidiOverlayTarget::cc11Expression:   return MidiOverlayTarget::pitchBend;
            case MidiOverlayTarget::pitchBend:        return MidiOverlayTarget::velocityEnvelope;
        }
        return MidiOverlayTarget::velocityEnvelope;
    }

    [[nodiscard]] inline juce::String noteName (const int pitch)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int p = juce::jlimit (0, 127, pitch);
        return juce::String (names[p % 12]) + juce::String ((p / 12) - 1);
    }

    /** Il valore MIDI resta 0-based; il numero mostrato all'utente e' 1-based. */
    [[nodiscard]] inline juce::String programChangeLabel (const std::uint8_t program)
    {
        return "PC " + juce::String (static_cast<int> (program) + 1);
    }

    [[nodiscard]] inline bool isBlackKey (const int pitch) noexcept
    {
        const int n = ((pitch % 12) + 12) % 12;
        return (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
    }
}

struct MidiClipLayout
{
    juce::Rectangle<float> headerBounds;
    juce::Rectangle<float> pianoRollBounds;
    juce::Rectangle<float> velocityLaneBounds;
};

[[nodiscard]] inline MidiClipLayout computeMidiClipLayout (
    const juce::Rectangle<float> clipBounds,
    const clip_midi_law::ZoomLod lod) noexcept
{
    MidiClipLayout layout;
    layout.headerBounds = clipBounds.withHeight (juce::jmin (16.0f, clipBounds.getHeight()));
    auto content = clipBounds.withTrimmedTop (layout.headerBounds.getHeight());

    if (lod == clip_midi_law::ZoomLod::noteAndVelocityEdit && content.getHeight() >= 80.0f)
    {
        const float requested = juce::jlimit (72.0f, 112.0f, content.getHeight() * 0.30f);
        const float velocityHeight = juce::jmin (requested, content.getHeight() * 0.45f);
        layout.velocityLaneBounds = content.removeFromBottom (velocityHeight);
        content.removeFromBottom (1.0f);
    }

    layout.pianoRollBounds = content;
    return layout;
}

struct MidiPitchScrollbarGeometry
{
    juce::Rectangle<float> trackBounds;
    juce::Rectangle<float> thumbBounds;
};

[[nodiscard]] inline MidiPitchScrollbarGeometry computeMidiPitchScrollbarGeometry (
    const juce::Rectangle<float> clipBounds,
    const clip_midi_law::ZoomLod lod,
    const std::uint8_t displayMinPitch,
    const std::uint8_t displayMaxPitch,
    const bool onLeft = false) noexcept
{
    MidiPitchScrollbarGeometry geometry;
    if (lod == clip_midi_law::ZoomLod::preview) { return geometry; }

    const auto pianoBounds = computeMidiClipLayout (clipBounds, lod).pianoRollBounds;
    if (pianoBounds.getWidth() < 20.0f || pianoBounds.getHeight() < 24.0f) { return geometry; }

    // Spaziatura dal bordo della clip distanziata a 7px (con larghezza 4px),
    // per garantire un margine confortevole alle maniglie/bracket laterali ed evitare collisioni di hit-test.
    constexpr float width = 4.0f;
    constexpr float gap   = 7.0f;
    const float x = onLeft ? (clipBounds.getX() - gap - width)
                           : (clipBounds.getRight() + gap);
    geometry.trackBounds = { x, pianoBounds.getY(), width, pianoBounds.getHeight() };

    const int minPitch = juce::jlimit (0, 127, static_cast<int> (displayMinPitch));
    const int maxPitch = juce::jlimit (minPitch, 127, static_cast<int> (displayMaxPitch));
    const int visibleCount = maxPitch - minPitch + 1;
    const float thumbHeight = juce::jlimit (14.0f, geometry.trackBounds.getHeight(),
                                            geometry.trackBounds.getHeight()
                                                * static_cast<float> (visibleCount) / 128.0f);
    const float travel = geometry.trackBounds.getHeight() - thumbHeight;
    const float topNorm = 1.0f - (static_cast<float> (minPitch)
                                / static_cast<float> (juce::jmax (1, 128 - visibleCount)));
    geometry.thumbBounds = { x, geometry.trackBounds.getY() + juce::jlimit (0.0f, 1.0f, topNorm) * travel,
                             width, thumbHeight };
    return geometry;
}

/** Singola nota MIDI proiettata in coordinate normalizzate di clip [0..1]. */
struct MidiNoteVisual
{
    /** Indice della nota nell'owner che ha materializzato questa vista.
        Non coincide necessariamente con l'indice visuale: trim e slip possono
        escludere note precedenti dalla cache del frame. */
    int sourceIndex = -1;
    float startNormX = 0.0f;
    float endNormX   = 0.1f;
    std::uint8_t pitch    = 60;
    std::uint8_t velocity = 100;
    std::uint8_t channel  = 1;
    bool selected = false;
    bool muted    = false;
    bool overhangsRight = false;
    bool overhangsLeft  = false;
};

/** Geometria canonica di una nota MIDI, condivisa da paint, hit-test e marquee. */
struct MidiNoteGeometry
{
    juce::Rectangle<float> bounds;
    juce::Point<float> centre;
    float rowHeight = 0.0f;
    float drumRadius = 0.0f;
};

[[nodiscard]] inline MidiNoteGeometry computeMidiNoteGeometry (
    const MidiNoteVisual& note,
    const juce::Rectangle<float> clipBounds,
    const std::uint8_t displayMinPitch,
    const std::uint8_t displayMaxPitch,
    const bool drumMode,
    const clip_midi_law::ZoomLod lod = clip_midi_law::ZoomLod::noteEdit) noexcept
{
    const auto pianoBounds = computeMidiClipLayout (clipBounds, lod).pianoRollBounds;
    const float bodyTop = pianoBounds.getY();
    const float bodyHeight = juce::jmax (10.0f, pianoBounds.getHeight());
    const int minPitch = juce::jlimit (0, 126, static_cast<int> (displayMinPitch));
    const int maxPitch = juce::jlimit (minPitch + 1, 127, static_cast<int> (displayMaxPitch));
    const int pitchSpan = maxPitch - minPitch + 1;
    const float rowHeight = bodyHeight / static_cast<float> (pitchSpan);

    const float startX = clipBounds.getX()
                       + juce::jlimit (0.0f, 1.0f, note.startNormX) * clipBounds.getWidth();
    const float endX = clipBounds.getX()
                     + juce::jlimit (0.0f, 1.0f, note.endNormX) * clipBounds.getWidth();
    if (static_cast<int> (note.pitch) < minPitch || static_cast<int> (note.pitch) > maxPitch)
        return {};

    const int pitchOffset = maxPitch - static_cast<int> (note.pitch);
    const float noteY = bodyTop + static_cast<float> (pitchOffset) * rowHeight;
    const float noteHeight = juce::jlimit (3.0f, 18.0f, std::max (3.0f, rowHeight - 1.0f));

    MidiNoteGeometry geometry;
    geometry.rowHeight = rowHeight;
    geometry.centre = { startX, noteY + noteHeight * 0.5f };

    if (drumMode)
    {
        const float velocityNorm = juce::jlimit (0.15f, 1.0f,
                                                 static_cast<float> (note.velocity) / 127.0f);
        geometry.drumRadius = juce::jlimit (3.5f, 7.5f,
                                            noteHeight * (0.45f + 0.35f * velocityNorm));
        geometry.bounds = juce::Rectangle<float> (
            geometry.centre.x - geometry.drumRadius,
            geometry.centre.y - geometry.drumRadius,
            geometry.drumRadius * 2.0f,
            geometry.drumRadius * 2.0f);
    }
    else
    {
        geometry.bounds = { startX, noteY, std::max (3.5f, endX - startX), noteHeight };
        geometry.centre = geometry.bounds.getCentre();
    }

    return geometry;
}



/** Lane CC non attiva mostrata in ghosting sullo sfondo della clip MIDI. */
struct MidiCcGhostLaneVisual
{
    const juce::Path* normalizedPath = nullptr; // Path in spazio 1000 x 100
    juce::Colour colour { juce::Colours::cyan };
    juce::String shortName;
};

/** Evento puntuale di Program Change (bandierina + linea verticale tratteggiata). */
struct MidiProgramChangeVisual
{
    float normX = 0.0f;
    std::uint8_t program = 0;
    juce::String label;
};

/** Rettangolo canonico della bandierina Program Change.
    Su clip troppo strette la bandierina e' omessa, mentre la linea evento resta
    visibile: questo evita clamp con limiti invertiti e hit-zone fuori dal paint. */
[[nodiscard]] inline juce::Rectangle<float> computeMidiProgramChangeFlagBounds (
    const juce::Rectangle<float> clipBounds, const float normX) noexcept
{
    constexpr float flagWidth = 38.0f;
    constexpr float horizontalMargin = 2.0f;
    constexpr float rightReserve = 42.0f;

    if (clipBounds.getWidth() < rightReserve + horizontalMargin)
        return {};

    const float eventX = clipBounds.getX()
                       + juce::jlimit (0.0f, 1.0f, normX) * clipBounds.getWidth();
    const float flagX = juce::jlimit (clipBounds.getX() + horizontalMargin,
                                      clipBounds.getRight() - rightReserve,
                                      eventX);
    return { flagX, clipBounds.getBottom() - 15.0f, flagWidth, 13.0f };
}

/** Proiezione completa, non autorevole, necessaria al paint di una clip MIDI. */
struct MidiClipVisual
{
    juce::Rectangle<float> bounds;
    ClipFeatures features;
    juce::String title;
    juce::Colour tint { juce::Colours::cyan };
    bool selected   = false;
    bool suspended  = false;
    bool isDrumMode = false;
    clip_midi_law::ZoomLod zoomLod { clip_midi_law::ZoomLod::preview };

    TimelineClipZone hoveredZone { TimelineClipZone::none };
    int hoveredMarkerIdx        = -1;
    int hoveredEnvelopePointIdx = -1;
    int hoveredCurveSegmentIdx  = -1;
    int hoveredNoteIdx          = -1;
    int hoveredProgramChangeIdx = -1;
    float hoverMouseX           = -1.0f;
    bool showWarpAddGhost       = false;
    float warpAddGhostX         = -1.0f;
    bool isDraggingAllEnvelope  = false;
    bool isDraggingTranspose    = false;

    // Feedback interattivo durante il drag (Pitch e Velocity)
    bool isDraggingPitchFeedback = false;
    std::uint8_t draggedPitchValue = 0;
    juce::Point<float> pitchFeedbackPosition;

    bool isDraggingVelocityFeedback = false;
    std::uint8_t draggedVelocityValue = 0;
    bool velocityVisibleInNote = false;
    juce::Point<float> velocityFeedbackPosition;

    // Range verticale di pitch calcolato per l'auto-fit
    std::uint8_t displayMinPitch = 48;
    std::uint8_t displayMaxPitch = 72;
    bool showPitchScrollbar = false;
    bool pitchScrollbarOnLeft = false;
    bool pitchScrollbarActive = false;

    // Note MIDI visibili
    const MidiNoteVisual* notes = nullptr;
    int numNotes = 0;

    // Warp Markers (Linked Note+CC oppure Unlinked Solo-Note)
    const float* warpMarkersNorm     = nullptr;
    const float* origWarpMarkersNorm = nullptr;
    int numWarpMarkers = 0;
    double clipLengthSec = 4.0;
    bool isUnlinkedNoteWarp = false;

    // Curva di controllo attiva in primo piano
    MidiOverlayTarget activeOverlay { MidiOverlayTarget::velocityEnvelope };
    const MidiEnvelopePoint* overlayPoints = nullptr;
    int numOverlayPoints = 0;

    // Tracce CC fantasma (ghost) + pallini di attivita' sul Pill Switcher
    const MidiCcGhostLaneVisual* ghostLanes = nullptr;
    int numGhostLanes = 0;

    // Program Changes
    const MidiProgramChangeVisual* programChanges = nullptr;
    int numProgramChanges = 0;

    // Offset non distruttivi di clip
    int transposeSemitones = 0;
    int velocityOffset     = 0;
    std::int64_t stretchDeltaTicks = 0;
    bool isStretchActive   = false;
    std::int64_t slipDeltaTicks = 0;
    bool isSlipActive      = false;
    ClipOverlapVisual overlap;
};


/** Disegna l'indicazione grafica di Overlap o XFade dentro la clip superiore:
    - Tratteggio diagonale nell'header della zona di overlap;
    - Linea tratteggiata verticale con parentesi nel punto in cui finisce/inizia la clip nascosta sotto;
    - In XFade (Audio/Image/Line): mostra la rampa di uscita della clip sottostante con punto e curve handle;
    - In Overlap normale: mostra in ghost tratteggiato la curva coperta (e il gradino verticale per LineClip);
    - In MIDI: mostra le note della clip sottostante (vuote tratteggiate in replace, colorate in merge). */
inline void drawClipOverlapDecoration (
    juce::Graphics& g,
    const juce::Rectangle<float>& clipBounds,
    const juce::Rectangle<float>& curveAreaBounds,
    const ClipOverlapVisual& ov,
    const bool isMidiClip = false,
    const std::uint8_t displayMinPitch = 48,
    const std::uint8_t displayMaxPitch = 72,
    const bool isDrumMode = false,
    const clip_midi_law::ZoomLod midiLod = clip_midi_law::ZoomLod::preview)
{
    if (! ov.hasOverlap || ! ov.isTopClip)
        return;

    namespace tk = ayra::tokens;

    const float x0 = clipBounds.getX() + juce::jlimit (0.0f, 1.0f, ov.startNormX) * clipBounds.getWidth();
    const float x1 = clipBounds.getX() + juce::jlimit (0.0f, 1.0f, ov.endNormX)   * clipBounds.getWidth();
    if (x1 <= x0 + 1.5f)
        return;

    const juce::Rectangle<float> ovRect (x0, clipBounds.getY(), x1 - x0, clipBounds.getHeight());
    const auto seamCol = ov.isCrossfade ? tk::accent() : ov.partnerTint.brighter (0.35f);

    // 1. Wash traslucido sulla regione di overlap
    g.setColour (seamCol.withAlpha (ov.isCrossfade ? 0.13f : 0.08f));
    g.fillRect (ovRect);

    // 2. Tratteggio diagonale nell'header della zona di overlap
    const auto headerStrip = ovRect.withHeight (juce::jmin (clipTitleAreaHeight, ovRect.getHeight()));
    {
        const juce::Graphics::ScopedSaveState hatchScope (g);
        g.reduceClipRegion (headerStrip.toNearestInt());
        g.setColour (seamCol.withAlpha (0.30f));
        for (float hx = headerStrip.getX() - headerStrip.getHeight(); hx < headerStrip.getRight(); hx += 6.5f)
            g.drawLine (hx, headerStrip.getBottom(), hx + headerStrip.getHeight(), headerStrip.getY(), 1.1f);
    }

    // 3. Linea di confine della clip nascosta sotto
    auto drawHiddenClipBoundary = [&] (float seamX, bool pointsInwardLeft)
    {
        g.setColour (seamCol.withAlpha (0.88f));
        for (float cy = clipBounds.getY() + 2.0f; cy < clipBounds.getBottom() - 2.0f; cy += 6.0f)
            g.drawVerticalLine (juce::roundToInt (seamX), cy, std::min (cy + 3.5f, clipBounds.getBottom() - 2.0f));

        const float arm = pointsInwardLeft ? -4.5f : 4.5f;
        g.drawLine (seamX, clipBounds.getY() + 1.5f, seamX + arm, clipBounds.getY() + 1.5f, 1.6f);
        g.drawLine (seamX, clipBounds.getBottom() - 1.5f, seamX + arm, clipBounds.getBottom() - 1.5f, 1.6f);
    };

    if (ov.endNormX < 0.998f)
        drawHiddenClipBoundary (x1, true);
    if (ov.startNormX > 0.002f)
        drawHiddenClipBoundary (x0, false);

    // 4. Note della clip MIDI sottostante
    if (isMidiClip && ov.partnerNotes != nullptr && ov.numPartnerNotes > 0)
    {
        for (int ni = 0; ni < ov.numPartnerNotes; ++ni)
        {
            const auto& pn = ov.partnerNotes[ni];
            if (pn.endNormX < ov.startNormX || pn.startNormX > ov.endNormX)
                continue;

            const auto geom = computeMidiNoteGeometry (
                pn, clipBounds, displayMinPitch, displayMaxPitch, isDrumMode, midiLod);
            if (geom.bounds.isEmpty())
                continue;

            const auto clippedNote = geom.bounds.getIntersection (ovRect);
            if (clippedNote.isEmpty())
                continue;

            const auto velCol = clip_midi_law::colourForVelocity (pn.velocity);
            if (ov.midiMode == MidiOverlapMode::merge)
            {
                g.setColour (velCol.withAlpha (0.34f));
                g.fillRoundedRectangle (clippedNote, 2.0f);
                g.setColour (velCol.brighter (0.2f).withAlpha (0.72f));
                g.drawRoundedRectangle (clippedNote, 2.0f, 0.9f);
            }
            else
            {
                juce::Path border;
                border.addRoundedRectangle (clippedNote, 2.0f);
                juce::Path dashed;
                constexpr float dashes[] = { 3.0f, 2.5f };
                juce::PathStrokeType (0.9f).createDashedStroke (dashed, border, dashes, 2);
                g.setColour (velCol.withAlpha (0.38f));
                g.fillPath (dashed);
            }
        }
    }

    // 5. Curva della clip sottostante (Rampa XFade oppure Curva Ghost in Overlap)
    if (ov.showPartnerCurve && ! curveAreaBounds.isEmpty())
    {
        const float cx0 = curveAreaBounds.getX() + juce::jlimit (0.0f, 1.0f, ov.startNormX) * curveAreaBounds.getWidth();
        const float cx1 = curveAreaBounds.getX() + juce::jlimit (0.0f, 1.0f, ov.endNormX)   * curveAreaBounds.getWidth();
        const float py0 = curveAreaBounds.getBottom() - juce::jlimit (0.0f, 1.0f, ov.partnerStartNormY) * curveAreaBounds.getHeight();
        const float py1 = curveAreaBounds.getBottom() - juce::jlimit (0.0f, 1.0f, ov.partnerEndNormY)   * curveAreaBounds.getHeight();

        const auto seg = makeBoundedCurvePixelSegment (
            { cx0, py0 }, { cx1, py1 }, ov.partnerCurveControl);

        juce::Path partnerPath;
        partnerPath.startNewSubPath (seg.start);
        appendBoundedCurvePixelSegment (partnerPath, seg);

        if (ov.isCrossfade)
        {
            g.setColour (juce::Colour (0x55000000));
            g.strokePath (partnerPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ov.partnerTint.brighter (0.45f).withAlpha (0.92f));
            g.strokePath (partnerPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float hr = ov.partnerCurveHovered ? 4.5f : 3.5f;
            const auto hCol = tk::accent().brighter (0.25f);
            const juce::Rectangle<float> hDot (seg.control.x - hr, seg.control.y - hr, hr * 2.0f, hr * 2.0f);
            if (ov.partnerCurveHovered)
            {
                g.setColour (hCol.withAlpha (0.35f));
                g.fillEllipse (hDot.expanded (3.0f));
                g.setColour (hCol);
                g.fillEllipse (hDot);
            }
            g.setColour (hCol);
            g.drawEllipse (hDot, 1.4f);

            const float pr = (ov.partnerPointHovered || ov.partnerPointSelected) ? 5.5f : 4.0f;
            const juce::Rectangle<float> pDot (cx0 - pr, py0 - pr, pr * 2.0f, pr * 2.0f);
            g.setColour ((ov.partnerPointHovered || ov.partnerPointSelected) ? tk::accent() : ov.partnerTint.brighter (0.5f));
            g.fillEllipse (pDot);
            g.setColour (juce::Colours::white);
            g.drawEllipse (pDot.reduced (0.5f), 1.2f);
        }
        else
        {
            juce::Path dashedPartner;
            constexpr float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashedPartner, partnerPath, dashes, 2);
            g.setColour (ov.partnerTint.brighter (0.35f).withAlpha (0.42f));
            g.fillPath (dashedPartner);

            if (ov.showStepJumpAtStart)
            {
                const float topStartY = curveAreaBounds.getBottom()
                                      - juce::jlimit (0.0f, 1.0f, ov.topClipStartNormY) * curveAreaBounds.getHeight();
                juce::Path jumpPath;
                jumpPath.startNewSubPath (cx0, py0);
                jumpPath.lineTo (cx0, topStartY);
                juce::Path dashedJump;
                juce::PathStrokeType (1.5f).createDashedStroke (dashedJump, jumpPath, dashes, 2);
                g.setColour (juce::Colours::white.withAlpha (0.70f));
                g.fillPath (dashedJump);
            }
        }
    }

    // 6. Badge compatto dello stato di Overlap ("XFADE" / "MERGE" / "OVERLAP")
    if (ovRect.getWidth() >= 46.0f && clipBounds.getHeight() >= 40.0f)
    {
        const juce::String badgeText = ov.isCrossfade ? "XFADE"
                                     : (isMidiClip && ov.midiMode == MidiOverlapMode::merge) ? "MERGE"
                                     : "OVERLAP";
        const juce::Colour badgeCol = ov.isCrossfade ? tk::accent()
                                    : (isMidiClip && ov.midiMode == MidiOverlapMode::merge) ? tk::streamAudio()
                                    : seamCol;

        const float bw = juce::jmin (54.0f, ovRect.getWidth() - 6.0f);
        const float bh = 13.0f;
        const float bx = ovRect.getCentreX() - bw * 0.5f;
        const float by = clipBounds.getY() + clipTitleAreaHeight + 3.0f;
        const juce::Rectangle<float> pillRect (bx, by, bw, bh);

        g.setColour (juce::Colour (0xd80b171e));
        g.fillRoundedRectangle (pillRect, bh * 0.5f);
        g.setColour (badgeCol.withAlpha (0.90f));
        g.drawRoundedRectangle (pillRect, bh * 0.5f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (tk::type::caption().boldened().withHeight (8.5f));
        g.drawText (badgeText, pillRect.toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
// PROIEZIONI VISUALI LINE CLIP E LANE GLOBALI
//==============================================================================

/** Comportamento richiesto all'host quando termina una LineClip.
    E' metadata visuale/configurativo: il widget non emette eventi MIDI. */
enum class LineClipEndBehavior : std::uint8_t
{
    holdLastOrSilent = 0,
    autoReturnToNeutral
};

enum class LineClipRenderMode : std::uint8_t
{
    continuousCurve = 0,
    gateBlock
};

/** Proiezione non autorevole di una curva limitata ai confini della clip. */
struct LineClipVisual
{
    juce::Rectangle<float> bounds;
    ClipFeatures features;
    juce::String title;
    juce::Colour tint { juce::Colours::cyan };
    juce::Colour curveColour { juce::Colours::white };
    ayra::TimelineCurveUnderlayStyle curveUnderlayStyle {
        ayra::TimelineCurveUnderlayStyle::darkBelow };
    bool selected = false;
    bool suspended = false;

    AutomationBoundaryPolicy boundaryPolicy { AutomationBoundaryPolicy::pinnedEndpoints };
    float boundaryStartNormValue = 0.0f;
    float boundaryEndNormValue = 0.0f;
    ayra::curves::QuadraticControl fadeInControl {};
    ayra::curves::QuadraticControl fadeOutControl {};
    LineClipRenderMode renderMode { LineClipRenderMode::continuousCurve };
    LineClipEndBehavior endBehavior { LineClipEndBehavior::holdLastOrSilent };
    float defaultNormValue = 0.0f;
    float minDisplayValue = 0.0f;
    float maxDisplayValue = 127.0f;
    juce::String valueUnit;

    const MidiEnvelopePoint* points = nullptr;
    int numPoints = 0;
    const float* warpMarkersNorm = nullptr;
    const float* origWarpMarkersNorm = nullptr;
    int numWarpMarkers = 0;
    double clipLengthSec = 0.0;

    TimelineClipZone hoveredZone { TimelineClipZone::none };
    int hoveredMarkerIdx = -1;
    int hoveredPointIdx = -1;
    int hoveredCurveSegmentIdx = -1;
    float hoverMouseX = -1.0f;
    bool showWarpAddGhost = false;
    float warpAddGhostX = -1.0f;
    bool isDraggingAllPoints = false;

    std::int64_t stretchDeltaTicks = 0;
    bool isStretchActive = false;
    std::int64_t slipDeltaTicks = 0;
    bool isSlipActive = false;
    ClipOverlapVisual overlap;
};

struct KeyframeEventVisual
{
    int sourceIndex = -1;
    float startNormX = 0.0f;
    float endNormX = 0.0f;
    juce::String label;
    juce::Colour colour { juce::Colours::cyan };
    bool selected = false;
    int visualRow = 0;
    float pillWidth = 22.0f;
    bool showBottomRemovalRail = false;
};

namespace keyframe_lane_layout
{
    static constexpr float padY = 3.0f;
    static constexpr float pillHeight = 18.0f;
    static constexpr float rowGap = 4.0f;
    static constexpr int maxRows = 6;

    /** Rettangolo visuale canonico del LookAndFeel.
        Per gli span la larghezza deriva dal prossimo evento; per i point dal testo.
        Le hit-zone restano deliberate responsabilita' del canvas, cosi' un cambio
        estetico non altera le gesture esistenti. */
    [[nodiscard]] inline juce::Rectangle<float> eventBounds (
        const juce::Rectangle<float> laneBounds,
        const KeyframeEventVisual& event,
        const bool spanMode) noexcept
    {
        const float startNorm = juce::jlimit (0.0f, 1.0f, event.startNormX);
        const float x0 = laneBounds.getX() + startNorm * laneBounds.getWidth();
        const float x1 = laneBounds.getX()
                       + juce::jlimit (startNorm, 1.0f, event.endNormX) * laneBounds.getWidth();
        const float width = spanMode ? juce::jmax (1.0f, x1 - x0)
                                     : juce::jmax (22.0f, event.pillWidth);
        const float y = laneBounds.getY() + padY
                      + static_cast<float> (juce::jlimit (0, maxRows - 1, event.visualRow))
                        * (pillHeight + rowGap);
        return { x0, y, width, pillHeight };
    }
}

/** Tipologia funzionale della corsia di Keyframe. */
enum class KeyframeLaneType : std::uint8_t
{
    points = 0,          ///< Eventi puntuali (Marker / Cue)
    contiguousSpans = 1, ///< Mode 1: Partizioni continue (primo ancorato a 0, ultimo infinito/fine timeline, senza buchi)
    disjointSpans = 2    ///< Mode 2: Intervalli finiti con inizio e fine, buchi permessi, no overlap, rimozione barra sotto
};

using KeyframeSpanKind = KeyframeLaneType;

/** Proiezione di una lane di eventi puntuali o estesi fino all'evento seguente. */
struct KeyframeLaneVisual
{
    juce::Rectangle<float> bounds;
    juce::Colour tint { juce::Colours::cyan };
    bool suspended = false;
    bool spanToNextEvent = true;
    KeyframeLaneType laneType = KeyframeLaneType::contiguousSpans;
    const KeyframeEventVisual* events = nullptr;
    int numEvents = 0;
    int hoveredEventIdx = -1;
    int hoveredBottomRailIdx = -1;
    float hoverCutX = -1.0f;
};

/** Proiezione di una curva globale che copre l'intera timeline visibile. */
struct ContinuousLineVisual
{
    juce::Rectangle<float> bounds;
    juce::Colour tint { juce::Colours::cyan };
    juce::Colour curveColour { juce::Colours::white };
    juce::Colour activeNodeColour { juce::Colours::orange };
    bool suspended = false;
    float minDisplayValue = 0.0f;
    float maxDisplayValue = 1.0f;
    float defaultDisplayValue = 0.0f;
    int valueDecimals = 1;
    bool showLeftScale = true;
    const MidiEnvelopePoint* points = nullptr;
    int numPoints = 0;
    TimelineClipZone hoveredZone { TimelineClipZone::none };
    int hoveredPointIdx = -1;
    int hoveredSegmentIdx = -1;
};

struct ContinuousLineLayout
{
    juce::Rectangle<float> scaleBounds;
    juce::Rectangle<float> graphBounds;
};

/** Geometria unica della lane continua, condivisa da paint e gesture. */
[[nodiscard]] inline ContinuousLineLayout computeContinuousLineLayout (
    const juce::Rectangle<float> bounds,
    const bool showLeftScale) noexcept
{
    constexpr float scaleWidth = 42.0f;
    ContinuousLineLayout layout;
    layout.scaleBounds = showLeftScale ? bounds.withWidth (scaleWidth) : juce::Rectangle<float>();
    layout.graphBounds = (showLeftScale ? bounds.withTrimmedLeft (scaleWidth) : bounds).reduced (2.0f);
    return layout;
}

} // namespace ayra::lab::clips
