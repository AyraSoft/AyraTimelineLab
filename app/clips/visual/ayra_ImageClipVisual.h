#pragma once

namespace ayra::lab::clips
{

/** Punto dell'inviluppo di Opacita' (0% .. 100%) per una ImageClip.
    Ricalca esattamente `GainPoint` dell'audio per coerenza totale di gesture. */
struct OpacityPoint
{
    float normX   = 0.5f;
    float opacity = 1.0f; // [0.0 = 0% trasparente .. 1.0 = 100% opaco]
    float tension = 0.5f; // Controllo Y relativo legacy [0..1]
    bool selected = false;
    float controlX = -1.0f; // [0..1] nel segmento; <0 usa la tensione legacy
    float controlY = -1.0f; // [0..1] nel rettangolo degli endpoint
    float controlWeight = -1.0f; // <=0 usa la sharpness automatica
};

[[nodiscard]] inline AutomationPoint opacityPointAsAutomationPoint (
    const OpacityPoint& point) noexcept
{
    return {
        point.normX, point.opacity, point.tension, point.selected,
        point.controlX, point.controlY, point.controlWeight
    };
}

[[nodiscard]] inline OpacityPoint automationPointAsOpacityPoint (
    const AutomationPoint& point) noexcept
{
    return {
        point.normX, point.normY, point.tension, point.selected,
        point.controlX, point.controlY, point.controlWeight
    };
}

namespace clip_opacity_law
{
    [[nodiscard]] inline juce::String formatOpacityPercentage (const float opacity01)
    {
        const int pct = juce::roundToInt (juce::jlimit (0.0f, 1.0f, opacity01) * 100.0f);
        return juce::String (pct) + "%";
    }

    [[nodiscard]] inline float opacityToNorm (const float opacity01) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, opacity01);
    }

    [[nodiscard]] inline float normToOpacity (const float norm) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, norm);
    }

    /** Valuta il punto di controllo quadratico tra due OpacityPoint. */
    [[nodiscard]] inline float opacityControlValue (
        const OpacityPoint& start, const OpacityPoint& end) noexcept
    {
        return start.opacity + juce::jlimit (0.0f, 1.0f, start.tension)
            * (end.opacity - start.opacity);
    }

    [[nodiscard]] inline bool hasExplicitOpacityControl (
        const OpacityPoint& point) noexcept
    {
        return std::isfinite (point.controlX) && std::isfinite (point.controlY)
            && point.controlX >= 0.0f;
    }

    [[nodiscard]] inline juce::Point<float> opacityControlPoint (
        const OpacityPoint& start, const OpacityPoint& end) noexcept
    {
        const ayra::curves::QuadraticControl control = hasExplicitOpacityControl (start)
            ? ayra::curves::QuadraticControl {
                  start.controlX, start.controlY, start.controlWeight }
            : ayra::curves::QuadraticControl { 0.5, start.tension };
        const auto projected = ayra::curves::boundedQuadraticControlPoint ({
            start.normX, start.opacity, end.normX, end.opacity, control });
        return { static_cast<float> (projected.x), static_cast<float> (projected.y) };
    }

    inline void setOpacityControlPoint (
        OpacityPoint& start, const OpacityPoint& end,
        const float normX, const float opacity) noexcept
    {
        const auto control = ayra::curves::makeBoundedQuadraticControl (
            { start.normX, start.opacity, end.normX, end.opacity },
            normX, juce::jlimit (0.0f, 1.0f, opacity));
        start.controlX = static_cast<float> (control.x);
        start.controlY = static_cast<float> (control.y);
        start.controlWeight = -1.0f;
        start.tension = 0.5f;
    }

    inline void resetOpacityControlPoint (
        OpacityPoint& start, const OpacityPoint& end) noexcept
    {
        start.controlX = 0.5f;
        start.controlY = 0.5f;
        start.controlWeight = -1.0f;
        start.tension = 0.5f;
    }

    /** Valuta il segmento bounded di opacita'. Paint, hit-test e split
        condividono questa unica geometria e non possono produrre overshoot. */
    [[nodiscard]] inline float evaluateOpacitySegmentUnclamped (
        const OpacityPoint& start,
        const OpacityPoint& end,
        const float normX) noexcept
    {
        const float span = std::max (0.0001f, end.normX - start.normX);
        const double localX = juce::jlimit (
            0.0f, 1.0f, (normX - start.normX) / span);
        const ayra::curves::QuadraticControl control = hasExplicitOpacityControl (start)
            ? ayra::curves::QuadraticControl {
                  start.controlX, start.controlY, start.controlWeight }
            : ayra::curves::QuadraticControl { 0.5, start.tension };
        return static_cast<float> (ayra::curves::evaluateBoundedQuadraticValue (
            localX, start.opacity, control, end.opacity));
    }

    struct OpacitySegmentSplit
    {
        float cutValue = 0.5f;
        ayra::curves::QuadraticControl leftControl {};
        ayra::curves::QuadraticControl rightControl {};
    };

    [[nodiscard]] inline OpacitySegmentSplit splitOpacitySegment (
        const OpacityPoint& start,
        const OpacityPoint& end,
        const float splitNormX) noexcept
    {
        const ayra::curves::QuadraticControl control = hasExplicitOpacityControl (start)
            ? ayra::curves::QuadraticControl {
                  start.controlX, start.controlY, start.controlWeight }
            : ayra::curves::QuadraticControl { 0.5, start.tension };
        const auto split = ayra::curves::splitBoundedQuadraticAtX (
            { start.normX, start.opacity, end.normX, end.opacity, control },
            splitNormX);
        return {
            static_cast<float> (split.y),
            split.leftControl,
            split.rightControl
        };
    }

    /** Valuta l'opacita' effettiva [0..1] in una coordinata normalizzata X [0..1],
        includendo i Fade-In / Fade-Out automatici quando il primo o l'ultimo punto
        vengono spostati verso l'interno della clip. */
    [[nodiscard]] inline float evaluateOpacityAt (
        const OpacityPoint* points,
        const int numPoints,
        const float baseOpacity,
        const float normX,
        const ayra::curves::QuadraticControl fadeInControl = {},
        const ayra::curves::QuadraticControl fadeOutControl = {}) noexcept
    {
        if (points == nullptr || numPoints < 2)
            return juce::jlimit (0.0f, 1.0f, baseOpacity);

        ayra::curves::CurveBoundarySpec boundary;
        boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
        boundary.fadeInControl = fadeInControl;
        boundary.fadeOutControl = fadeOutControl;
        return evaluateProjectedAutomationCurveAt (
            points, numPoints, baseOpacity, boundary, normX,
            [] (const OpacityPoint& point) noexcept
            {
                return opacityPointAsAutomationPoint (point);
            });
    }
} // namespace clip_opacity_law

/** Singolo slot fotogramma proiettato nella filmstrip della clip.
    Il modulo non decodifica file e non gestisce il playback: l'applicazione
    fornisce un'immagine thumbnail opzionale (o lascia `thumbnail` null per
    usare il fallback vettoriale procedurale del LookAndFeel) e l'indice del
    fotogramma sorgente warppato che cade in quello slot. */
struct ImageFrameVisual
{
    float startNormX = 0.0f;          ///< Inizio dello slot nella clip [0..1]
    float endNormX   = 0.1f;          ///< Fine dello slot nella clip [0..1]
    int sourceFrameIndex = 0;         ///< Indice del fotogramma sorgente (0-based)
    juce::Image thumbnail {};         ///< Bitmap pre-scalata opzionale fornita dall'host
};

/** Proiezione completa, non autorevole, necessaria al paint di una ImageClip.
    I puntatori sono view valide per la sola chiamata di `paint()`. */
struct ImageClipVisual
{
    juce::Rectangle<float> bounds;
    ClipFeatures features;
    juce::String title;
    juce::Colour tint { juce::Colour (0xff19b6e6) }; // Default: tokens::streamVideo()
    bool selected  = false;
    bool suspended = false;

    TimelineClipZone hoveredZone { TimelineClipZone::none };
    int hoveredMarkerIdx       = -1;
    int hoveredOpacityPointIdx = -1;
    int hoveredCurveSegmentIdx = -1;
    float hoverMouseX          = -1.0f;
    bool showWarpAddGhost      = false;
    float warpAddGhostX        = -1.0f;
    bool isDraggingAllOpacity  = false;

    // Metadati sequenza immagini (forniti dall'applicazione host)
    float frameRate        = 24.0f;
    int totalSourceFrames  = 120;
    float frameAspectRatio = 16.0f / 9.0f;
    int visualSeed         = 101;
    juce::String sequenceFormatText; // es. "24 fps / 120 frm"

    // Slot fotogrammi visibili (gia' mappati attraverso il Warp)
    const ImageFrameVisual* frames = nullptr;
    int numFrames = 0;

    // Inviluppo di Opacita' (0% .. 100%)
    float baseOpacity = 1.0f;
    ayra::curves::QuadraticControl fadeInControl {};
    ayra::curves::QuadraticControl fadeOutControl {};
    const OpacityPoint* opacityPoints = nullptr;
    int numOpacityPoints = 0;

    // Warp Markers (identici a AudioClipVisual)
    const float* warpMarkersNorm     = nullptr;
    const float* origWarpMarkersNorm = nullptr;
    int numWarpMarkers = 0;
    double clipLengthSec = 0.0;

    // Scostamenti Stretch / Slip (in fotogrammi)
    std::int64_t stretchDeltaFrames = 0;
    bool isStretchActive = false;
    std::int64_t slipDeltaFrames = 0;
    bool isSlipActive = false;
};

} // namespace ayra::lab::clips
