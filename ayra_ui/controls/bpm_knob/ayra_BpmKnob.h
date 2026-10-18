/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / BpmKnob
   ------------------------------------------------------------------
   Knob compatto per il tempo (BPM). Usa la stessa geometria canonica del Panpot:
   disco rotary con pointer + readout nella fascia inferiore.

   ## Interazione
   - drag verticale: su = piu' veloce (Shift = fine).
   - rotellina: +/- 1 BPM.
   - doppio click sul READOUT inferiore: edit da tastiera (digita il valore).
   - doppio click sul disco: reset al default (120).

   ## Decoupled / riusabile
   Non conosce il transport: notifica il nuovo valore via `Listener` +
   `onBpmChanged`. L'app collega `onBpmChanged` a `TransportController::setBpm`.
   Customizzabile via `ColourIds`.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class BpmKnob : public juce::Component,
               public juce::SettableTooltipClient
{
public:
    //==========================================================================
    enum ColourIds
    {
        backgroundColourId    = 0x3b00050, ///< Sfondo del knob.
        arcBackgroundColourId = 0x3b00051, ///< Track dell'arco.
        arcColourId           = 0x3b00052, ///< Arco di valore + indicatore.
        valueTextColourId     = 0x3b00053  ///< Readout valore + unita'.
    };

    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bpmKnobValueChanged (BpmKnob*, double newBpm) = 0;
    };

    //==========================================================================
    BpmKnob();
    ~BpmKnob() override;

    //==========================================================================
    void setRange (double newMin, double newMax) noexcept;
    void setDefaultBpm (double newDefault) noexcept { defaultBpm = newDefault; }

    /// Etichetta sotto il knob (default "BPM"). Il widget e' riusato anche come knob
    /// di volume/dB: in quel caso l'app la cambia (es. "dB") o la svuota. ASCII-only.
    void setCaption (const juce::String& newCaption) { caption = newCaption; repaint(); }

    void setBpm (double newBpm, juce::NotificationType notify = juce::sendNotificationSync);
    [[nodiscard]] double getBpm() const noexcept { return bpm; }

    //==========================================================================
    void addListener (Listener*);
    void removeListener (Listener*);
    std::function<void (double)> onBpmChanged;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    //==========================================================================
    void applyBpm (double newBpm, juce::NotificationType notify);
    [[nodiscard]] double proportion() const noexcept;   ///< 0..1 sul range.

    /// Rettangolo del disco del knob, con la fascia readout gia' esclusa.
    [[nodiscard]] juce::Rectangle<float> knobDisc() const noexcept;
    /// Fascia inferiore del valore, condivisa da paint/hit-test/editor.
    [[nodiscard]] juce::Rectangle<float> readoutBounds() const noexcept;

    /// Mostra l'editor da tastiera sopra il readout.
    void beginValueEdit();

    //==========================================================================
    double bpm        = 120.0;
    double minBpm     = 20.0;
    double maxBpm     = 300.0;
    double defaultBpm = 120.0;

    juce::String caption { "BPM" };   ///< etichetta sotto il knob (setCaption per riuso come dB)

    double dragStartBpm = 0.0;
    int    dragStartY   = 0;

    // Editor da tastiera del valore (visibile solo durante l'edit; il numero a
    // riposo lo disegna paint()). Figlio nascosto: si mostra su beginValueEdit().
    juce::Label valueLabel;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BpmKnob)
};

} // namespace ayra
