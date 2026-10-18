/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / MacroSlotComponent
   ------------------------------------------------------------------
   Slot applicativo di un singolo macro/quick-control: il "mattone" che la
   MacrosPanel ripete in griglia. E' la versione adattata al nuovo engine
   (ayra::GraphProcessor / TensorAudioProcessor) e al design system Frosted Lab
   del legacy NodeGraph_Plugin/Components/MacroSlotComponent.

   ## Cosa fa (livello app, NON nel modulo ayra_parameters_manager)
   - Sceglie il widget giusto dal MacroControlType della macro:
       * continuous  -> juce::Slider in stile ROTARY (knob), attaccato al param
                        APVTS `quick_control_N` via SliderParameterAttachment.
                        Il look (arco accent + well recessed) arriva dalla LAF
                        d'app (MultiverseLookAndFeel::drawRotarySlider): qui si
                        usa `juce::Slider::Rotary` direttamente, niente disegno
                        custom.
       * toggle / momentary / multiChoice -> delega al mattone riusabile
                        ayra::MacroDisplayComponent (nessuno slider coinvolto:
                        riuso, niente duplicazione di stepMapping/radio/combo).
   - Mostra una name-label, tinta col colore identitario della macro.
   - Disegna una "colour cap" (barretta colorata) come marker d'identita': l'arco
     del knob e' sempre accent (Frosted Lab), quindi il colore della macro vive
     sulla cap + sulla label, non sull'arco.
   - Modalita' LINK: se il macro ha un ParameterLinkGroup con >=2 parametri,
     mostra il widget del parametro target (ConstrainedComponentFactory).
   - Placeholder: se il macro non ha assignment, un bottone che apre la Macro Map.
   - Right-click (o doppio click sulla label) -> apre la Macro Map per questo macro.

   ## Eventi
   `onOpenMacroMap` (std::function) e' l'unico evento: un'AZIONE (apri il mapping),
   non un value-change. La MacrosPanel la inoltra col proprio indice di macro.

   ## Customizzazione
   ColourIds nel namespace 0x3c000xx. Il widget funziona out-of-the-box: i colori
   di default arrivano dai token Frosted Lab (`tokens::`).

   ## Riuso / modulo?
   La logica di sync param continuo qui e' minimale (un SliderParameterAttachment
   sul param normalizzato 0..1): non duplica MacroDisplayComponent, lo affianca
   solo per i 3 tipi non-slider. Se in futuro servisse un rotary "display-range"
   completo, il posto giusto e' estendere ayra::MacroDisplayComponent nel modulo.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class MacroSlotComponent : public juce::Component,
                           public juce::SettableTooltipClient,
                           public ayra::MacroParameter::Listener
{
public:
    //==========================================================================
    enum ColourIds
    {
        backgroundColourId = 0x3c00010, ///< Sfondo dello slot (default: trasparente).
        nameTextColourId   = 0x3c00011, ///< Testo della label (sovrascritto dal colore macro se impostato).
        colourCapColourId  = 0x3c00012  ///< Cap d'identita' quando la macro non ha colore.
    };

    //==========================================================================
    /** @param macro        il MacroParameter da rappresentare (puo' essere nullptr).
        @param apvts         APVTS host che contiene il param `quick_control_N`.
        @param apvtsParamId  parameterId APVTS di questo macro (es. "quick_control_1").
        @param graph         grafo Tensor: serve a risolvere il NOME del nodo di
                             ogni parametro mappato per il tooltip.
        @param linkGroup     ParameterLinkGroup associato (puo' essere nullptr). */
    MacroSlotComponent (ayra::MacroParameter* macro,
                        juce::AudioProcessorValueTreeState& apvts,
                        juce::String apvtsParamId,
                        ayra::GraphProcessor& graph,
                        ayra::ParameterLinkGroup* linkGroup = nullptr);

    ~MacroSlotComponent() override;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    //==========================================================================
    /** Azione: apri la Macro Map per questo slot. Invocata dal bottone gear
        (ancora al gear), dal right-click e dal doppio click (ancora allo slot).
        `anchor` e' il component a cui ancorare il CallOutBox dell'editor. */
    std::function<void (juce::Component& anchor)> onOpenMacroMap;

    //==========================================================================
    /** Aggiorna il link group (chiamare dopo un cambio di link) e ricostruisce. */
    void setLinkGroup (ayra::ParameterLinkGroup* group);

    /** Ricostruisce il contenuto (rotary / display / link) in base al control type.
        Il knob e' SEMPRE presente per i macro continuous, anche senza assignment:
        e' legato al param APVTS `quick_control_N`, che esiste a prescindere. */
    void rebuild();

    [[nodiscard]] ayra::MacroParameter* getMacro() const noexcept { return macroParam; }

    //==========================================================================
    // ayra::MacroParameter::Listener
    void onMacroNameChanged        (ayra::MacroParameter*) override;
    void onMacroColourChanged      (ayra::MacroParameter*) override;
    void onMacroControlTypeChanged (ayra::MacroParameter*) override;

private:
    //==========================================================================
    [[nodiscard]] bool isInLinkMode() const noexcept;

    void showRotaryKnob();   ///< continuous -> juce::Slider rotary + attachment APVTS.
    void showMacroDisplay(); ///< toggle/momentary/multiChoice -> MacroDisplayComponent.
    void showLinkView();     ///< link mode -> widget del parametro target.
    void clearContent();

    void updateColours();

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    //==========================================================================
    /** Colore identitario effettivo: colore della macro se valido, altrimenti
        la cap di default dai token. */
    [[nodiscard]] juce::Colour identityColour() const;

    /** Colore d'accento del widget del controllo: colore custom della macro se
        impostato, altrimenti l'accent dell'app (lo slot e' visibile solo se la
        macro e' mappata -> il widget porta SEMPRE un colore). Usato per tingere
        rotary/checkbox/segmented/bottone col colore della macro. */
    [[nodiscard]] juce::Colour macroAccentColour() const;

    /** Costruisce il testo del tooltip: i controlli mappati (param + nodo) in
        modo advanced, oppure il target linkato in modo link. Vuoto se nulla. */
    [[nodiscard]] juce::String buildMappingTooltip() const;

    /** Nome del nodo che possiede il parametro dato (o nome del grafo per i param
        root, vuoto se non trovato). Per il tooltip. */
    [[nodiscard]] juce::String nodeNameForParam (juce::AudioProcessorParameter* p) const;

    /** Mappa `param* -> nome nodo` dell'INTERO documento, costruita una sola volta
        per `buildMappingTooltip` (lookup O(1) per assignment) invece di un
        `nodeNameForParam` O(nodi x parametri) per ogni assignment. Semantica
        identica: priorita' ai parametri di nodo, fallback ai param ROOT del grafo. */
    [[nodiscard]] std::unordered_map<juce::AudioProcessorParameter*, juce::String>
        buildParamNodeNameMap() const;

    /** Applica il tooltip dei mapping allo slot + a tutti i sub-widget interattivi
        (i ColourId/tooltip JUCE non si propagano ai figli): cosi' il tooltip
        compare ovunque si passi sul widget. Esclude il gear (tooltip proprio). */
    void applyMappingTooltip();

    //==========================================================================
    ayra::MacroParameter*               macroParam = nullptr;
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::String                        paramId;
    ayra::GraphProcessor&               graph;
    ayra::ParameterLinkGroup*           currentLinkGroup = nullptr;

    juce::Label nameLabel;

    // Gear in alto a destra: apre la Macro Map per questo slot (stessa
    // dimensione del gear della Snapshot card -> 18x18 in una colonna da 20).
    ayra::ShapeIconButton settingsButton;

    // Un solo contenuto vivo alla volta (gli altri sono nullptr).
    std::unique_ptr<juce::Slider>                    knob;
    std::unique_ptr<juce::SliderParameterAttachment> knobAttachment;
    std::unique_ptr<ayra::MacroDisplayComponent>     macroDisplay;
    std::unique_ptr<juce::Component>                 linkDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroSlotComponent)
};

} // namespace ayra
