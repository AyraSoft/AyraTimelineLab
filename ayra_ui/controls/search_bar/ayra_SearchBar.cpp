/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / SearchBar (impl)
   ------------------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (tipi gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
SearchBar::SearchBar()
{
    // Campo testo: well recessed on-brand (stessa ricetta degli editor del macro map).
    textField.setFont (tokens::type::body());
    textField.setJustification (juce::Justification::centredLeft);   // testo centrato sulle Y
    textField.setIndents (12, 0);                                    // margine sinistro + centratura V
    applyTextFieldColours();
    textField.onTextChange = [this] { query.text = textField.getText(); fireChange(); };
    textField.onReturnKey = [this] { fireSubmit(); };
    addAndMakeVisible (textField);

    // Modalita' contains/matches (segmented on-brand).
    modeSeg.setOptions ({ "Contains", "Matches" });
    modeSeg.setSelectedIndex (0, juce::dontSendNotification);
    modeSeg.onChange = [this] (int idx)
    {
        query.mode = (idx == 1) ? MatchMode::matches : MatchMode::contains;
        fireChange();
    };
    addAndMakeVisible (modeSeg);

    // Toggle case-sensitive: pill "Aa" che si accende in accent quando attivo.
    caseButton.setClickingTogglesState (true);
    caseButton.setTooltip ("Case sensitive");
    caseButton.onClick = [this]
    {
        query.caseSensitive = caseButton.getToggleState();
        fireChange();
    };
    addAndMakeVisible (caseButton);

    // Bottone "more": glifo fader (filtri). Nascosto finche' setShowMoreButton(true).
    moreButton.setGlyph (glyphs::sliders);
    moreButton.setTooltip ("More filters");
    moreButton.onClick = [this] { if (onMoreOptions) { onMoreOptions (*this); } };
    addChildComponent (moreButton);
}

SearchBar::~SearchBar() = default;

//==============================================================================
void SearchBar::setPlaceholder (const juce::String& text)
{
    if (placeholder == text) { return; }
    placeholder = text;
    applyTextFieldColours();
}

void SearchBar::setShowMoreButton (bool shouldShow)
{
    if (showMore == shouldShow) { return; }
    showMore = shouldShow;
    moreButton.setVisible (shouldShow);
    resized();
}

void SearchBar::setText (const juce::String& text, juce::NotificationType notify)
{
    query.text = text;
    textField.setText (text, juce::dontSendNotification);
    if (notify != juce::dontSendNotification) { fireChange(); }
}

void SearchBar::grabTextFocus()
{
    textField.grabKeyboardFocus();
}

//==============================================================================
void SearchBar::addListener (Listener* l)    { listeners.add (l); }
void SearchBar::removeListener (Listener* l) { listeners.remove (l); }

void SearchBar::fireChange()
{
    const juce::Component::BailOutChecker checker (this);
    listeners.callChecked (
        checker, [this] (Listener& l) { l.searchBarChanged (this, query); });
    if (! checker.shouldBailOut() && onChange) { onChange (query); }
}

void SearchBar::fireSubmit()
{
    const juce::Component::BailOutChecker checker (this);
    listeners.callChecked (
        checker, [this] (Listener& l) { l.searchBarSubmitted (this, query); });
    if (! checker.shouldBailOut() && onSubmit) { onSubmit (query); }
}

//==============================================================================
void SearchBar::lookAndFeelChanged()
{
    juce::Component::lookAndFeelChanged();
    applyTextFieldColours();
}

void SearchBar::colourChanged()
{
    juce::Component::colourChanged();
    applyTextFieldColours();
}

void SearchBar::applyTextFieldColours()
{
    // Precedenza: override SearchBar -> ColourId SearchBar nel LAF -> colore
    // TextEditor del LAF -> token. Cosi' l'API specifica del widget resta viva
    // senza scavalcare un LookAndFeel host che personalizza gli editor standard.
    const auto resolveEditorColour = [this] (int searchId, int editorId,
                                             juce::Colour fallback)
    {
        if (isColourSpecified (searchId))
            return findColour (searchId);

        auto& laf = getLookAndFeel();
        if (laf.isColourSpecified (searchId))
            return laf.findColour (searchId);
        if (laf.isColourSpecified (editorId))
            return laf.findColour (editorId);
        return fallback;
    };

    textField.setColour (
        juce::TextEditor::backgroundColourId,
        resolveEditorColour (backgroundColourId,
                             juce::TextEditor::backgroundColourId,
                             tokens::surfaceRecessed()));
    textField.setColour (
        juce::TextEditor::textColourId,
        resolveEditorColour (textColourId,
                             juce::TextEditor::textColourId,
                             tokens::textPrimary()));
    textField.setTextToShowWhenEmpty (
        placeholder,
        tokens::componentColour (*this, placeholderColourId, tokens::textSecondary()));
}

void SearchBar::resized()
{
    auto area = getLocalBounds();
    constexpr int gap = 6;

    // Right -> left: more (opz), case, mode; il campo testo riempie il resto.
    if (showMore)
    {
        moreButton.setBounds (area.removeFromRight (area.getHeight()));
        area.removeFromRight (gap);
    }

    caseButton.setBounds (area.removeFromRight (34));
    area.removeFromRight (gap);

    modeSeg.setBounds (area.removeFromRight (juce::jmin (150, area.getWidth() / 2)));
    area.removeFromRight (gap);

    textField.setBounds (area);
}

} // namespace ayra
