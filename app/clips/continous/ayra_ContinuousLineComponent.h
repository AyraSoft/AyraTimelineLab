#pragma once

namespace ayra::lab::clips
{

class ContinuousLineComponent;

/** Adapter letto dal widget durante `paint()`.
    Materializza la vista di una curva continua lungo l'intera timeline
    (es. Traccia TEMPO o automazione globale di traccia). */
class ContinuousLinePresentationProvider
{
public:
    virtual ~ContinuousLinePresentationProvider() = default;

    [[nodiscard]] virtual ContinuousLineVisual makeContinuousLineVisual (
        const std::string& laneId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Specializzazione concreta di `ClipComponent` per una lane a linea continua
    ininterrotta (stile traccia TEMPO con scala graduata laterale e nodi). */
class ContinuousLineComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawContinuousLine (juce::Graphics&,
                                         const ContinuousLineVisual&,
                                         ContinuousLineComponent&);
    };

    ContinuousLineComponent (std::string laneId, ContinuousLinePresentationProvider&);
    ~ContinuousLineComponent() override = default;

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return ClipKind::continuousLine;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    ContinuousLinePresentationProvider& presentationProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContinuousLineComponent)
};

} // namespace ayra::lab::clips