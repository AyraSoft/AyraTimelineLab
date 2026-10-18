/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_CommonLookAndFeelMethods.h
 * @brief INTERFACCIA (pura virtuale) dell'API di disegno bespoke di ayra_common.
 *
 * # COSA E'
 * Questa e' l'API che TUTTI i widget di ayra_common chiamano per disegnarsi:
 * un set di ricette `drawXxx(...)` riusabili (chrome / node graph / mixer /
 * session / inspector / form). Le finestre reali E i mockup condividono
 * ESATTAMENTE la stessa grammatica visiva -> niente duplicazione di ricette.
 *
 * Legge unica: "soft is the bed, crisp is the signal".
 *
 * # PATTERN MULTI-MODULO (CRITICO)
 * `CommonLookAndFeelMethods` **NON** eredita da `juce::LookAndFeel_V4`. E'
 * intenzionale: permette di combinare i LookAndFeel di piu' moduli Ayra senza
 * diamond inheritance. Solo l'APP finale eredita `LookAndFeel_V4`:
 *
 * @code
 * class MultiverseLookAndFeel : public juce::LookAndFeel_V4,         // l'UNICO LookAndFeel_V4
 *                               public ayra::CommonLookAndFeelMethods, // ricette del grafo/tensor
 *                               public ayra::GuiLookAndFeelMethods,    // widget ayra_gui
 *                               public ayra::PluginsManagerLookAndFeelMethods
 * {
 *     // override solo cio' che vuoi; il resto eredita dal Default::getDefaultInstance().
 * };
 * @endcode
 *
 * # FUORI SCOPE (vive nell'APP, NON qui)
 * Gli override JUCE `LookAndFeel_V4` (drawRotarySlider / drawButtonBackground /
 * drawComboBox / drawPopupMenu*, ...) NON appartengono a questo modulo: sono
 * stile d'app, vivono nella LAF dell'app. Qui c'e' SOLO l'API bespoke.
 *
 * # DUE PALETTE
 *   - Chrome / widget generici -> `tokens::streamColour(StreamType)`.
 *   - Primitive interne al canvas del grafo -> `tokens::nodeColour(StreamType)`.
 * Vedi raggruppamento "NODE GRAPH PRIMITIVES" piu' sotto: quelle ricette si
 * aspettano un tint preso da `nodeColour()`.
 *
 * @note `<JuceHeader.h>` e `ayra_Tokens.h` sono inclusi dal file root del
 *       modulo (`ayra_common.h`) prima di questo header.
 */

namespace ayra
{

//==============================================================================
/** Stato di una clip / snapshot launcher (session view). */
enum class ClipState { Empty, Stopped, Playing, Recording };

/** Tipo di bottone di trasporto. */
enum class TransportKind { Rewind, Play, Stop, Record };

//==============================================================================
/** @brief Stato visivo di una CARD nodo, passato per reference a `drawNodeCard`.

    Struct aggregato (pattern Ayra "struct = dato puro passato in giro", come
    `MultiStreamContext`): aggiungere un nuovo attributo visivo NON cambia la
    firma della ricetta ne' alcun call-site esistente. Il canvas lo riempie a
    partire dal `GraphDocument` (+ provider metriche opzionale); il LookAndFeel
    lo legge per disegnare la card 1:1 con il `NodeComponent` di ayra_performer.

    Convenzione 1-based lato presentazione: `idText` e' un'etichetta gia' pronta
    (il canvas la deriva dallo uuid), la ricetta non la interpreta. */
struct NodeCardVisual
{
    juce::Rectangle<float> bounds;        ///< FOOTPRINT del nodo (card + margini per pin/bottoni/cpu/logo).
    juce::String  title;                  ///< nome del nodo (centrato, bold).
    juce::String  subtitle;               ///< manufacturer / "Sub-Tensor" (sotto il nome).
    juce::String  idText;                 ///< "ID" + identificativo (in basso a dx, come il reale).
    juce::Colour  tint  { juce::Colours::green };
    bool          selected  { false };    ///< ring bianco esterno.
    bool          suspended { false };    ///< bypass/shutdown -> card scurita (darker.darker).
    bool          hovered   { false };    ///< HUD corner-brackets + variante decori al passaggio del mouse.
    bool          pressed   { false };    ///< mouse down -> HUD brackets piu' interni (reduced 24 vs 16).
    float         cpuLevel01 { -1.0f };   ///< CPU 0..1 (meter in basso a sx); < 0 = niente meter.
    float         scale      { 1.0f };    ///< zoom del canvas: scala margini/font/raggi/bottoni (1 = nativo).

    //--- CONSOLE (bottoni + decori attorno alla card, 1:1 col NodeComponent) ----
    // Capacita' del nodo: determinano QUALI bottoni/decori appaiono (come il reale).
    bool isInputNode   { false };         ///< nodo input del grafo: niente bypass/buffer/oversample/latency/remove.
    bool isOutputNode  { false };         ///< nodo output del grafo: niente bypass/buffer/oversample/remove.
    bool acceptsAudio  { true };          ///< buffer/oversample solo se audio in/out.
    bool producesAudio { true };
    bool acceptsMidi   { false };         ///< bottone panic + linea midi solo se accetta MIDI.
    bool hasEditor     { false };         ///< bottone "window" solo se il plugin ha editor.
    bool hasParams     { false };         ///< bottone "Parameters" (allParams) se ha param vivi o program change.
    bool canRemove     { true };          ///< bottone "remove" (cestino) accanto al titolo.
    // Stati toggle (colore acceso del bottone quando attivo).
    bool bypassed   { false };
    bool shutdown   { false };
    bool latencyComp{ false };
    bool bufferized { false };
    bool oversampled{ false };
    bool windowOpen { false };
    // Logo formato (in basso a dx) + readout latenza (in alto). Vuoti = default/skip.
    juce::String formatLabel;             ///< "VST3"/"AudioUnit"/"VST"/"LV2"/"" (interno -> logo Ayra).
    juce::String latencyText;             ///< es. "> 0 > + [ 64 ] = { 64 }"; vuoto = niente readout.
    bool compact { false };                ///< sola superficie canonica, senza console/testi.
};

/** @brief Stato visivo di un PIN, passato per reference a `drawNodePin`.

    Stesso pattern aggregato. `connected` distingue un pin libero (disegna il
    "buco" scuro interno, come il PinComponent reale) da uno collegato; `level01`
    porta il VU per-pin (audio) se alimentato da uno snapshot; `audio`/`isInput`
    orientano l'offset del buco lungo il bordo giusto. */
struct NodePinVisual
{
    juce::Point<float> centre;
    juce::Colour  tint  { juce::Colours::green };
    bool          isInput   { false };
    bool          audio     { true };     ///< audio = bordo top/bottom; dati = left/right.
    bool          connected { false };    ///< pin libero -> "buco" scuro; collegato -> pieno.
    float         level01   { -1.0f };    ///< VU 0..1 sul pin; < 0 = niente meter.
    float         scale     { 1.0f };     ///< zoom del canvas: scala diametro/buco/VU (1 = nativo).
};

//==============================================================================
/** @brief Interfaccia aggregata dell'API di disegno bespoke di ayra_common.

    Ogni metodo e' una ricetta riusabile. I widget la chiamano via il proprio
    `getLAF()` (dynamic_cast + fallback su `CommonDefaultLookAndFeel`). L'app
    puo' override-are qualsiasi ricetta ereditando questa interfaccia.

    @see CommonDefaultLookAndFeel — l'implementazione di default.
*/
struct CommonLookAndFeelMethods
{
    virtual ~CommonLookAndFeelMethods() = default;

    /// Alias di comodita': il tipo di stream multi-canale (definito nei token).
    using StreamType = tokens::StreamType;

    //==========================================================================
    // ACCENT PER-ISTANZA (track colour) — vedi doc dettagliata in
    // CommonDefaultLookAndFeel. Vive QUI sull'interfaccia (non sull'impl di
    // default) perche' i call-site lo raggiungono via `getCommonLAF(comp)`, che
    // ritorna `CommonLookAndFeelMethods&` -> cosi' anche un painter "nativo" (es.
    // NodeComponent::paint, che non passa per le ricette drawXxx) puo' leggere
    // l'accent con `getCommonLAF(*this).accentOr(fallback)`.
    //
    // Default inattivo -> accentOr(fallback) ritorna SEMPRE il fallback (zero
    // regressione). Il colore va passato GIA' tone-mappato (tokens::toReadableAccent)
    // dall'app. Message thread only (lo setta l'editor, lo legge il paint).
    //==========================================================================

    /// Attiva l'accent per-istanza (track colour, gia' tone-mappato).
    void setInstanceAccent (juce::Colour accent) noexcept { instanceAccent = accent; instanceAccentActive = true; }

    /// Disattiva l'accent per-istanza: si torna ai colori di default del tema.
    void clearInstanceAccent() noexcept { instanceAccentActive = false; }

    /// L'accent per-istanza se attivo, altrimenti il `fallback` del tema (es.
    /// tokens::accent(), tokens::focusRing(), o juce::Colours::white per i ring).
    [[nodiscard]] juce::Colour accentOr (juce::Colour fallback) const noexcept
    {
        return instanceAccentActive ? instanceAccent : fallback;
    }

    /// Come `accentOr`, ma per i LETTI/WASH SOFT (es. tokens::accentSoft(), o un
    /// accent a bassa alpha). `accentOr` ritornerebbe l'accent d'istanza PIENO
    /// (opaco), facendo collassare il fill soft sul colore della barra/bordo crisp
    /// accanto -> si perde "soft is the bed, crisp is the signal". Qui invece, quando
    /// l'accent d'istanza e' attivo, lo ritorniamo con la STESSA alpha del
    /// `fallbackSoft`, cosi' il fill resta chiaro/translucido, solo tinto col track
    /// colour. Inattivo -> `fallbackSoft` invariato (zero behaviour change).
    [[nodiscard]] juce::Colour accentSoftOr (juce::Colour fallbackSoft) const noexcept
    {
        return instanceAccentActive ? instanceAccent.withAlpha (fallbackSoft.getFloatAlpha())
                                     : fallbackSoft;
    }

private:
    juce::Colour instanceAccent;
    bool         instanceAccentActive = false;

public:

    //==========================================================================
    // CHROME — fondo app, pannelli, glass strip, header, chip, logo badge.
    //==========================================================================

    /// Fondo finestra: gradiente verticale + dot-grid 28px (hairline tenue).
    virtual void drawAppBackground (juce::Graphics&, juce::Rectangle<int> bounds) = 0;

    /// Pannello raised (gradiente + dual rim, via fillRaisedSurface) o recessed
    /// (surface incassata + inner shadow + bordo hairline, via fillRecessedSurface).
    /// Nessuna drop-shadow esterna: il pannello puo' essere un Component a se' e la
    /// drop verrebbe clippata ai bounds rettangolari (piastra squadrata).
    virtual void drawAppPanel (juce::Graphics&, juce::Rectangle<float> bounds,
                               bool raised, float corner) = 0;

    /// Striscia di vetro (toolbar/tab strip): gradiente base + hairline su un lato.
    virtual void drawAppGlassStrip (juce::Graphics&, juce::Rectangle<float>,
                                    bool hairlineBottom) = 0;

    /// Header di sezione: testo secondary uppercase + hairline sotto.
    virtual void drawAppSectionHeader (juce::Graphics&, juce::Rectangle<int>,
                                       const juce::String& text) = 0;

    /// Chip icona (toggle): fill accentSoft/raised + bordo accent/hairline.
    virtual void drawAppIconChip (juce::Graphics&, juce::Rectangle<float>, bool active) = 0;

    /// Logo badge "robottino glossy": cerchio bianco + due occhi ambra.
    virtual void drawAppLogoBadge (juce::Graphics&, juce::Rectangle<float>) = 0;

    /// Header di finestra (strip + glyph accent + titolo):
    /// usa `fillFlatBackground` (gradiente, niente angoli) come fondo, poi disegna
    /// `iconData`/`iconLen` in accent a sinistra e `title` con font title a destra.
    /// Sostituisce la tripletta `drawAppGlassStrip` + drawGlyph + drawText che ogni
    /// host componeva manualmente.
    virtual void drawAppWindowHeader (juce::Graphics&, juce::Rectangle<float> bounds,
                                      const unsigned char* iconData, size_t iconLen,
                                      const juce::String& title) = 0;

    /// Voce di navigazione sidebar (pill hover/selected + accent bar + icon + label):
    /// usa `fillDarkCard` (dark recessed, bordi arrotondati) come background
    /// dello stato selected/hover, garantendo coerenza con il pannello sidebar.
    /// selected = pill accentSoft + barra accent + bold; hover = pill raised dim.
    virtual void drawSidebarNavItem (juce::Graphics&, juce::Rectangle<float> bounds,
                                     const unsigned char* iconData, size_t iconLen,
                                     const juce::String& label,
                                     bool selected, bool hover) = 0;

    //--- Overload UNICI lato chiamante: prendono un `glyphs::Glyph` e inoltrano alle
    //    virtual byte-based qui sopra (che restano l'hook overridabile dell'app).
    //    Cosi' i call-site usano una sola forma e non maneggiano mai raw bytes/SVG.
    void drawAppWindowHeader (juce::Graphics& g, juce::Rectangle<float> bounds,
                              const glyphs::Glyph& icon, const juce::String& title)
    {
        const auto b = icon.binary();
        drawAppWindowHeader (g, bounds, b.data, b.size, title);
    }

    void drawSidebarNavItem (juce::Graphics& g, juce::Rectangle<float> bounds,
                             const glyphs::Glyph& icon, const juce::String& label,
                             bool selected, bool hover)
    {
        const auto b = icon.binary();
        drawSidebarNavItem (g, bounds, b.data, b.size, label, selected, hover);
    }

    /// Background del layout split-panel (sidebar sinistra + content destra):
    /// usa `fillDarkCard` per la sidebar e `fillLightCard` (light raised,
    /// bordi arrotondati) per l'area di contenuto. L'header va sopra, separatamente.
    /// @param headerHeight   Pixel da saltare in cima (strip gi\u00e0 disegnata dall'host).
    /// @param sidebarWidth   Larghezza della colonna sidebar in pixel logici.
    virtual void drawSplitPanelBackground (juce::Graphics&, juce::Rectangle<int> bounds,
                                           int headerHeight, int sidebarWidth) = 0;

    // NODE GRAPH PRIMITIVES (DARK COSMIC) — il canvas del grafo chiama QUESTE.
    //  Tint atteso da tokens::nodeColour(StreamType) (palette satura su nero),
    //  NON da streamColour(). Riproducono 1:1 NodeComponent/PinComponent/
    //  ConnectorComponent di ayra_performer.
    //==========================================================================

    /// Card nodo: ombra dura offset (shadowReactor), fill obliquo + vignette
    /// radiale verso outerVoid, bordo categoria, ring bianco se selezionato,
    /// titolo + sottotitolo + "ID", scurita se sospesa, HUD corner-brackets in
    /// hover, arco CPU in basso a dx se alimentato. corner 16. Vedi NodeCardVisual.
    virtual void drawNodeCard (juce::Graphics&, const NodeCardVisual&) = 0;

    /// Pin: cerchio pieno colore stream + ring scuro 2px (shadowReactor), "buco"
    /// interno se non connesso, VU per-pin se alimentato. Vedi NodePinVisual.
    virtual void drawNodePin (juce::Graphics&, const NodePinVisual&) = 0;

    /// Cavo: bezier verticale (audio) / orizzontale (dati). Stroke 2.5;
    /// tratteggio {8,4,2,4} se selezionato (dashed).
    virtual void drawConnectionCable (juce::Graphics&, juce::Point<float> from,
                                      juce::Point<float> to, juce::Colour tint,
                                      bool dashed, bool verticalFlow) = 0;

    //==========================================================================
    // MIXER — channel strip, colour cap, fader, VU meter, macro knob.
    //==========================================================================

    /// Fondo channel strip (delega a un pannello raised con corner ridotto).
    virtual void drawChannelStripBackground (juce::Graphics&, juce::Rectangle<float>) = 0;

    /// Cap colorato di canale (rounded rect tinta + nome bianco centrato).
    virtual void drawColourCap (juce::Graphics&, juce::Rectangle<float>,
                                juce::Colour tint, const juce::String& name) = 0;

    /// Fader verticale: track recessed + thumb raised con tacca tinta.
    virtual void drawFader (juce::Graphics&, juce::Rectangle<float> area,
                            float pos01, juce::Colour tint) = 0;

    /// VU meter verticale: well recessed + fill gradiente verde->ambra->rosso.
    virtual void drawVMeter (juce::Graphics&, juce::Rectangle<float>, float level) = 0;

    /// Macro knob: well recessed circolare + arco accent (uni/bipolare) + tacca.
    virtual void drawMacroKnob (juce::Graphics&, juce::Rectangle<float>,
                                float value, bool bipolar) = 0;

    //==========================================================================
    // SESSION / TRANSPORT / TIMELINE — clip cell, transport button, timeline lane/clip.
    //==========================================================================

    /// Cella clip della session view: empty ("+" tenue) / stopped (triangolo) /
    /// playing (stop glyph) / recording (cerchio rosso + glow).
    virtual void drawClipCell (juce::Graphics&, juce::Rectangle<float>, juce::Colour tint,
                               ClipState, const juce::String& name) = 0;

    /// Bottone di trasporto (icona chip + glifo play/stop/record/rewind).
    virtual void drawTransportButton (juce::Graphics&, juce::Rectangle<float>,
                                      TransportKind, bool active) = 0;

    /// Timeline: disegno corsia (spine, watermark, background highlight, hairline).
    virtual void drawTimelineLane (juce::Graphics&, const TimelineLaneVisual&) = 0;

    /** Curva canonica di clip e lane continue: geometria, punti e handle restano
        nel widget; il modulo disegna il riempimento richiesto e la linea
        TimelineLab (alone scuro + tratto crisp). */
    virtual void drawTimelineAutomationCurveUnderlay (juce::Graphics&,
                                                      const juce::Path& line,
                                                      juce::Rectangle<float> bounds,
                                                      TimelineCurveUnderlayStyle) = 0;
    virtual void drawTimelineAutomationCurveStroke (juce::Graphics&,
                                                    const juce::Path& line,
                                                    juce::Colour lineColour) = 0;

    /** Letto canonico delle lane point/span keyframe. */
    virtual void drawTimelineKeyframeLaneBackground (juce::Graphics&,
                                                     juce::Rectangle<float> bounds) = 0;

    /** Evento point/span canonico: tacca temporale + pillola/range glass nello
        stile Counter/Marker adottato da Multiverse. */
    virtual void drawTimelineKeyframeEvent (juce::Graphics&,
                                            juce::Rectangle<float> laneBounds,
                                            juce::Rectangle<float> eventBounds,
                                            const juce::String& label,
                                            juce::Colour colour,
                                            bool selected,
                                            bool hovered,
                                            bool suspended) = 0;

    //==========================================================================
    // INSPECTOR / SNAPSHOT / CPU — gauge CPU, snapshot launcher, node list item,
    //  curve editor. (riusabili da finestre reali + mockup).
    //==========================================================================

    /// Gauge CPU circolare (track + arco valore verde->ambra->rosso + "NN%").
    virtual void drawCpuArc (juce::Graphics&, juce::Rectangle<float>, float level01,
                             const juce::String& caption) = 0;

    /// Bottone-lancio snapshot sci-fi: anello + glifo shutdown + arco countdown
    /// (ringProgress 0..1 = pending; < 0 nessun arco). Stato empty/idle/active/rec.
    virtual void drawSnapshotLaunchButton (juce::Graphics&, juce::Rectangle<float>,
                                           juce::Colour tint, ClipState,
                                           float ringProgress) = 0;

    /// Card nodo ORIZZONTALE stile NodesInspector (~200x48): gradiente radiale dal
    /// colore nodo, dial segmentato rotante attorno all'indicatore colore, HUD
    /// corner-brackets se selected/hover, separatore in alto, nome+uid.
    /// @note Hybrid: contenitore chrome ma tint = nodeColour() (coerenza canvas).
    virtual void drawNodeListItem (juce::Graphics&, juce::Rectangle<float>,
                                   const juce::String& name, const juce::String& uid,
                                   juce::Colour tint, bool selected, bool hover) = 0;

    /// Editor di curva bezier (transizione/remapping): well + griglia + curva +
    /// control point ambra. mode: 0 = linear, 1 = single CP, 2 = two CP.
    virtual void drawCurveEditor (juce::Graphics&, juce::Rectangle<float>,
                                  int mode, juce::Colour tint) = 0;

    //==========================================================================
    // FORM / BROWSER — switch, badge, search field, param slider, plugin card.
    //==========================================================================

    /// Toggle switch a pillola: track accent/recessed + knob bianco scorrevole.
    /// `accentColour` trasparente -> usa l'accent del tema; altrimenti tinge il
    /// track ON col colore dato (es. colore-nodo nel NodesInspector).
    virtual void drawSwitch (juce::Graphics&, juce::Rectangle<float>, bool on,
                             juce::Colour accentColour,
                             juce::Colour offColour = {}) = 0;

    /// Badge a pillola: fill tinta soft + bordo + testo tinta.
    virtual void drawBadge (juce::Graphics&, juce::Rectangle<float>,
                            const juce::String& text, juce::Colour tint) = 0;

    /// Campo di ricerca: pannello recessed pill + lente + placeholder.
    virtual void drawSearchField (juce::Graphics&, juce::Rectangle<float>,
                                  const juce::String& placeholder) = 0;

    /// Riga parametro stile LinearBar: well recessed + fill tinta + nome a sx,
    /// valore (SF Mono) a dx.
    virtual void drawParamSlider (juce::Graphics&, juce::Rectangle<float>, float value01,
                                  const juce::String& name, const juce::String& valueText,
                                  juce::Colour tint) = 0;

    /// Card plugin del browser: raised + rail tinta in alto + nome/vendor +
    /// badge formato (VST3/AU) + badge tipo. Tint = streamColour(kind) (chrome).
    virtual void drawPluginCard (juce::Graphics&, juce::Rectangle<float>,
                                 const juce::String& name, const juce::String& vendor,
                                 const juce::String& fmt, StreamType kind, bool selected) = 0;
};

} // namespace ayra
