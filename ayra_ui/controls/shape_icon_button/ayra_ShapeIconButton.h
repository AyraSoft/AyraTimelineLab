/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / ShapeIconButton
   ------------------------------------------------------------------
   Bottone a ICONA (path o glyph del design system), coerente col LookAndFeel.
   Riusabile ovunque serva un comando grafico: transport (play/stop/rewind),
   control bar (MIDI panic, buffer, oversampling), toolbar, ...

   ## Perche' non `juce::ShapeButton`
   `juce::ShapeButton` non ha uno stato "on" (toggle) disegnato, ne' lo sfondo
   "chip" reattivo a hover/down/toggle che usiamo nel footer, ne' i nostri
   `ColourIds`/token. Questo bottone aggiunge esattamente quelle tre cose
   restando minimale: una sola `juce::Path` scalata a fit, tinta per stato, su
   uno sfondo opzionale arrotondato.

   ## Uso
   @code
   ayra::ShapeIconButton panic;
   panic.setGlyph (ayra::glyphs::midiPanic);         // glyph del design system
   panic.onClick = [&]{ lifecycle.requestMidiPanic(); };

   ayra::ShapeIconButton buffer;
   buffer.setShape (myBufferPath);                   // path custom
   buffer.setClickingTogglesState (true);            // toggle (juce::Button)
   buffer.onClick = [&]{ lifecycle.setBufferize (buffer.getToggleState()); };
   @endcode
  ==============================================================================
*/

#pragma once

namespace ayra
{

class ShapeIconButton : public juce::Button
{
public:
    //==========================================================================
    /// Colori (range widget condivisi `0x3b000xx`). Default risolti a paint
    /// dai token e dall'accento correnti; gli override semantici dei caller
    /// restano locali.
    enum ColourIds
    {
        iconColourId         = 0x3b00030, ///< Icona a riposo.
        iconOverColourId     = 0x3b00031, ///< Icona in hover.
        iconDownColourId     = 0x3b00032, ///< Icona premuta.
        iconOnColourId       = 0x3b00033, ///< Icona con toggle ON.
        backgroundColourId   = 0x3b00034, ///< Sfondo "chip" in hover/down.
        backgroundOnColourId = 0x3b00035, ///< Sfondo "chip" con toggle ON.
        backgroundIdleColourId = 0x3b00036, ///< Sfondo "chip" a riposo (off, no hover) quando showIdleBackground e' attivo.
        backgroundIdleOutlineColourId = 0x3b00037 ///< Bordo del chip a riposo.
    };

    //==========================================================================
    explicit ShapeIconButton (const juce::String& name = {});
    ~ShapeIconButton() override = default;

    //==========================================================================
    /// Forma in coordinate qualsiasi: viene scalata a fit (proporzioni mantenute)
    /// dentro l'area dell'icona.
    void setShape (juce::Path newShape);

    /// Glyph del design system (`ayra::glyphs::xxx`). Interfaccia unica: il Glyph
    /// astrae SVG/byte-array, quindi niente piu' overload raw `(data, numBytes)`.
    void setGlyph (const ayra::glyphs::Glyph& glyph);

    /// Padding dell'icona dentro il bottone (frazione del lato minore). Default 0.28.
    void setIconPadding (float fractionOfMinSide) noexcept;

    /// Mostra/nasconde lo sfondo "chip" reattivo. Default true.
    void setDrawBackground (bool shouldDraw) noexcept;

    /// Disegna SEMPRE un chip di sfondo, anche a riposo (off + no hover), usando
    /// `backgroundIdleColourId`. Serve quando il bottone deve leggersi come un
    /// "tasto" gia' a riposo (es. toolbar di flag dello Snapshot Editor) invece
    /// dello stile transport a icona nuda. Default false (icona nuda a riposo).
    void setShowIdleBackground (bool shouldShow) noexcept;

    /** Usa l'accento corrente anche per icona e chip a riposo. Gli stati
        down/on usano gia' l'accento corrente come default; questa opzione serve
        ai comandi la cui identita' e' l'accento stesso (per esempio `AddButton`).
        Gli override espliciti dei `ColourIds` continuano ad avere precedenza. */
    void setUseThemeAccentAtRest (bool shouldUse) noexcept;

    //==========================================================================
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    //==========================================================================
    juce::Path shape;
    float      iconPadding   = 0.28f;
    bool       drawBackground = true;
    bool       showIdleBackground = false;
    bool       useThemeAccentAtRest = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShapeIconButton)
};

//==============================================================================
// Toolbar di FLAG-TOGGLE in stile Node Inspector — helper condivisi.
// Generalizzano il pattern "riga di bottoni-icona identici dentro un well
// recessed" usato dal Node Inspector, cosi' mixer, pannello Cues e Inspector
// restano IDENTICI per costruzione (zero boilerplate, colori/glyph da fonte
// unica). Il chiamante possiede i bottoni e decide toggle/onClick; questi
// helper impostano solo aspetto e geometria.
//==============================================================================

/** Configura un `ShapeIconButton` come toggle di flag: glyph + colore ON
    (icona piena) + wash di sfondo ON (0.22). A riposo resta icona nuda: la
    superficie recessed la mette il well (drawFlagToolbarWell). NON imposta il
    toggle-mode: il chiamante sceglie `setClickingTogglesState` o pilota lo
    stato dal proprio modello (come fa il Node Inspector). */
void configureFlagToggle (ShapeIconButton& button, const glyphs::Glyph& glyph, juce::Colour onColour);

/** Relazione fra lo stato `juce::Button::getToggleState()` e l'operativita'.
    I controlli enable usano `onMeansOperational`; i flag documentali shutdown
    usano `onMeansShutdown`. La distinzione evita di invertire callback o stato
    persistente solo per ottenere la grafica power verde/rossa. */
enum class PowerTogglePolarity
{
    onMeansOperational,
    onMeansShutdown
};

/** Configura un power toggle a due stati: operativo verde, spento rosso. */
void configurePowerToggle (ShapeIconButton& button, PowerTogglePolarity polarity);

/** Aspetto neutro per una risorsa non disponibile: glifo grigio senza chip,
    wash o rim persistente. Il controllo resta cliccabile, cosi' un device
    desiderato ma disconnesso puo' comunque essere disabilitato. */
void configureUnavailablePowerToggle (ShapeIconButton& button);

/** Come sopra ma per un `NodeButton`: glyph e colore dalla fonte unica
    (`nodeButtonGlyph` / `nodeButtonActiveColour`). Un flag-nodo ha lo stesso
    aspetto ovunque. */
void configureNodeFlagToggle (ShapeIconButton& button, NodeButton kind);

/** Disegna il well recessed dietro una toolbar di flag-toggle (le icone ci
    scorrono sopra). Stessa superficie del Node Inspector. */
void drawFlagToolbarWell (juce::Graphics& g, juce::Rectangle<float> well);

/** Distribuisce uniformemente i bottoni nel `well`: celle uguali, ogni icona
    centrata e MAI piu' larga di (altezza cella + 10) -> niente pillole stirate.
    Ricetta identica al Node Inspector. */
void layoutFlagToolbar (juce::Rectangle<int> well, const juce::Array<ShapeIconButton*>& buttons);

} // namespace ayra
