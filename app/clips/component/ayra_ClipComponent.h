#pragma once

namespace ayra::lab::clips
{

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
