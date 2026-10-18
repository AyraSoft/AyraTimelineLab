/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / ColourButton
   ------------------------------------------------------------------
   Bottone-pastiglia di SCELTA COLORE riusabile + picker standardizzato.
   E' il controllo canonico per "scegli un colore" in tutta l'app: colore di un
   nodo del grafo (NodesInspector), override colore per tipo di stream
   (Settings > Appearance > NODE COLOURS), e ovunque servira' in futuro.

   ## Perche' esiste (e cosa ha sostituito)
   Prima c'erano DUE implementazioni divergenti: il "pallino colore" disegnato a
   mano nel NodesInspector (con tanto di `juce::DocumentWindow` + `ColourSelector`
   dedicati) e la `ColourChip` del pannello Settings (con un `CallOutBox` proprio,
   file-locale). Stessa funzione, due look, due picker. Questo widget unifica:
     - UN solo aspetto (pastiglia rounded + dual-rim + dot di override);
     - UN solo picker (`launchPicker`: riga di swatch quick-pick + `ColourSelector`
       in un `CallOutBox`), cosi' la scelta colore e' identica ovunque.

   ## Interazione
     - left-click  -> apre il picker ancorato al bottone; notifica LIVE (anche
                      durante il drag nel selettore) via `onColourPicked`.
     - right-click -> `onReset` (es. "torna al colore di fabbrica").
   In modalita' SWATCH (`setPickerEnabled(false)`) il left-click non apre nulla e
   chiama `onClick`: e' la modalita' con cui il picker stesso disegna i propri
   quick-pick (niente picker dentro il picker).

   ## Uso
   @code
   ayra::ColourButton btn;
   btn.setColour (current);
   btn.setSwatches (ayra::AppearanceSettings::getDefaultSwatches());
   btn.onColourPicked = [this] (juce::Colour c) { model.setColour (c); };
   btn.onReset        = [this]                  { model.clearColour();  };
   @endcode

   Doctrine: ZERO #include (tipi via ayra_common.h), namespace ayra, juce::
   qualificato, ASCII-only, const/noexcept/[[nodiscard]].
  ==============================================================================
*/

#pragma once

namespace ayra
{

class ColourButton final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    //==========================================================================
    /// Colori (range widget condivisi `0x3b000xx`). Default risolti a paint
    /// dai token correnti; gli override Component/LookAndFeel hanno precedenza.
    enum ColourIds
    {
        rimDarkColourId  = 0x3b00040,
        rimLightColourId = 0x3b00041,
        markerColourId   = 0x3b00042
    };

    //==========================================================================
    ColourButton();
    ~ColourButton() override;

    //==========================================================================
    using juce::Component::setColour;

    /** Colore mostrato dalla pastiglia (NON un ColourId: e' il valore scelto). */
    void setColour (juce::Colour newColour);
    [[nodiscard]] juce::Colour getColour() const noexcept { return colour; }

    /** Mostra il puntino accent di "override attivo" in alto a dx. */
    void setOverridden (bool isOverridden);
    [[nodiscard]] bool isOverridden() const noexcept { return overridden; }

    /** Palette offerta come quick-pick nel picker (vuota = nessuna riga swatch). */
    void setSwatches (juce::Array<juce::Colour> palette);

    /** false = modalita' swatch: il click chiama `onClick` anziche' aprire il
        picker. Revoca anche un picker gia' aperto. Message Thread only. */
    void setPickerEnabled (bool shouldOpenPickerOnClick) noexcept;

    //==========================================================================
    std::function<void()> onPickerOpened;
    std::function<void (juce::Colour)> onColourPicked;
    std::function<void()> onReset;
    std::function<void()> onClick;

    /** Apre il picker rispettando enabled e pickerEnabled. Un picker ancora
        aperto viene portato davanti, senza creare una seconda finestra. */
    void openPicker();

    //==========================================================================
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void dismissPicker() noexcept;

    juce::Colour              colour { juce::Colours::grey };
    bool                      overridden = false;
    bool                      pickerEnabled = true;
    juce::Array<juce::Colour> swatches;
    /** Handle non-owning al CallOutBox gestito da JUCE. La distruzione del
        bottone revoca callback e LookAndFeel borrowed prima del suo teardown. */
    juce::Component::SafePointer<juce::CallOutBox> activePicker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourButton)
};

} // namespace ayra
