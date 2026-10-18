#pragma once

namespace ayra::lab::clips
{

class KeyframeLaneComponent;

/** Adapter letto dal widget durante `paint()`.
    Materializza la vista di una lane di Keyframe puntuali (Marker / Counter)
    o estesi con continuita' da punto A a punto B (Program Change / Arranger). */
class KeyframeLanePresentationProvider
{
public:
    virtual ~KeyframeLanePresentationProvider() = default;

    [[nodiscard]] virtual KeyframeLaneVisual makeKeyframeLaneVisual (
        const std::string& laneId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Componente visuale per lane di eventi (`pointKeyframe` o `spanKeyframe`).
    Quando configurato in modalita' `spanKeyframe` (es. Program Change), ogni
    evento riempie lo spazio fino all'evento successivo e l'ultimo evento si
    estende fino alla fine della timeline per garantire visivamente il MIDI Chase. */
class KeyframeLaneComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawKeyframeLane (juce::Graphics&,
                                       const KeyframeLaneVisual&,
                                       KeyframeLaneComponent&);
    };

    KeyframeLaneComponent (std::string laneId,
                           KeyframeLanePresentationProvider&,
                           bool spanToNextEvent = true,
                           KeyframeLaneType laneType = KeyframeLaneType::contiguousSpans);
    ~KeyframeLaneComponent() override = default;

    void setSpanMode (bool enabled)
    {
        setLaneType (enabled ? KeyframeLaneType::contiguousSpans : KeyframeLaneType::points);
    }
    [[nodiscard]] bool isSpanMode() const noexcept
    {
        return currentLaneType != KeyframeLaneType::points;
    }

    void setLaneType (KeyframeLaneType type)
    {
        if (currentLaneType == type) { return; }
        currentLaneType = type;
        isSpanKeyframeMode = (type != KeyframeLaneType::points);
        repaint();
    }
    [[nodiscard]] KeyframeLaneType getLaneType() const noexcept { return currentLaneType; }

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return isSpanMode() ? ClipKind::spanKeyframe : ClipKind::pointKeyframe;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    KeyframeLanePresentationProvider& presentationProvider;
    bool isSpanKeyframeMode = true;
    KeyframeLaneType currentLaneType = KeyframeLaneType::contiguousSpans;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyframeLaneComponent)
};

} // namespace ayra::lab::clips
