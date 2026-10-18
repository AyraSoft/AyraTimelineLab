#pragma once

namespace ayra::lab::clips
{

/** Interfaccia aggregata del LookAndFeel clip.
    Non eredita da `juce::LookAndFeel_V4`: l'app puo' combinarla con gli altri
    moduli Ayra senza diamond inheritance. I metodi del singolo widget hanno un
    forwarding di default, quindi l'app sovrascrive soltanto cio' che serve. */
struct ClipLookAndFeelMethods : AudioClipComponent::LookAndFeelMethods,
                                MidiClipComponent::LookAndFeelMethods,
                                LineClipComponent::LookAndFeelMethods,
                                KeyframeLaneComponent::LookAndFeelMethods,
                                ContinuousLineComponent::LookAndFeelMethods,
                                ImageClipComponent::LookAndFeelMethods
{
    ~ClipLookAndFeelMethods() override = default;
};

/** Risolve il LookAndFeel clip installato sul componente o il default stateless. */
[[nodiscard]] ClipLookAndFeelMethods& getClipLookAndFeel (juce::Component&);

} // namespace ayra::lab::clips
