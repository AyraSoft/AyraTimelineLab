#pragma once

#include <functional>

namespace ayra::lab::clips
{

//==============================================================================
// CROSSFADE COMPENSATION SOLVER CONTRACTS (Engine-UI Delegation)
//==============================================================================

/** Tipo di interazione che richiede la compensazione del crossfade dall'engine. */
enum class CrossfadeInteractionKind : std::uint8_t
{
    levelChange,      ///< Spostamento verticale del punto di fade (gain / value / alpha)
    curvatureChange,  ///< Modifica della maniglia di curvatura del fade
    boundaryResize    ///< Variazione della finestra temporale di overlap
};

//------------------------------------------------------------------------------
// Audio Crossfade Solver
//------------------------------------------------------------------------------

/** Contesto di calcolo per il crossfade audio sottoposto all'engine. */
struct AudioCrossfadeContext
{
    CrossfadeInteractionKind interaction = CrossfadeInteractionKind::levelChange;
    std::string modifiedClipId;      ///< Id della clip direttamente modificata dal gesto
    bool isLeftClipModified = false; ///< true se la clip mossa e' quella uscente (sinistra)

    // Clip Uscente (Left / Outgoing)
    std::string leftClipId;
    float currentLeftGainDb = 0.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    // Clip Entrante (Right / Incoming)
    std::string rightClipId;
    float currentRightGainDb = 0.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };

    // Parametri dell'interazione
    float requestedGainDb = 0.0f; ///< Nuovo dB richiesto per la clip modificata
    ayra::curves::QuadraticControl requestedControl { 0.5, 0.5, -1.0 }; ///< Nuovo controllo se curvatureChange
    double overlapDurationSec = 0.0;
};

/** Risultato matematico restituito dall'engine per le due clip in crossfade audio. */
struct AudioCrossfadeResult
{
    float leftGainDb = 0.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    float rightGainDb = 0.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };
};

using AudioCrossfadeSolver = std::function<AudioCrossfadeResult (const AudioCrossfadeContext&)>;

/** Solver di default canonico per audio (Equal-Power & Livelli Indipendenti).
    Preserva l'indipendenza dei livelli nominali delle due clip e calcola
    la curvatura coniugata complementare per mantenere la somma di potenza. */
[[nodiscard]] inline AudioCrossfadeResult defaultAudioCrossfadeSolver (const AudioCrossfadeContext& ctx) noexcept
{
    AudioCrossfadeResult res;
    // 1. Indipendenza dei guadagni: la clip modificata assume il valore richiesto,
    //    l'altra clip mantiene inalterato il proprio guadagno nominale.
    if (ctx.interaction == CrossfadeInteractionKind::levelChange)
    {
        if (ctx.isLeftClipModified)
        {
            res.leftGainDb = ctx.requestedGainDb;
            res.rightGainDb = ctx.currentRightGainDb;

            // Ricalcola la maniglia coniugata della clip partner (right / in)
            // in modo che il profilo del crossfade mantenga la coniugazione a pari potenza / lineare
            res.leftFadeOutControl = ctx.leftFadeOutControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.leftFadeOutControl.x),
                juce::jlimit (0.0, 1.0, ctx.leftFadeOutControl.y),
                ctx.leftFadeOutControl.weight
            };
        }
        else
        {
            res.leftGainDb = ctx.currentLeftGainDb;
            res.rightGainDb = ctx.requestedGainDb;

            // Ricalcola la maniglia coniugata della clip partner (left / out)
            res.rightFadeInControl = ctx.rightFadeInControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.rightFadeInControl.x),
                juce::jlimit (0.0, 1.0, ctx.rightFadeInControl.y),
                ctx.rightFadeInControl.weight
            };
        }
    }
    else if (ctx.interaction == CrossfadeInteractionKind::curvatureChange)
    {
        res.leftGainDb = ctx.currentLeftGainDb;
        res.rightGainDb = ctx.currentRightGainDb;

        // 2. Rispecchiamento speculare coniugato della curvatura:
        //    Se l'utente modifica una delle due curve, la gemella viene coniugata
        //    in modo che l'ampiezza di curvatura sia complementare.
        if (ctx.isLeftClipModified)
        {
            res.leftFadeOutControl = ctx.requestedControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
        else
        {
            res.rightFadeInControl = ctx.requestedControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
    }
    else
    {
        res.leftGainDb = ctx.currentLeftGainDb;
        res.rightGainDb = ctx.currentRightGainDb;
        res.leftFadeOutControl = ctx.leftFadeOutControl;
        res.rightFadeInControl = ctx.rightFadeInControl;
    }
    return res;
}

//------------------------------------------------------------------------------
// Line Crossfade Solver
//------------------------------------------------------------------------------

/** Contesto di calcolo per il crossfade di automazione (LineClip) sottoposto all'engine. */
struct LineCrossfadeContext
{
    CrossfadeInteractionKind interaction = CrossfadeInteractionKind::levelChange;
    std::string modifiedClipId;
    bool isLeftClipModified = false;

    std::string leftClipId;
    float currentLeftValue = 0.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    std::string rightClipId;
    float currentRightValue = 0.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };

    float requestedValue = 0.0f;
    ayra::curves::QuadraticControl requestedControl { 0.5, 0.5, -1.0 };
    double overlapDurationSec = 0.0;
};

/** Risultato matematico restituito dall'engine per le due clip in crossfade di automazione. */
struct LineCrossfadeResult
{
    float leftValue = 0.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    float rightValue = 0.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };
};

using LineCrossfadeSolver = std::function<LineCrossfadeResult (const LineCrossfadeContext&)>;

/** Solver di default per LineClip: preserva i valori nominali ed esegue
    la coniugazione speculare delle maniglie di curvatura. */
[[nodiscard]] inline LineCrossfadeResult defaultLineCrossfadeSolver (const LineCrossfadeContext& ctx) noexcept
{
    LineCrossfadeResult res;
    if (ctx.interaction == CrossfadeInteractionKind::levelChange)
    {
        if (ctx.isLeftClipModified)
        {
            res.leftValue = ctx.requestedValue;
            res.rightValue = ctx.currentRightValue;

            // Ricalcola la maniglia coniugata della clip partner (right / in)
            res.leftFadeOutControl = ctx.leftFadeOutControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.leftFadeOutControl.x),
                juce::jlimit (0.0, 1.0, ctx.leftFadeOutControl.y),
                ctx.leftFadeOutControl.weight
            };
        }
        else
        {
            res.leftValue = ctx.currentLeftValue;
            res.rightValue = ctx.requestedValue;

            // Ricalcola la maniglia coniugata della clip partner (left / out)
            res.rightFadeInControl = ctx.rightFadeInControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.rightFadeInControl.x),
                juce::jlimit (0.0, 1.0, ctx.rightFadeInControl.y),
                ctx.rightFadeInControl.weight
            };
        }
    }
    else if (ctx.interaction == CrossfadeInteractionKind::curvatureChange)
    {
        res.leftValue = ctx.currentLeftValue;
        res.rightValue = ctx.currentRightValue;
        if (ctx.isLeftClipModified)
        {
            res.leftFadeOutControl = ctx.requestedControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
        else
        {
            res.rightFadeInControl = ctx.requestedControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
    }
    else
    {
        res.leftValue = ctx.currentLeftValue;
        res.rightValue = ctx.currentRightValue;
        res.leftFadeOutControl = ctx.leftFadeOutControl;
        res.rightFadeInControl = ctx.rightFadeInControl;
    }
    return res;
}

//------------------------------------------------------------------------------
// Image Crossfade Solver
//------------------------------------------------------------------------------

/** Contesto di calcolo per il crossfade di opacita' (ImageClip) sottoposto all'engine. */
struct ImageCrossfadeContext
{
    CrossfadeInteractionKind interaction = CrossfadeInteractionKind::levelChange;
    std::string modifiedClipId;
    bool isLeftClipModified = false;

    std::string leftClipId;
    float currentLeftAlpha = 1.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    std::string rightClipId;
    float currentRightAlpha = 1.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };

    float requestedAlpha = 1.0f;
    ayra::curves::QuadraticControl requestedControl { 0.5, 0.5, -1.0 };
    double overlapDurationSec = 0.0;
};

/** Risultato matematico restituito dall'engine per le due clip in crossfade di immagini. */
struct ImageCrossfadeResult
{
    float leftAlpha = 1.0f;
    ayra::curves::QuadraticControl leftFadeOutControl { 0.5, 0.5, -1.0 };

    float rightAlpha = 1.0f;
    ayra::curves::QuadraticControl rightFadeInControl { 0.5, 0.5, -1.0 };
};

using ImageCrossfadeSolver = std::function<ImageCrossfadeResult (const ImageCrossfadeContext&)>;

/** Solver di default per ImageClip: transizione complementare di trasparenza. */
[[nodiscard]] inline ImageCrossfadeResult defaultImageCrossfadeSolver (const ImageCrossfadeContext& ctx) noexcept
{
    ImageCrossfadeResult res;
    if (ctx.interaction == CrossfadeInteractionKind::levelChange)
    {
        if (ctx.isLeftClipModified)
        {
            res.leftAlpha = ctx.requestedAlpha;
            res.rightAlpha = ctx.currentRightAlpha;

            // Ricalcola la maniglia coniugata della clip partner (right / in)
            res.leftFadeOutControl = ctx.leftFadeOutControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.leftFadeOutControl.x),
                juce::jlimit (0.0, 1.0, ctx.leftFadeOutControl.y),
                ctx.leftFadeOutControl.weight
            };
        }
        else
        {
            res.leftAlpha = ctx.currentLeftAlpha;
            res.rightAlpha = ctx.requestedAlpha;

            // Ricalcola la maniglia coniugata della clip partner (left / out)
            res.rightFadeInControl = ctx.rightFadeInControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.rightFadeInControl.x),
                juce::jlimit (0.0, 1.0, ctx.rightFadeInControl.y),
                ctx.rightFadeInControl.weight
            };
        }
    }
    else if (ctx.interaction == CrossfadeInteractionKind::curvatureChange)
    {
        res.leftAlpha = ctx.currentLeftAlpha;
        res.rightAlpha = ctx.currentRightAlpha;
        if (ctx.isLeftClipModified)
        {
            res.leftFadeOutControl = ctx.requestedControl;
            res.rightFadeInControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
        else
        {
            res.rightFadeInControl = ctx.requestedControl;
            res.leftFadeOutControl = {
                juce::jlimit (0.01, 0.99, 1.0 - ctx.requestedControl.x),
                juce::jlimit (0.0, 1.0, ctx.requestedControl.y),
                ctx.requestedControl.weight
            };
        }
    }
    else
    {
        res.leftAlpha = ctx.currentLeftAlpha;
        res.rightAlpha = ctx.currentRightAlpha;
        res.leftFadeOutControl = ctx.leftFadeOutControl;
        res.rightFadeInControl = ctx.rightFadeInControl;
    }
    return res;
}


/** Famiglia visuale di una clip. Il tipo del documento resta invece definito
    dalla Source della take attiva in `ayra_timeline`. */
enum class ClipKind
{
    audio,
    midiNote,
    lineClip,
    pointKeyframe,
    spanKeyframe,
    continuousLine,
    imageSequence
};

enum class TimelinePointerTarget
{
    clip,
    continuousLane,
    keyframeEvent,
    keyframeLane
};

/** Payload sincrono per menu contestuali e comandi esterni.

    Contiene gia' l'area semantica e gli indici risolti dall'hit-test canonico:
    un consumer non deve ripetere geometria o cercare nuovamente l'oggetto.
    Le stringhe sono possedute, quindi il callback puo' copiare e trattenere il
    payload senza dipendere dal lifetime del canvas.
*/
struct TimelinePointerContext
{
    TimelinePointerTarget target = TimelinePointerTarget::clip;
    ClipKind kind = ClipKind::audio;
    TimelineClipZone zone = TimelineClipZone::none;
    KeyframeLaneType keyframeLaneType = KeyframeLaneType::points;
    bool isInClipOverlap = false;
    bool isInCrossfade = false;
    std::string overlappedPartnerClipId;
    double overlapStartSeconds = 0.0;
    double overlapEndSeconds = 0.0;
    MidiOverlapMode midiOverlapMode = MidiOverlapMode::replace;

    std::string laneId;
    std::string occurrencePath;
    std::string relationId;
    std::uint64_t documentRevision = 0;
    bool isMuted = false;
    bool isEffectiveMuted = false;
    bool isLocked = false;
    bool isEffectiveLocked = false;
    std::string transitionTypeId;
    std::string transitionName;
    std::string trackId;
    std::string itemId;
    juce::Point<float> localPosition;
    double timelineSeconds = 0.0;
    int markerIndex = -1;
    int pointIndex = -1;
    int segmentIndex = -1;
    int noteIndex = -1;
    int eventIndex = -1;
    int eventId = -1;
    juce::ModifierKeys modifiers;
};

[[nodiscard]] inline TimelinePointerContext makeTimelinePointerContext (
    std::string trackId,
    std::string itemId,
    const TimelinePointerTarget target,
    const ClipKind kind,
    const TimelineClipZone zone,
    const juce::Point<float> localPosition,
    const double timelineSeconds,
    const juce::ModifierKeys modifiers)
{
    TimelinePointerContext context;
    context.target = target;
    context.kind = kind;
    context.zone = zone;
    context.trackId = std::move (trackId);
    context.itemId = std::move (itemId);
    context.localPosition = localPosition;
    context.timelineSeconds = timelineSeconds;
    context.modifiers = modifiers;
    return context;
}

/** Radice UI comune delle clip.

    La classe possiede solo identita' di vista e geometria locale. Non possiede
    stato documentale, media, Undo o routing. Il canvas genitore mantiene per
    ora la cattura del mouse: in questo modo l'estrazione non cambia neppure una
    priorita' delle gesture gia' approvate. */

struct TimelineAuditionRequest
{
    std::string trackId;
    std::string laneId;
    std::string clipId;
    std::string occurrencePath;
    std::string relationId;
    ClipKind clipKind = ClipKind::audio;
    double timelineSeconds = 0.0;
    float clipNormX = 0.0f;
    std::uint64_t documentRevision = 0;
};

class ClipComponent : public juce::Component
{
public:
    explicit ClipComponent (std::string clipId);
    ~ClipComponent() override = default;

    [[nodiscard]] const std::string& getClipId() const noexcept { return id; }
    [[nodiscard]] virtual ClipKind getClipKind() const noexcept = 0;

    /** Posiziona il child preservando le coordinate float del corpo e include
        lo spazio dinamico richiesto dalle decorazioni attive. */
    void setBodyBoundsInParent (juce::Rectangle<float> bodyBounds);

    /** Configura gli overflow richiesti dalle decorazioni attive per consentire
        la Space Reclamation (es. recupero dell'altezza se warp/cut sono disabilitati). */
    void setDecorationOverflows (float top, float bottom, float horizontal = 8.0f);

    [[nodiscard]] juce::Rectangle<float> getBodyBounds() const noexcept
    {
        return localBodyBounds;
    }

    [[nodiscard]] juce::Point<float> getParentOrigin() const noexcept
    {
        return getPosition().toFloat();
    }

    [[nodiscard]] float getTopOverflow() const noexcept { return currentTopOverflow; }
    [[nodiscard]] float getBottomOverflow() const noexcept { return currentBottomOverflow; }
    [[nodiscard]] float getHorizontalOverflow() const noexcept { return currentHorizontalOverflow; }

private:
    float currentHorizontalOverflow = 8.0f;
    float currentTopOverflow = 8.0f;
    float currentBottomOverflow = 6.0f;

    std::string id;
    juce::Rectangle<float> localBodyBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipComponent)
};

} // namespace ayra::lab::clips
