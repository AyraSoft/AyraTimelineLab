/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / TimeSignatureControl
   ------------------------------------------------------------------
   Widget compatto per la divisione metrica (time signature) "N / D".

   ## Perche' non due ComboBox (come il vecchio NodeGraph)
   Due `juce::ComboBox` occupano spazio, rompono il ritmo visivo della footer e
   costringono a un click+menu per ogni cambio. Qui il numeratore e il
   denominatore sono due zone DRAGGABILI di un unico glifo "N / D": drag verticale
   o rotellina sulla meta' sinistra cambia il numeratore (1..32), sulla meta'
   destra il denominatore (cicla 1/2/4/8/16/32). Niente menu, footprint minimo,
   lettura immediata.

   ## Decoupled / riusabile
   Non conosce il transport: notifica via `Listener` + `onTimeSignatureChanged`.
   L'app collega a `TransportController::setTimeSignature`. Customizzabile via
   `ColourIds`.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class TimeSignatureControl : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    //==========================================================================
    enum ColourIds
    {
        backgroundColourId = 0x3b00060, ///< Sfondo.
        textColourId       = 0x3b00061, ///< Cifre.
        slashColourId      = 0x3b00062, ///< Barra "/".
        highlightColourId  = 0x3b00063  ///< Evidenziazione zona hover/drag.
    };

    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void timeSignatureChanged (TimeSignatureControl*, int numerator, int denominator) = 0;
    };

    //==========================================================================
    TimeSignatureControl();
    ~TimeSignatureControl() override;

    //==========================================================================
    void setTimeSignature (int numerator, int denominator,
                           juce::NotificationType notify = juce::sendNotificationSync);

    [[nodiscard]] int getNumerator()   const noexcept { return numerator; }
    [[nodiscard]] int getDenominator() const noexcept { return denominator; }

    //==========================================================================
    void addListener (Listener*);
    void removeListener (Listener*);
    std::function<void (int, int)> onTimeSignatureChanged;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    //==========================================================================
    enum class Zone { none, numerator, denominator };

    [[nodiscard]] Zone zoneAt (juce::Point<int> p) const noexcept;
    void bumpNumerator (int delta, juce::NotificationType notify);
    void bumpDenominator (int direction, juce::NotificationType notify);   // cicla potenze di 2
    void apply (int n, int d, juce::NotificationType notify);

    [[nodiscard]] static int denominatorIndex (int denom) noexcept;        // -> indice in {1,2,4,8,16,32}

    //==========================================================================
    int numerator   = 4;
    int denominator = 4;

    Zone hoverZone = Zone::none;
    Zone dragZone  = Zone::none;
    int  dragStartValue = 0;
    int  dragStartY     = 0;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeSignatureControl)
};

} // namespace ayra
