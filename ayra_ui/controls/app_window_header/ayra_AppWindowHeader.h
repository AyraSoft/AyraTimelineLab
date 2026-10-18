/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / AppWindowHeader
   ------------------------------------------------------------------
   Header canonico delle superfici secondarie Ayra: icona, titolo e azioni.

  ==============================================================================
*/

#pragma once

namespace ayra
{

/** Chrome condivisa per finestre, viste floating e callout dell'app.

    Il widget e' l'unica fonte di verita' per altezza, paint, dimensione dei
    bottoni, inset, tinta, ordine e spaziatura delle azioni. Il consumer
    specifica soltanto icona, titolo, tooltip e callback dei comandi; non
    possiede stato di dominio.

    Le azioni vengono mostrate da sinistra a destra nell'ordine di inserimento.
    Il bottone restituito conserva la normale dual API JUCE (`addListener` e
    `onClick`). */
class AppWindowHeader final : public juce::Component
{
public:
    static constexpr int preferredHeight = tokens::layout::windowHeaderH;

    AppWindowHeader (glyphs::Glyph icon, juce::String title);
    ~AppWindowHeader() override = default;

    void setIcon (glyphs::Glyph);
    void setTitle (juce::String);
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }

    /** Aggiunge un comando icona standard. `leadingGap` separa questa azione
        dalla precedente nell'ordine visivo; viene ignorato per la prima. */
    ShapeIconButton& addAction (const glyphs::Glyph&, juce::String tooltip,
                                int leadingGap = tokens::layout::actionGap - 4);

    /** Aggiunge un'azione che resta sempre dopo le azioni ordinarie. Le azioni
        ordinarie aggiunte successivamente vengono inserite prima di questo
        gruppo. Utile per la chiusura canonica, che deve restare a destra. */
    ShapeIconButton& addTrailingAction (
        const glyphs::Glyph&, juce::String tooltip,
        int leadingGap = tokens::layout::actionGap - 4);

    /** Aggiunge il comando `+` canonico mantenendo lo stesso layout delle altre
        azioni dell'header. */
    AddButton& addAddAction (juce::String tooltip,
                             int leadingGap = tokens::layout::actionGap - 4);

    /** Mostra o nasconde un'azione posseduta da questo header e rilayouta le
        azioni visibili. */
    void setActionVisible (ShapeIconButton&, bool shouldBeVisible);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ShapeIconButton& addActionInternal (
        const glyphs::Glyph&, juce::String tooltip,
        int leadingGap, bool trailing);

    template <typename ButtonType>
    ButtonType& addOwnedAction (
        std::unique_ptr<ButtonType>, int leadingGap, bool trailing = false);

    glyphs::Glyph icon;
    juce::String title;
    juce::OwnedArray<ShapeIconButton> actions;
    juce::Array<int> leadingGaps;
    int trailingActionCount = 0;

    static constexpr int horizontalInset = 16;
    static constexpr int actionSize = tokens::layout::buttonH;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppWindowHeader)
};

} // namespace ayra
