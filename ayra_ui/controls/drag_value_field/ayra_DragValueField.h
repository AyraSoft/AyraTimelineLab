/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / DragValueField
   ------------------------------------------------------------------
   "Slider implicito" compatto: un valore numerico editabile per DRAG del mouse,
   senza track ne' thumb. Il pattern del footer (BpmKnob / TimeSignatureControl)
   generalizzato in UN widget riusabile.

   ## Interazione
   - drag verticale: su = aumenta, giu' = diminuisce (sensibilita' configurabile).
   - Shift + drag: SNAP a intero (niente cifre decimali) -- richiesta esplicita
     "se teniamo premuto shift ... non prendiamo i valori dopo la virgola".
   - rotellina: +/- un passo (`interval`, default 1).
   - doppio click: apre un editor da tastiera (`juce::Label`) per digitare il valore.
     Invio / focus-lost = commit; Esc = annulla.

   ## Due modalita' di valore
   - RANGE continuo: `setRange(min, max, interval)`. `interval == 0` = continuo
     (con `decimalPlaces` cifre); `interval > 0` = quantizzato a multipli.
   - VALORI AMMESSI: `setAllowedValues({...})`. Il drag/rotella scorre la lista
     (indice), il display mostra il valore, il type parsa al piu' vicino ammesso.
     Serve p.es. per il denominatore metrico (1/2/4/8/16/32).

   ## Commit vs preview (una mutazione per gesto)
   - `onValueChange(v)` = LIVE, a ogni cambiamento (drag/type/wheel) -> feedback
     immediato (il widget si ridisegna gia' da solo).
   - `onValueCommit(v)` = FINE del gesto (mouseUp dopo un drag, Invio nel type,
     tick di rotella). Il consumer scrive il documento QUI -> un solo step di undo,
     niente flood di mutazioni per ogni pixel (pattern MixerPanel::Entry).
   - `onEditBegin` / `onEditEnd` = il gesto (drag o type) e' iniziato / finito:
     un host puo' tenersi "occupato" (es. l'inspector rimane espanso).
   `isBusy()` = drag in corso o editor aperto.

   ## Decoupled / riusabile
   Non conosce il dominio: notifica via `Listener` + `std::function`. Tipi forti,
   `ColourIds` personalizzabili, tokens d'app di default. Message thread only.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class DragValueField : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    //==========================================================================
    enum ColourIds
    {
        backgroundColourId = 0x3b000c0, ///< Sfondo della cella.
        textColourId       = 0x3b000c1, ///< Valore.
        captionColourId    = 0x3b000c2, ///< Caption (piccola, sopra il valore).
        outlineColourId    = 0x3b000c3, ///< Bordo.
        highlightColourId  = 0x3b000c4  ///< Evidenziazione hover / drag.
    };

    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void dragValueFieldChanged (DragValueField*, double newValue) = 0;
    };

    //==========================================================================
    DragValueField();
    ~DragValueField() override;

    //==========================================================================
    // Configurazione.

    /** Range continuo/quantizzato. `interval == 0` = continuo (usa `decimalPlaces`);
        `interval > 0` = snap a multipli di `interval`. Azzera eventuali allowedValues. */
    void setRange (double newMin, double newMax, double newInterval = 0.0) noexcept;

    /** Valori ammessi (drag = scorre la lista). Non-vuoto -> ha priorita' sul range.
        Vuoto -> torna in modalita' range. */
    void setAllowedValues (juce::Array<double> values);

    /** Unita' di valore per pixel di drag verticale (modalita' range). Default 0.25. */
    void setSensitivity (double unitsPerPixel) noexcept;

    /** Pixel di drag per passo (modalita' allowedValues). Default 12. */
    void setStepPixels (int pixelsPerStep) noexcept;

    /** Cifre decimali del display (modalita' range, `interval == 0`). `< 0` = intero. */
    void setDecimalPlaces (int places) noexcept;

    /** Caption piccola sopra il valore ("" = nessuna). */
    void setCaption (const juce::String& text);

    /** Suffisso appeso al valore (es. " dB"). ASCII only. */
    void setSuffix (const juce::String& text);

    /** Override del display del valore (bypassa la formattazione numerica). */
    void setDisplayFormatter (std::function<juce::String (double)> fn);

    /** Override del parsing del testo digitato (ritorna false = input non valido). */
    void setTextParser (std::function<bool (const juce::String&, double&)> fn);

    //==========================================================================
    void setValue (double newValue, juce::NotificationType notify = juce::sendNotificationSync);
    [[nodiscard]] double getValue() const noexcept { return value; }

    /** Vero mentre un gesto e' in corso (drag o editor aperto): l'host lo usa per
        posticipare set esterni che distruggerebbero il widget sotto le dita. */
    [[nodiscard]] bool isBusy() const noexcept { return dragging || editor.isVisible(); }

    //==========================================================================
    void addListener (Listener*);
    void removeListener (Listener*);

    std::function<void (double)> onValueChange;   ///< live (ogni cambio)
    std::function<void (double)> onValueCommit;   ///< fine gesto (commit al documento)
    std::function<void()>        onEditBegin;     ///< inizio drag/type
    std::function<void()>        onEditEnd;        ///< fine drag/type

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    //==========================================================================
    /** Applica + clamp + notifica LIVE (`onValueChange` + Listener). Non fa commit. */
    void apply (double newValue, juce::NotificationType notify);
    void fireCommit();                              ///< `onValueCommit` col valore corrente

    [[nodiscard]] double clampValue (double v) const noexcept;
    [[nodiscard]] juce::String textForValue (double v) const;
    [[nodiscard]] bool parseText (const juce::String& text, double& out) const;
    [[nodiscard]] int  nearestAllowedIndex (double v) const noexcept;

    void beginTypeEdit();
    void commitTypeEdit();

    //==========================================================================
    double value    = 0.0;
    double minValue = 0.0;
    double maxValue = 1.0;
    double interval = 0.0;   ///< 0 = continuo
    double unitsPerPixel = 0.25;
    int    stepPixels    = 12;
    int    decimalPlaces = -1;

    juce::Array<double> allowedValues;   ///< non-vuoto -> modalita' a lista
    juce::String        caption;
    juce::String        suffix;

    std::function<juce::String (double)>                    displayFormatter;
    std::function<bool (const juce::String&, double&)>      textParser;

    //--- Stato del gesto -------------------------------------------------------
    bool   dragging   = false;
    bool   dragMoved  = false;
    bool   hover      = false;
    double dragStartValue = 0.0;
    int    dragStartIndex = 0;      ///< modalita' allowedValues
    int    dragStartY     = 0;

    juce::Label editor;             ///< editor da tastiera (doppio click)

    juce::ListenerList<Listener> listeners;

    juce::Font captionFont { ayra::tokens::type::caption().withHeight (8.0f) };
    juce::Font valueFont   { ayra::tokens::type::value().withHeight (12.0f) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragValueField)
};

} // namespace ayra
