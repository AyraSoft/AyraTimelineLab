/*
  ==============================================================================
   ayra_common / widgets / controls / MacroSlotComponent (impl)
   Zero-include (dottrina moduli Ayra): tutti i tipi arrivano da ayra_common.h.
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Helper file-local: il test "la macro ha un colore custom (alpha > 0)" e il
// derivare un colore-o-fallback erano scritti a mano in identityColour(),
// macroAccentColour() e updateColours(). Un solo punto di verita' qui.
namespace
{
   [[nodiscard]] bool hasCustomMacroColour (const ayra::MacroParameter* macro) noexcept
   {
      return macro != nullptr && macro->getColour().getAlpha() > 0;
   }

   [[nodiscard]] juce::Colour macroColourOr (const ayra::MacroParameter* macro,
                                             juce::Colour fallback) noexcept
   {
      if (macro != nullptr)
      {
         const auto c = macro->getColour();
         if (c.getAlpha() > 0)
            return c;
      }
      return fallback;
   }

   /// Se `linkDisplay` e' un ConstrainedComponent, gli applica il colore d'accento.
   /// (cast + setAccentColour ripetuto in showLinkView e updateColours).
   void tintConstrainedLinkDisplay (juce::Component* linkDisplay, juce::Colour accent)
   {
      if (auto* cc = dynamic_cast<ayra::ConstrainedComponent*> (linkDisplay))
         cc->setAccentColour (accent);
   }
}

MacroSlotComponent::MacroSlotComponent (ayra::MacroParameter* macro,
                                        juce::AudioProcessorValueTreeState& apvts,
                                        juce::String apvtsParamId,
                                        ayra::GraphProcessor& graph_,
                                        ayra::ParameterLinkGroup* linkGroup)
    : macroParam (macro),
      valueTreeState (apvts),
      paramId (std::move (apvtsParamId)),
      graph (graph_),
      currentLinkGroup (linkGroup)
{
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions { 12.0f });
    nameLabel.setEditable (false, false);
    nameLabel.setInterceptsMouseClicks (false, false); // i click li gestisce lo slot
    addAndMakeVisible (nameLabel);

    // Gear: apre la Macro Map ancorata al gear stesso (come la card Snapshot).
    settingsButton.setGlyph (ayra::glyphs::settings);
    settingsButton.setTooltip ("Macro Map");
    settingsButton.onClick = [this]
    {
        if (onOpenMacroMap)
            onOpenMacroMap (settingsButton);
    };
    addAndMakeVisible (settingsButton);

    if (macroParam != nullptr)
    {
        macroParam->addListener (this);
        nameLabel.setText (macroParam->getName(), juce::dontSendNotification);
    }

    rebuild();
}

MacroSlotComponent::~MacroSlotComponent()
{
    if (macroParam != nullptr)
        macroParam->removeListener (this);

    // L'attachment deve morire PRIMA dello slider che referenzia.
    knobAttachment.reset();
}

//==============================================================================
bool MacroSlotComponent::isInLinkMode() const noexcept
{
    return currentLinkGroup != nullptr && currentLinkGroup->getNumParameters() >= 2;
}

//==============================================================================
void MacroSlotComponent::rebuild()
{
    clearContent();

    // Mostra un widget SOLO se il macro ha una mappatura (link in modo non-advanced
    // oppure assignment in modo advanced). Senza nulla mappato la cella resta vuota
    // (solo gear + nome) -- e ci ritorna se si rimuove la mappatura.
    if (macroParam != nullptr)
    {
        if (isInLinkMode())
        {
            // Non-advanced: il target linkato. Continuo -> rotary di default (il
            // ParameterLinkGroup mirrora sul target); choice/discreto -> widget
            // constrained del target.
            auto* target = currentLinkGroup->getParameter (1);
            if (target != nullptr && ! target->isBoolean() && ! target->isDiscrete())
                showRotaryKnob();
            else
                showLinkView();
        }
        else if (macroParam->getNumAssignments() > 0)
        {
            // Advanced: widget per il controlType del macro.
            if (macroParam->getControlType() == ayra::MacroControlType::continuous)
                showRotaryKnob();
            else
                showMacroDisplay();
        }
        // else: niente mappato -> cella vuota.
    }

    updateColours();
    applyMappingTooltip();   // i sub-widget appena creati ereditano il tooltip dei mapping
    resized();
    repaint();
}

void MacroSlotComponent::clearContent()
{
    knobAttachment.reset();   // prima dello slider
    knob.reset();
    macroDisplay.reset();
    linkDisplay.reset();
}

void MacroSlotComponent::showRotaryKnob()
{
    knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                           juce::Slider::NoTextBox);
    // Arc + thumb del colore d'accento della macro (custom, o accent dell'app se la
    // macro non ha colore custom): lo slot esiste solo se mappato -> sempre colorato.
    knob->setColour (juce::Slider::rotarySliderFillColourId, macroAccentColour());
    addAndMakeVisible (*knob);

    // Attacco DIRETTO al param APVTS normalizzato 0..1 (gesture + sync inclusi).
    // Il look rotary (arco accent + well) arriva da MultiverseLookAndFeel.
    if (auto* p = valueTreeState.getParameter (paramId))
        knobAttachment = std::make_unique<juce::SliderParameterAttachment> (*p, *knob);
}

void MacroSlotComponent::showMacroDisplay()
{
    // Riuso del mattone del modulo per i tipi NON-slider (toggle/momentary/multiChoice).
    macroDisplay = std::make_unique<ayra::MacroDisplayComponent> (macroParam, valueTreeState, paramId);
    macroDisplay->setLabelPosition (ayra::MacroDisplayComponent::LabelPosition::None);
    macroDisplay->setAccentColour (macroAccentColour());   // toggle/momentary/multiChoice = colore macro

    // multiChoice resa come ComboBox (>4 scelte): il testo della scelta selezionata
    // si tinge col colore di rotazione di quella choice (stessa palette/logica di
    // ControlsComponent::choiceColourFor, che pero' vive in un altro file). Il modulo
    // non conosce la palette d'app -> gliela passiamo come provider per-indice.
    macroDisplay->setChoiceColourProvider ([] (int choiceIndex) -> juce::Colour
    {
        const auto swatches = ayra::AppearanceSettings::getDefaultSwatches();
        if (swatches.isEmpty()) { return tokens::accent(); }
        return swatches[choiceIndex % swatches.size()];
    });

    addAndMakeVisible (*macroDisplay);
}

void MacroSlotComponent::showLinkView()
{
    if (currentLinkGroup == nullptr || currentLinkGroup->getNumParameters() < 2)
        return;

    if (auto* targetParam = currentLinkGroup->getParameter (1))
    {
        linkDisplay = ayra::ConstrainedComponentFactory::create (*targetParam);
        if (linkDisplay != nullptr)
        {
            // Tinge il widget del target (checkbox/segmented/slider) col colore macro.
            tintConstrainedLinkDisplay (linkDisplay.get(), macroAccentColour());
            addAndMakeVisible (*linkDisplay);
        }
    }
}

//==============================================================================
void MacroSlotComponent::setLinkGroup (ayra::ParameterLinkGroup* group)
{
    currentLinkGroup = group;
    rebuild();
}

//==============================================================================
juce::Colour MacroSlotComponent::identityColour() const
{
    // Colore custom della macro, altrimenti la cap di default dai token.
    return macroColourOr (
        macroParam,
        tokens::componentColour (*this, colourCapColourId, tokens::borderHairline()));
}

juce::Colour MacroSlotComponent::macroAccentColour() const
{
    // Colore custom della macro, altrimenti l'accent dell'app (lo slot esiste solo
    // se mappato -> il widget porta sempre un colore).
    return macroColourOr (macroParam, tokens::accent());
}

//==============================================================================
juce::String MacroSlotComponent::nodeNameForParam (juce::AudioProcessorParameter* p) const
{
    if (p == nullptr) { return {}; }

    auto& doc = graph.getDocument();
    for (const auto& uuid : doc.getAllNodeUuids())
        if (auto* proc = graph.getProcessorForNode (uuid))
            for (auto* np : proc->getParameters())
                if (np == p)
                    return doc.getNodeName (uuid);

    // Fallback: parametro ROOT del grafo (es. un quick_control).
    for (auto* gp : graph.getParameters())
        if (gp == p)
            return graph.getName();

    return {};
}

std::unordered_map<juce::AudioProcessorParameter*, juce::String>
MacroSlotComponent::buildParamNodeNameMap() const
{
    // Costruzione UNA volta per tooltip: scansione lineare del documento (nodi x
    // parametri) anziche' un nodeNameForParam O(nodi x parametri) PER assignment.
    // Semantica identica a nodeNameForParam: priorita' ai parametri di nodo, poi
    // fallback ai parametri ROOT del grafo (insert solo se assente -> node-first).
    std::unordered_map<juce::AudioProcessorParameter*, juce::String> map;

    auto& doc = graph.getDocument();
    for (const auto& uuid : doc.getAllNodeUuids())
        if (auto* proc = graph.getProcessorForNode (uuid))
        {
            const auto name = doc.getNodeName (uuid);
            for (auto* np : proc->getParameters())
                map.emplace (np, name);
        }

    // Fallback param ROOT: emplace non sovrascrive un'eventuale entry di nodo.
    for (auto* gp : graph.getParameters())
        map.emplace (gp, graph.getName());

    return map;
}

juce::String MacroSlotComponent::buildMappingTooltip() const
{
    if (macroParam == nullptr) { return {}; }

    const auto paramNodeNames = buildParamNodeNameMap();

    auto describe = [&paramNodeNames] (juce::AudioProcessorParameter* p) -> juce::String
    {
        if (p == nullptr) { return {}; }
        const auto it   = paramNodeNames.find (p);
        const auto node = (it != paramNodeNames.end()) ? it->second : juce::String();
        return node.isNotEmpty() ? (p->getName (40) + " (" + node + ")")
                                 : p->getName (40);
    };

    // Link mode: il singolo target linkato.
    if (isInLinkMode())
    {
        if (auto* t = currentLinkGroup->getParameter (1))
            return "Linked to: " + describe (t);
        return {};
    }

    // Advanced: elenco degli assignment (param + nodo).
    juce::StringArray lines;
    for (int i = 0; i < macroParam->getNumAssignments(); ++i)
        if (auto* a = macroParam->getAssignment (i))
            if (auto* t = a->getTargetParameter())
                lines.add ("- " + describe (t));

    if (lines.isEmpty()) { return {}; }
    return "Mapped controls:\n" + lines.joinIntoString ("\n");
}

void MacroSlotComponent::applyMappingTooltip()
{
    const auto tip = buildMappingTooltip();
    setTooltip (tip);   // lo slot stesso copre le aree vuote (SettableTooltipClient)

    // Il TooltipWindow usa il component DIRETTAMENTE sotto il mouse (non risale i
    // parent): il tooltip va messo su OGNI sub-widget. Escluso il gear, che tiene
    // il suo "Macro Map".
    std::function<void (juce::Component&)> propagate =
        [this, &tip, &propagate] (juce::Component& c)
        {
            for (int i = 0; i < c.getNumChildComponents(); ++i)
            {
                auto* child = c.getChildComponent (i);
                if (child == nullptr || child == &settingsButton) { continue; }
                if (auto* ttc = dynamic_cast<juce::SettableTooltipClient*> (child))
                    ttc->setTooltip (tip);
                propagate (*child);
            }
        };
    propagate (*this);
}

void MacroSlotComponent::updateColours()
{
    const bool hasMacroColour = hasCustomMacroColour (macroParam);

    nameLabel.setColour (juce::Label::textColourId,
                         hasMacroColour ? macroParam->getColour().brighter (0.4f)
                                        : tokens::componentColour (
                                              *this, nameTextColourId,
                                              tokens::textSecondary()));

    // Re-tinta live il widget del controllo quando cambia il colore del macro
    // (senza rebuild). Ogni widget porta l'accent della macro (custom o accent app).
    const auto accent = macroAccentColour();
    if (knob != nullptr)
    {
        knob->setColour (juce::Slider::rotarySliderFillColourId, accent);
        knob->repaint();
    }
    if (macroDisplay != nullptr)
        macroDisplay->setAccentColour (accent);
    tintConstrainedLinkDisplay (linkDisplay.get(), accent);
}

//==============================================================================
void MacroSlotComponent::paint (juce::Graphics& g)
{
    const auto bg = tokens::componentColour (
        *this, backgroundColourId, juce::Colours::transparentBlack);
    if (! bg.isTransparent())
    {
        g.setColour (bg);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }

    // Colour cap d'identita': barretta arrotondata in cima.
    auto cap = getLocalBounds().reduced (8, 4).removeFromTop (3).toFloat();
    g.setColour (identityColour());
    g.fillRoundedRectangle (cap, 1.5f);
}

void MacroSlotComponent::lookAndFeelChanged()
{
    juce::Component::lookAndFeelChanged();
    updateColours();
}

void MacroSlotComponent::resized()
{
    auto area = getLocalBounds().reduced (6, 4);

    area.removeFromTop (5);              // spazio per la colour cap (disegnata in paint)
    nameLabel.setBounds (area.removeFromBottom (16));
    area.removeFromBottom (2);

    if (knob != nullptr)              { knob->setBounds (area); }
    else if (macroDisplay != nullptr){ macroDisplay->setBounds (area); }
    else if (linkDisplay != nullptr) { linkDisplay->setBounds (area); }

    // Gear in alto a destra (18x18 in colonna da 20, come la card Snapshot).
    // toFront: il contenuto (knob/display) viene aggiunto DOPO nel rebuild e si
    // sovrapporrebbe al gear nell'angolo, intercettandone i click.
    settingsButton.setBounds (getLocalBounds().reduced (4).removeFromTop (20)
                                  .removeFromRight (20).withSizeKeepingCentre (18, 18));
    settingsButton.toFront (false);
}

//==============================================================================
// ayra::MacroParameter::Listener
//==============================================================================
void MacroSlotComponent::onMacroNameChanged (ayra::MacroParameter*)
{
    if (macroParam != nullptr)
        nameLabel.setText (macroParam->getName(), juce::dontSendNotification);
}

void MacroSlotComponent::onMacroColourChanged (ayra::MacroParameter*)
{
    updateColours();
    repaint();
}

void MacroSlotComponent::onMacroControlTypeChanged (ayra::MacroParameter*)
{
    // Cambio continuous <-> altri tipi (rotary vs MacroDisplayComponent): rebuild
    // deferred sul message thread (il cambio puo' arrivare da un contesto in cui
    // ricostruire i child sincrono non e' sicuro).
    juce::MessageManager::callAsync (
        [safe = juce::Component::SafePointer<MacroSlotComponent> (this)]
        {
            if (safe != nullptr)
                safe->rebuild();
        });
}

//==============================================================================
void MacroSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() && onOpenMacroMap)
        onOpenMacroMap (*this);
}

void MacroSlotComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onOpenMacroMap)
        onOpenMacroMap (*this);
}

} // namespace ayra
