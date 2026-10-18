#pragma once

namespace ayra::lab::clips
{

class AudioClipComponent;

/** Adapter letto dal widget durante `paint()`.

    Il provider materializza una view non autorevole a partire dall'owner reale.
    In Multiverse l'implementazione potra' leggere snapshot/cache del modulo;
    nel Lab legge la fixture e lo stato transiente del canvas. */
class AudioClipPresentationProvider
{
public:
    virtual ~AudioClipPresentationProvider() = default;

    [[nodiscard]] virtual AudioClipVisual makeAudioClipVisual (
        const std::string& clipId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Prima specializzazione concreta della radice `ClipComponent`.

    Non decodifica file e non possiede il documento: visualizza waveform e
    stato di editing gia' preparati dal provider. */
class AudioClipComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawAudioClip (juce::Graphics&,
                                    const AudioClipVisual&,
                                    AudioClipComponent&);
    };

    AudioClipComponent (std::string clipId, AudioClipPresentationProvider&);
    ~AudioClipComponent() override = default;

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return ClipKind::audio;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    AudioClipPresentationProvider& presentationProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipComponent)
};

} // namespace ayra::lab::clips

