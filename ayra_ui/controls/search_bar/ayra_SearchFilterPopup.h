/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / SearchFilterPopup
   ------------------------------------------------------------------------------
   Popup CUSTOM (Frosted Lab) dei filtri avanzati, ancorato al bottone "more" di
   una `ayra::SearchBar`. Sostituisce il `juce::PopupMenu` nativo (checkmark JUCE):
   N sezioni di toggle a pillola (`SettingToggle`) + 1 sezione segmented
   (`SettingSegmented`) + un bottone di reset.

   ## Perche' esiste
   Il browser dei parametri di `MacroMapComponent` e la lista parametri di
   `SnapshotParamsComponent` (ParamsList) avevano lo STESSO popup di filtri
   (Show types / Show special / <segmented>): l'unica differenza era l'etichetta e
   le opzioni della sezione segmented ("Assignment status" vs "Tick status") e lo
   stato su cui scrivevano. Questo widget generalizza quel popup: NON sa nulla del
   dominio (parametri, macro, ...), riceve le sezioni come dati (`ToggleSection` +
   `SegmentedSection`) e delega ogni mutazione al consumer via `bool*` / callable.

   ## Uso
   @code
   ayra::SearchBar bar;
   bar.setShowMoreButton (true);
   bar.onMoreOptions = [this] (juce::Component& anchor)
   {
       ayra::SearchFilterPopup::ToggleSection types { "Show types", {
           { "Float", &filterState.showFloat }, { "Int", &filterState.showInt } } };
       ayra::SearchFilterPopup::SegmentedSection seg {
           "Tick status", { "All", "Ticked", "Unticked" },
           [this] { return (int) filterState.tick; },
           [this] (int i) { filterState.tick = (TickStatus) i; } };

       auto popup = std::make_unique<ayra::SearchFilterPopup> (
           juce::Array<ayra::SearchFilterPopup::ToggleSection> { types },
           seg,
           [this] { rebuild(); },                 // onChange
           [this] { filterState = FilterState{}; }); // onReset (reset dello stato consumer)
       ayra::SearchFilterPopup::launch (anchor, std::move (popup));
   };
   @endcode

   Doctrine: ZERO #include (tipi via ayra_common.h), namespace ayra, ASCII-only.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class SearchFilterPopup : public juce::Component
{
public:
    //==========================================================================
    /// Un singolo toggle: etichetta + puntatore al bool di stato del consumer.
    /// Il popup scrive su `*flag` e poi chiama `onChange`.
    struct Toggle
    {
        juce::String label;
        bool*        flag = nullptr;
    };

    /// Una sezione di toggle: header + N toggle (es. "Show types" -> 5 tipi).
    /// `toggles` e' un std::vector (NON juce::Array) di proposito: il ctor
    /// initializer_list di juce::Array e' templato sul tipo elemento, quindi
    /// NON deduce il tipo da graffe annidate senza tipo esplicito
    /// (`{ {"Float", &flag}, ... }`). Il ctor di std::vector non e' templato e
    /// accetta l'aggregate-init annidata direttamente. Percorso message-thread
    /// (build del popup on-click): std::vector e' la scelta corretta qui.
    struct ToggleSection
    {
        juce::String      header;
        std::vector<Toggle> toggles;
    };

    /// Sezione segmented (una sola, in coda): header + opzioni + get/set dell'indice.
    /// get/set (non un `int*`) per non aliasare enum/int del consumer in modo unsafe.
    struct SegmentedSection
    {
        juce::String             header;
        juce::StringArray        options;
        std::function<int()>     getIndex;
        std::function<void(int)> setIndex;
    };

    //==========================================================================
    static constexpr int kWidth = 250;

    /** @param sections  Sezioni di toggle (nell'ordine di visualizzazione).
        @param segmented Sezione segmented finale (header vuoto = nascosta).
        @param onChange  Chiamato dopo OGNI mutazione (il consumer rifa' il filtro).
        @param onReset   Chiamato dal bottone reset: il consumer riporta il proprio
                         stato ai default. Il popup poi risincronizza i controlli e
                         chiama onChange. */
    SearchFilterPopup (juce::Array<ToggleSection> sections,
                       SegmentedSection segmented,
                       std::function<void()> onChange,
                       std::function<void()> onReset);
    ~SearchFilterPopup() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Altezza consigliata per il contenuto corrente (usata da `launch`).
    [[nodiscard]] int preferredHeight() const noexcept;

    /// Riallinea i controlli allo stato del consumer (dopo un reset).
    void syncFromState();

    /** Apre `popup` in un `juce::CallOutBox` ancorato ad `anchor`, dimensionato al
        contenuto ed ereditando la LAF d'app. Uso tipico dal `onMoreOptions` di una
        SearchBar. Prende ownership del popup. */
    static void launch (juce::Component& anchor, std::unique_ptr<SearchFilterPopup> popup);

private:
    juce::Array<ToggleSection> sections;
    SegmentedSection           segmented;
    std::function<void()>      onChange;
    std::function<void()>      onReset;

    juce::OwnedArray<juce::Label>         sectionHeaderLabels;   // uno per ToggleSection
    juce::OwnedArray<juce::Label>         toggleLabels;          // flat, in ordine di sezione
    juce::OwnedArray<ayra::SettingToggle> toggleControls;        // flat, parallelo a toggleFlags
    juce::Array<bool*>                    toggleFlags;           // parallelo a toggleControls
    juce::Label                           segmentedHeaderLabel;
    ayra::SettingSegmented                segmentedControl;
    juce::TextButton                      resetBtn { "Reset filters" };

    [[nodiscard]] bool hasSegmented() const noexcept { return segmented.header.isNotEmpty(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchFilterPopup)
};

} // namespace ayra
