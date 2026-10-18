#pragma once

namespace ayra::lab::clips
{

class LineClipComponent;

/** Adapter letto dal widget durante `paint()`.
    Materializza una vista non autorevole per una clip di automazione/CC
    limitata nel tempo (fuori dalla clip non esiste linea ne' interpolazione). */
class LineClipPresentationProvider
{
public:
    virtual ~LineClipPresentationProvider() = default;

    [[nodiscard]] virtual LineClipVisual makeLineClipVisual (
        const std::string& clipId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Specializzazione concreta di `ClipComponent` per clip a linea (CC, ModWheel,
    Pitch Bend, Sustain, Parametri). Non possiede stato documentale: delega il
    rendering al LookAndFeel usando la proiezione `LineClipVisual`. */
class LineClipComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawLineClip (juce::Graphics&,
                                   const LineClipVisual&,
                                   LineClipComponent&);
    };

    LineClipComponent (std::string clipId, LineClipPresentationProvider&);
    ~LineClipComponent() override = default;

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return ClipKind::lineClip;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    LineClipPresentationProvider& presentationProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LineClipComponent)
};

} // namespace ayra::lab::clips