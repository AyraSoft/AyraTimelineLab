/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / dialogs / SectionImportDialog
   ------------------------------------------------------------------
   Dialog ASINCRONO di scelta sezioni all'importazione di un workspace
   (*.ayws) o di un file affine (*.ayw, *.ayt): mostra le sezioni TROVATE nel
   file, tutte gia' spuntate, e quelle assenti disabilitate.

   ## Perche' esiste
   Il flusso di sessione Ayra separa le decisioni: al SALVATAGGIO si scrive
   sempre tutto (zero domande, zero click); all'APERTURA si sceglie cosa
   caricare. Questo dialog e' il punto unico di quella scelta: la disponibilita'
   delle sezioni e' un fatto del file, non una preferenza del dialog.

   ## Cosa NON decide
   Nessuna logica di applicazione vive qui: il dialog raccoglie solo l'insieme
   degli identifier selezionati e lo consegna alla callback. L'ordine di
   applicazione e la validazione dei payload restano del chiamante (owner
   canonico di ciascuna sezione).

   ## Default a minimo click
   Sezioni disponibili -> spuntate (Invio = importa tutto). Sezioni assenti ->
   disabilitate e non spuntate (non selezionabili perche' il file non le
   contiene). Annulla/Esc -> callback con `accepted == false`.

   ## Uso
   @code
   ayra::showSectionImportDialogAsync (*this, "Import Workspace",
       "Choose the sections to load from this file:",
       { { "document", "Document", "Graph, macros, mappings", true },
         { "devices",  "Devices",  "Audio/MIDI/DMX/OSC state", true },
         { "layout",   "Panels",   "Dock arrangement",         false } },
       [safe] (bool accepted, const juce::StringArray& selected)
       {
           if (safe != nullptr && accepted) { safe->applyImportSelection (selected); }
       });
   @endcode

   Doctrine: header-only (inline), ZERO #include (juce e' gia' visibile via
   ayra_common.h), namespace ayra, async SEMPRE (niente modal loop annidato:
   vedi la nota in ayra_Dialogs.h), ASCII-only nei letterali stringa.

  ==============================================================================
*/

#pragma once

namespace ayra
{

//==============================================================================
/** Descrive UNA sezione presentata nel dialog di import.

    - `identifier`  chiave stabile concordata col chiamante (es. "document");
    - `title`       etichetta utente della riga (es. "Document");
    - `description` una riga che dice cosa carica quella sezione;
    - `available`   la sezione e' presente nel file scelto: solo le disponibili
                    sono spuntabili; le altre appaiono disabilitate. */
struct SectionImportItem
{
    juce::String identifier;
    juce::String title;
    juce::String description;
    bool available = false;
};

//==============================================================================
/** Colonne di righe toggle+descrizione mostrate dentro l'AlertWindow.
    Componente puro di layout: niente paint custom, la resa visiva e' tutta
    della LAF (Frosted Lab ereditata dall'anchor). */
class SectionImportRows final : public juce::Component
{
public:
    explicit SectionImportRows (const std::vector<SectionImportItem>& items)
    {
        for (const auto& item : items)
        {
            auto* toggle = toggles.add (new juce::ToggleButton (item.title));
            // Disponibile = spuntata di default e modificabile; assente =
            // disabilitata e non spuntata (non e' nel file).
            toggle->setToggleState (item.available, juce::dontSendNotification);
            toggle->setEnabled (item.available);
            addAndMakeVisible (*toggle);

            auto* description = descriptions.add (new juce::Label ({}, item.description));
            description->setColour (juce::Label::textColourId, tokens::textSecondary());
            description->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (*description);

            identifiers.add (item.identifier);
        }

        setSize (kWidth, kRowHeight * (int) items.size() + kBottomPadding);
    }

    /** Identifier delle sezioni disponibili E spuntate, nell'ordine mostrato. */
    [[nodiscard]] juce::StringArray getSelectedIdentifiers() const
    {
        juce::StringArray selected;
        for (int i = 0; i < toggles.size(); ++i)
            if (toggles[i]->getToggleState() && toggles[i]->isEnabled())
                selected.add (identifiers[i]);
        return selected;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        for (int i = 0; i < toggles.size(); ++i)
        {
            auto row = bounds.removeFromTop (kRowHeight).reduced (kMargin, 0);
            toggles[i]->setBounds (row.removeFromTop (kToggleHeight));
            descriptions[i]->setBounds (row.withTrimmedLeft (kDescriptionIndent));
        }
    }

private:
    static constexpr int kWidth             = 440;
    static constexpr int kRowHeight         = 44;
    static constexpr int kToggleHeight      = 24;
    static constexpr int kMargin            = 8;
    static constexpr int kDescriptionIndent = 26;
    static constexpr int kBottomPadding     = 4;

    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::Label>        descriptions;
    juce::StringArray                    identifiers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionImportRows)
};

//==============================================================================
/** Dialog di scelta sezioni: ASINCRONO, stilato con la LAF dell'anchor
    (pattern di ayra_Dialogs.h). Ritorna subito; il dialog vive sul desktop e
    si auto-distrugge alla chiusura.

    @param anchor           component vivo che porta la LAF d'app (tipicamente
                            `*this`); se la callback lo tocca, catturare un
                            `juce::Component::SafePointer` (il dialog e' modale
                            ma il chiamante puo' morire prima che giri).
    @param items            sezioni da mostrare (vedi `SectionImportItem`).
    @param onResult         invocato sul message thread con: `accepted` true se
                            confermato (Import / Return); `selected` = gli
                            identifier spuntati (vuoto se annullato). */
inline void showSectionImportDialogAsync (
    juce::Component& anchor,
    const juce::String& title,
    const juce::String& informativeText,
    const std::vector<SectionImportItem>& items,
    std::function<void (bool accepted, const juce::StringArray& selected)> onResult,
    const juce::String& okText     = "Import",
    const juce::String& cancelText = "Cancel")
{
    auto* dialog = new juce::AlertWindow (
        title, informativeText, juce::MessageBoxIconType::NoIcon, &anchor);
    dialog->setLookAndFeel (&anchor.getLookAndFeel());

    auto* rows = new SectionImportRows (items);
    dialog->addCustomComponent (rows);   // ownership: figlio dell'AlertWindow

    dialog->addButton (okText,     1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton (cancelText, 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [rows, cb = std::move (onResult)] (int result)
            {
                // deleteWhenDismissed invoca la callback PRIMA della
                // distruzione: leggere i toggle qui e' sicuro.
                if (cb == nullptr) { return; }
                if (result == 1) { cb (true, rows->getSelectedIdentifiers()); }
                else             { cb (false, {}); }
            }),
        true);   // deleteWhenDismissed
}

} // namespace ayra
