#pragma once

namespace ayra::lab::clips
{

class MidiClipComponent;

/** Adapter letto dal widget durante `paint()`.
    Materializza una vista MIDI non autorevole (note warppate, curva CC attiva,
    tracce CC fantasma, Program Change e stato LOD) valida per il singolo frame. */
class MidiClipPresentationProvider
{
public:
    virtual ~MidiClipPresentationProvider() = default;

    [[nodiscard]] virtual MidiClipVisual makeMidiClipVisual (
        const std::string& clipId,
        juce::Rectangle<float> localBodyBounds,
        juce::Point<float> parentOrigin) = 0;
};

/** Specializzazione concreta di `ClipComponent` per clip MIDI / Instrument.
    Non possiede sequenze MIDI o stato documentale: delega il rendering al LookAndFeel
    usando la proiezione `MidiClipVisual` costruita dal provider. */
class MidiClipComponent final : public ClipComponent
{
public:
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawMidiClip (juce::Graphics&,
                                   const MidiClipVisual&,
                                   MidiClipComponent&);
    };

    MidiClipComponent (std::string clipId, MidiClipPresentationProvider&);
    ~MidiClipComponent() override = default;

    [[nodiscard]] ClipKind getClipKind() const noexcept override
    {
        return ClipKind::midiNote;
    }

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] LookAndFeelMethods& getClipLookAndFeel();

    MidiClipPresentationProvider& presentationProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipComponent)
};

} // namespace ayra::lab::clips
