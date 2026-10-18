#pragma once

namespace ayra::lab::clips
{

class ImageClipComponent;

/** Adapter letto dal widget durante `paint()`.
    Il provider materializza una vista non autorevole (`ImageClipVisual`) a
    partire dall'owner reale, senza che il widget possieda media o playback. */
class ImageClipPresentationProvider
{
public:
    virtual ~ImageClipPresentationProvider() = default;

    [[nodiscard]] virtual ImageClipVisual makeImageClipVisual (
        const std::string& clipId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Specializzazione concreta di `ClipComponent` per clip di sequenze immagini.
    Non carica file su disco e non gestisce la riproduzione: disegna la filmstrip
    a tutta altezza, la vignettatura in tinta, i Warp Marker e la curva di Opacita'. */
class ImageClipComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawImageClip (juce::Graphics&,
                                    const ImageClipVisual&,
                                    ImageClipComponent&);
    };

    ImageClipComponent (std::string clipId, ImageClipPresentationProvider&);
    ~ImageClipComponent() override = default;

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return ClipKind::imageSequence;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    ImageClipPresentationProvider& presentationProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImageClipComponent)
};

} // namespace ayra::lab::clips