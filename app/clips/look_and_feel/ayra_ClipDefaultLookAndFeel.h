#pragma once

namespace ayra::lab::clips
{

/** LookAndFeel clip pronto all'uso.
    L'implementazione conserva intenzionalmente la ricetta visuale approvata
    dal Lab. E' stateless e puo' essere usata come fallback condiviso. */
class ClipDefaultLookAndFeel final : public ClipLookAndFeelMethods
{
public:
    [[nodiscard]] static ClipDefaultLookAndFeel& getDefaultInstance();

    void drawAudioClip (juce::Graphics&,
                        const AudioClipVisual&,
                        AudioClipComponent&) override;

    void drawMidiClip (juce::Graphics&,
                       const MidiClipVisual&,
                       MidiClipComponent&) override;

    void drawLineClip (juce::Graphics&,
                       const LineClipVisual&,
                       LineClipComponent&) override;

    void drawKeyframeLane (juce::Graphics&,
                           const KeyframeLaneVisual&,
                           KeyframeLaneComponent&) override;

    void drawContinuousLine (juce::Graphics&,
                             const ContinuousLineVisual&,
                             ContinuousLineComponent&) override;

    void drawImageClip (juce::Graphics&,
                        const ImageClipVisual&,
                        ImageClipComponent&) override;

    void drawAutomationCurve (juce::Graphics&,
                              const AutomationCurveParams&,
                              juce::Component* visualOwner = nullptr);
};

} // namespace ayra::lab::clips
