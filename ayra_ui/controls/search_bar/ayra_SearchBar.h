/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / SearchBar
   ------------------------------------------------------------------------------
   Barra di ricerca riusabile in stile Xcode (Frosted Lab): campo testo + toggle
   modalita' (Contains / Matches) + toggle case-sensitive (Aa) + bottone "more"
   opzionale che il consumer usa per aprire un popup di filtri custom.

   ## Perche' esiste
   Sostituisce il pattern ad-hoc "TextEditor + bottone '...' che apre un
   juce::PopupMenu nativo" sparso nei browser di parametri/liste. Generica e
   on-brand: NON sa nulla del dominio filtrato (parametri, file, ...). Il match
   lo incapsula in `Query::matches`, cosi' i consumer non lo reimplementano.

   ## Uso
   @code
   ayra::SearchBar bar;
   bar.setPlaceholder ("Filter parameters");
   bar.setShowMoreButton (true);
   bar.onChange      = [this] (const ayra::SearchBar::Query& q) { rebuild(); };
   bar.onSubmit      = [this] (const ayra::SearchBar::Query& q) { revealNext (q); };
   bar.onMoreOptions = [this] (juce::Component& anchor) { openFilterPopup (anchor); };
   // ... in buildPredicate:
   if (! bar.getQuery().matches (candidateName)) return false;
   @endcode

   Doctrine: ZERO #include (tipi via ayra_common.h), namespace ayra, ASCII-only.
   Anatomia widget Ayra: ColourIds, doppia API (Listener + std::function),
   default funzionante out-of-the-box, niente singleton.
  ==============================================================================
*/

#pragma once

namespace ayra
{

class SearchBar : public juce::Component
{
public:
    //==========================================================================
    enum ColourIds
    {
        backgroundColourId = 0x3b00140, ///< Fondo del campo (well recessed).
        textColourId       = 0x3b00141, ///< Testo digitato.
        placeholderColourId = 0x3b00142 ///< Placeholder.
    };

    //==========================================================================
    /// Come confrontare il testo col candidato.
    enum class MatchMode
    {
        contains = 0, ///< Il candidato CONTIENE il testo.
        matches       ///< Il candidato e' UGUALE al testo (match esatto).
    };

    /// Query corrente. `matches()` incapsula la logica di confronto (case + mode)
    /// cosi' ogni consumer la applica in modo identico.
    struct Query
    {
        juce::String text;
        MatchMode    mode          = MatchMode::contains;
        bool         caseSensitive = false;

        [[nodiscard]] bool isEmpty() const noexcept { return text.isEmpty(); }

        [[nodiscard]] bool matches (const juce::String& candidate) const noexcept
        {
            if (text.isEmpty()) { return true; }   // nessun filtro -> passa tutto
            if (caseSensitive)
                return mode == MatchMode::contains ? candidate.contains (text)
                                                   : candidate == text;
            return mode == MatchMode::contains ? candidate.containsIgnoreCase (text)
                                               : candidate.equalsIgnoreCase (text);
        }
    };

    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void searchBarChanged (SearchBar*, const Query&) = 0;
        virtual void searchBarSubmitted (SearchBar*, const Query&) {}
    };

    //==========================================================================
    SearchBar();
    ~SearchBar() override;

    /// Altezza nativa consigliata della barra (compatta: testo centrato sulle Y).
    static constexpr int kHeight = 24;

    void setPlaceholder (const juce::String& text);

    /// Mostra/nasconde il bottone "more" (default nascosto). Quando cliccato
    /// chiama `onMoreOptions(*this)`: il consumer apre il proprio popup custom.
    void setShowMoreButton (bool shouldShow);

    [[nodiscard]] Query getQuery() const noexcept { return query; }
    void setText (const juce::String& text, juce::NotificationType notify = juce::sendNotification);
    [[nodiscard]] juce::String getText() const noexcept { return query.text; }

    /** Porta il focus nel campo testuale interno, non sul contenitore composito. */
    void grabTextFocus();

    //==========================================================================
    void addListener (Listener*);
    void removeListener (Listener*);

    std::function<void (const Query&)>      onChange;
    std::function<void (const Query&)>      onSubmit;
    std::function<void (juce::Component& /*anchor*/)> onMoreOptions;

    //==========================================================================
    void resized() override;
    void lookAndFeelChanged() override;
    void colourChanged() override;

private:
    void fireChange();
    void fireSubmit();
    void applyTextFieldColours();

    Query query;
    juce::String placeholder { "Search" };

    juce::TextEditor   textField;
    SettingSegmented   modeSeg;            // Contains | Matches
    juce::TextButton   caseButton { "Aa" }; // toggle case-sensitive
    ShapeIconButton    moreButton;         // opzionale (filtri extra)
    bool               showMore = false;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchBar)
};

} // namespace ayra
