/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / tab_strip / TabStrip
   ------------------------------------------------------------------
   Selettore a TAB orizzontale "Frosted Lab" riusabile: un well recessed con il
   segmento attivo evidenziato da una pill accent; ogni tab e' [icona] [label]
   [badge] (tutti opzionali tranne la label). Layout ADATTIVO:
     - se le tab ci stanno tutte -> si allargano per RIEMPIRE la striscia;
     - se non ci stanno -> larghezza naturale + SCROLL orizzontale (mouse wheel /
       trackpad), con fade-edge a indicare il contenuto fuori vista.

   Nato come tab strip della finestra di configurazione nodo (NodeSettings),
   estratto qui come controllo generale del design system. Consumer:
     - NodeSettingsComponent (tab Node/I/O/MIDI/... con icona)
     - ParameterMappingComponent (tab dei cavi CV, con badge = n. mapping)
     - SnapshotInspectorComponent (64 gruppi snapshot, scrollabile, tinta=colore gruppo)

   ## Doctrine
   - Pura VIEW: nessuno stato proprio oltre alla selezione/scroll; il modello
     (lista di Tab) lo possiede il chiamante via `setTabs`.
   - Token-driven (`ayra::tokens` + `fillRecessedSurface` + `glyphs::drawGlyph`):
     tema-aware (light/dark) senza setup. Per-tab si puo' forzare la tinta della
     pill (es. colore del gruppo snapshot) via `Tab::accent`.
   - Doppia API eventi: `Listener` formale + `std::function onTabSelected`.
   - Solo message thread (e' GUI). ZERO #include (tipi via ayra_common.h).

   ## Uso
   @code
     ayra::TabStrip strip;
     strip.setTabs ({ { "Node", glyphs::chip },
                      { "I/O",  glyphs::stack } });
     strip.onTabSelected = [this] (int i) { showPage (i); };
     strip.setSelectedIndex (0, juce::dontSendNotification);
   @endcode
  ==============================================================================
*/

#pragma once

namespace ayra
{

//==============================================================================
/** Tab strip orizzontale on-brand, riempi-o-scrolla. Vedi header del file. */
class TabStrip final : public juce::Component,
                       private juce::ScrollBar::Listener
{
public:
    enum class SelectionMode
    {
        single,
        multiFilterWithAll
    };

    //==========================================================================
    /** Modello immutabile di una tab. `label` e' l'unico campo obbligatorio. */
    struct Tab
    {
        juce::String  label;
        glyphs::Glyph icon;                  ///< glifo opzionale (Glyph vuoto = nessuna icona).
        int           badge  = 0;            ///< contatore a destra (0 = nessuno).
        juce::Colour  accent;               ///< tinta pill attiva (trasparente = accent del tema).
    };

    //==========================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void tabStripSelectionChanged (TabStrip*, int index) = 0;
    };

    //==========================================================================
    TabStrip();
    ~TabStrip() override;

    /** Altezza nativa consigliata (il chiamante puo' darne un'altra). */
    static constexpr int kHeight = 40;

    /** Rimpiazza l'intera lista di tab. Preserva la selezione per indice se
        ancora valida (altrimenti clampa, o -1 se vuota). Non notifica. */
    void setTabs (std::vector<Tab> newTabs);

    void setSelectionMode (SelectionMode);
    [[nodiscard]] SelectionMode getSelectionMode() const noexcept { return selectionMode; }

    void setSelectedIndex (int index, juce::NotificationType notify = juce::sendNotification);
    [[nodiscard]] int getSelectedIndex() const noexcept { return selected; }
    [[nodiscard]] bool isTabSelected (int index) const noexcept;
    [[nodiscard]] int getNumTabs()       const noexcept { return (int) tabs.size(); }
    [[nodiscard]] const std::vector<Tab>& getTabs() const noexcept { return tabs; }
    [[nodiscard]] const Tab* getTab (int index) const noexcept
    {
        return juce::isPositiveAndBelow (index, (int) tabs.size()) ? &tabs[(std::size_t) index] : nullptr;
    }

    void setTabSelected (int index, bool shouldBeSelected, juce::NotificationType notify = juce::sendNotification);
    void setSelectedIndices (const std::vector<int>& indices, juce::NotificationType notify = juce::sendNotification);
    [[nodiscard]] std::vector<int> getSelectedIndices() const;

    //==========================================================================
    void addListener (Listener*);
    void removeListener (Listener*);
    std::function<void (int)> onTabSelected;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    //==========================================================================
    void  relayout();                                  ///< ricalcola slot, fill-vs-scroll, contentW.
    void  clampScroll() noexcept;
    void  ensureVisible (int index);
    [[nodiscard]] int   tabAtX (float xInComp) const;  ///< indice tab sotto x (component space); -1.
    [[nodiscard]] float measureTabWidth (const Tab&, float textW) const; ///< textW = larghezza testo body gia' misurata.
    [[nodiscard]] int   tabAreaHeight() const noexcept;///< altezza area tab (esclude lo scrollbar se visibile).
    void  notifySelection();
    void  toggleMultiFilterTab (int index);
    void  repaintTab (int index);
    void  updateScrollBar();                           ///< mostra/dimensiona lo scrollbar (solo se overflow).

    // juce::ScrollBar::Listener: drag dello scrollbar -> aggiorna scrollX.
    void  scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    std::vector<Tab>                    tabs;
    std::vector<juce::Rectangle<float>> slots;   ///< 1:1 con tabs, x in content-space (pre-scroll).
    std::vector<float>                  tabTextW; ///< 1:1 con tabs, larghezza testo body cache-ata (relayout -> paint).
    SelectionMode selectionMode { SelectionMode::single };
    std::vector<bool> multiSelected;
    int   selected = -1;
    int   hover    = -1;
    float scrollX  = 0.0f;
    float contentW = 0.0f;
    bool  scrollable = false;   ///< true se contentW > larghezza (scroll attivo, niente fill).

    // Scrollbar orizzontale: VISIBILE solo quando ci sono tab fuori vista, cosi'
    // si puo' scrollare col mouse (drag) anche senza trackpad/rotella orizzontale.
    juce::ScrollBar hScroll { false };

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabStrip)
};

} // namespace ayra
