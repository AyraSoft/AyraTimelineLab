/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_Tokens_Colours.h
 * @brief Token di COLORE del design system di ayra_common — due mondi, due palette.
 *
 * # I DUE MONDI (regola LOCKED, vedi _doc/DESIGN_LANGUAGE.md)
 *  - **Frosted Lab, Daylight** = il *chrome* e i widget generici (pannelli,
 *    slider, meter, popup, inspector, ...). Colori "daylight" tuned per fondo
 *    chiaro: superfici, accent ambra `#FF7A1A`, e i 7 `streamColour()`.
 *  - **Dark Cosmic** = i nodi e il canvas del grafo. Colori saturi su nero: la
 *    `NodePalette` ESATTA di ayra_performer, esposta qui come `nodeColour()`.
 *
 * Lo stesso *significato* di stream ha quindi DUE registri cromatici:
 *  `streamColour(StreamType)` (chrome) vs `nodeColour(StreamType)` (grafo).
 *  Il widget di chrome usa il primo; le primitive del canvas il secondo.
 *
 * # FILOSOFIA (tre layer, mai tutti e tre sullo stesso widget)
 *  - **Glass (frosted)** = il chrome: pannelli, popup, toolbar, tab strip.
 *  - **Neumorphism (soft emboss)** = SOLO container e track passivi: grooves,
 *    well dei meter, gutter dei toggle, group box recessed.
 *  - **Bitwig technicality** = il *valore attivo*: fill accent saturi, rim 1.5px
 *    colorati, numerici monospace. E' questo a portare il contrasto.
 *
 * REGOLA ANTI-WCAG-FAIL: "soft is the bed, crisp is the signal". Ogni elemento
 * interattivo ha SEMPRE un componente crisp sopra il letto soft (rim 1.5px /
 * fill saturo / glifo bold) -> affordance >= 3:1, testo >= 4.5:1. Il neumorphism
 * porta solo la PROFONDITA', mai il contrasto.
 *
 * Tutti i valori (hex, alpha) vengono dalla spec di design approvata. I colori
 * sono funzioni inline (juce::Colour non e' constexpr-friendly).
 *
 * @note Questo file NON va incluso direttamente: usa il facade
 *       `ayra_Tokens.h`.
 */

namespace ayra::tokens
{

//==============================================================================
/** Tipo di stream multi-canale dell'ecosistema Ayra.

    Identifica il dominio di un pin / cavo / categoria di nodo. Usato come chiave
    per le due funzioni-ponte `streamColour()` (chrome) e `nodeColour()` (grafo).

    @note Niente entry "Instrument"/"Utility": quelle categorie del NodePalette
          non sono *stream* ma ruoli di nodo, fuori dallo scope del colour token.
*/
enum class StreamType
{
    audio,   ///< Audio PCM (per-canale fisico mono).
    midi,    ///< Eventi MIDI.
    video,   ///< Frame video / sequenze immagini (alias "image").
    param,   ///< Parametri / CV.
    dmx,     ///< DMX512.
    osc,     ///< Open Sound Control.
    string   ///< Testo sample-accurate (StringMessage).
};

/// Numero di valori di StreamType (i 7 domini). Usato per dimensionare le tabelle
/// di override colore. Tenere allineato all'enum.
inline constexpr int numStreamTypes = 7;

/// Nome ASCII canonico del tipo di stream (chiave stabile per serializzazione
/// degli override colore). ASCII puro (regola CLAUDE.md sui letterali juce::String).
inline juce::String streamTypeName (StreamType s) noexcept
{
    switch (s)
    {
        case StreamType::audio:  return "audio";
        case StreamType::midi:   return "midi";
        case StreamType::video:  return "video";
        case StreamType::param:  return "param";
        case StreamType::dmx:    return "dmx";
        case StreamType::osc:    return "osc";
        case StreamType::string: return "string";
    }
    return "audio";
}

/// Inverso di streamTypeName: ritorna true e popola `out` se `name` e' valido.
[[nodiscard]] inline bool streamTypeFromName (const juce::String& name, StreamType& out) noexcept
{
    for (int i = 0; i < numStreamTypes; ++i)
    {
        const auto s = (StreamType) i;
        if (streamTypeName (s) == name) { out = s; return true; }
    }
    return false;
}

//==============================================================================
// THEME MODE — flip Light/Dark a runtime.
//  - Light = "Frosted Lab, Daylight".
//  - Dark  = fedele a CleanMyMac (fondo scuro, glass, glow, ombre diffuse) (default).
// Il colore corretto e' scelto da ogni funzione token via pick().
// DEFAULT: Dark (decisione di prodotto, allineata a AppearanceSettings::Defaults).
//==============================================================================
enum class Mode { Light, Dark };

inline Mode& themeMode() noexcept { static Mode m = Mode::Dark; return m; }
inline void  setThemeMode (Mode m) noexcept { themeMode() = m; }

namespace detail { inline bool dark() noexcept { return themeMode() == Mode::Dark; } }

/// Sceglie il colore in base al tema corrente (Light vs Dark).
inline juce::Colour pick (juce::uint32 lightArgb, juce::uint32 darkArgb) noexcept
{
    return juce::Colour (detail::dark() ? darkArgb : lightArgb);
}

/** Risoluzione cromatica canonica dei widget Ayra.
    Precedenza: override esplicito sul Component -> ColourId registrato nel
    LookAndFeel host -> fallback fornito dal widget (normalmente un token live).
    Centralizzare questa regola evita palette congelate nei costruttori e
    implementazioni duplicate sparse nei controlli. */
template <typename ComponentType>
[[nodiscard]] inline juce::Colour componentColour (const ComponentType& component,
                                                    int colourId,
                                                    juce::Colour fallback)
{
    if (component.isColourSpecified (colourId))
        return component.findColour (colourId);

    auto& laf = component.getLookAndFeel();
    return laf.isColourSpecified (colourId) ? laf.findColour (colourId)
                                            : fallback;
}

/** Testo leggibile sopra una tinta semantica arbitraria (performer, stream,
    macro, action). Evita la duplicazione di soglie brightness e soprattutto
    l'assunzione "testo attivo = sempre bianco". */
[[nodiscard]] inline juce::Colour textOnTint (juce::Colour tint,
                                              float alpha = 1.0f) noexcept
{
    return tint.getPerceivedBrightness() > 0.60f
             ? juce::Colours::black.withAlpha (0.88f * alpha)
             : juce::Colours::white.withAlpha (0.96f * alpha);
}

//==============================================================================
// SURFACE (chrome — Frosted Lab)
//==============================================================================
inline juce::Colour surfaceBase()      { return pick (0xffE7ECF2, 0xff1B1F26); }
inline juce::Colour surfaceBaseTop()   { return pick (0xffEEF1F5, 0xff20252E); } // gradient top
inline juce::Colour surfaceBaseBot()   { return pick (0xffE2E7EE, 0xff161A20); } // gradient bottom
inline juce::Colour surfaceRaised()    { return pick (0xffF4F7FB, 0xff272D37); }
inline juce::Colour surfaceRecessed()  { return pick (0xffDDE3EB, 0xff12151A); }
inline juce::Colour surfaceGlass()     { return detail::dark() ? juce::Colour (0xff2A313C).withAlpha (0.78f)
                                                               : juce::Colour (0xffF7FAFE).withAlpha (0.62f); }
inline juce::Colour surfaceGlassEdge() { return juce::Colours::white.withAlpha (detail::dark() ? 0.12f : 0.55f); }

//==============================================================================
// NEUMORPHIC SHADOWS (dual: dark bottom-right + light top-left)
// Colori usati con melatonin::DropShadow / InnerShadow (vedi ayra_ChromePrimitives.h).
//==============================================================================
inline juce::Colour shadowDark()       { return detail::dark() ? juce::Colours::black.withAlpha (0.55f)
                                                               : juce::Colour (0xffB6C0CE).withAlpha (0.55f); }
inline juce::Colour shadowLight()      { return juce::Colours::white.withAlpha (detail::dark() ? 0.06f : 0.90f); }
inline juce::Colour shadowRecessedIn() { return detail::dark() ? juce::Colours::black.withAlpha (0.55f)
                                                               : juce::Colour (0xffAEB9C7).withAlpha (0.50f); }
inline juce::Colour shadowRecessedHi() { return juce::Colours::white.withAlpha (detail::dark() ? 0.05f : 0.85f); }

//==============================================================================
// RIM / BORDER
//==============================================================================
inline juce::Colour rimLight()       { return juce::Colours::white.withAlpha (detail::dark() ? 0.10f : 0.70f); }
inline juce::Colour rimDark()        { return detail::dark() ? juce::Colours::black.withAlpha (0.45f)
                                                             : juce::Colour (0xffC4CDD9).withAlpha (0.60f); }
inline juce::Colour borderHairline() { return pick (0xffC9D2DD, 0xff3A434F); }

//==============================================================================
// TEXT
//==============================================================================
inline juce::Colour textPrimary()   { return pick (0xff1C2530, 0xffEDF1F7); }
inline juce::Colour textSecondary() { return pick (0xff5A6675, 0xff9AA6B5); }
inline juce::Colour textDisabled()  { return pick (0xffA4AEBB, 0xff5C6675); }
inline juce::Colour textValue()     { return pick (0xff11324A, 0xffBFE0FF); } // numerici (SF Mono)

//==============================================================================
// ACCENT (brand AMBRA)
//==============================================================================
inline juce::Colour accent()        { return juce::Colour (0xffFF7A1A); }
inline juce::Colour accentHover()   { return juce::Colour (0xffFF8C1A); }
inline juce::Colour accentPress()   { return juce::Colour (0xffE86A0C); }
inline juce::Colour accentSoft()    { return juce::Colour (0xffFF7A1A).withAlpha (detail::dark() ? 0.22f : 0.14f); }
inline juce::Colour focusRing()     { return juce::Colour (0xffFF8C1A).withAlpha (detail::dark() ? 0.95f : 0.85f); }
/// Secondo stop del gradiente accent (CTA/hero) — vira al corallo per il "glow" CleanMyMac.
inline juce::Colour accentGradient2() { return juce::Colour (0xffFF5E3A); }

/** Colori semantici DAW per i controlli Mute/Solo. Sono token distinti dagli
    stream anche quando condividono lo stesso ARGB: il significato resta stabile
    se in futuro cambia una delle due palette. */
inline juce::Colour muteIndicator() { return juce::Colour (0xffE8B400); }
inline juce::Colour soloIndicator() { return juce::Colour (0xffF03C3C); }
/// Stato GO/PLAY nel chrome (transport, action "Stop & Play").
inline juce::Colour playIndicator() { return juce::Colour (0xff4FB56E); }

/** Identita' cromatiche dei tre ruoli fissi del Project output path.

    Sono token di ruolo, non alias degli stream: Click, Counter e Main Out
    devono restare coerenti fra Project Strip e Project Graph anche se una
    palette stream viene personalizzata o cambia indipendentemente. */
inline juce::Colour projectGuidanceClickColour()   { return juce::Colour (0xffFF7A1A); }
inline juce::Colour projectGuidanceCounterColour() { return juce::Colour (0xff19B6E6); }
inline juce::Colour projectMainOutputColour()      { return juce::Colour (0xff16C95C); }

/** Identita' neutra del gruppo SONG TRACKS.

    Le singole tracce mantengono il proprio colore di tipo, mentre il colore
    della Song appartiene al container Song e a Song Main. Questo token evita
    che Mixer e Project Graph definiscano localmente due policy divergenti. */
inline juce::Colour songTracksGroupColour() { return textSecondary(); }

/** Colori semantici dei controlli POWER/ENABLE.

    Non sono alias concettuali dei colori stream: un controllo operativo resta
    verde e uno spento resta rosso anche se in futuro cambia la palette di
    audio/MIDI. `powerUnavailableIndicator` identifica invece una risorsa
    richiesta ma non disponibile (per esempio un device disconnesso): non deve
    sembrare ne' operativa ne' esplicitamente spenta. */
inline juce::Colour powerOnIndicator()          { return juce::Colour (0xff16C95C); }
inline juce::Colour powerOffIndicator()         { return juce::Colour (0xffF03C3C); }
inline juce::Colour powerUnavailableIndicator() { return textSecondary(); }
inline juce::Colour powerBackground (juce::Colour stateColour) { return stateColour.withAlpha (0.22f); }
inline juce::Colour powerOutline    (juce::Colour stateColour) { return stateColour.withAlpha (0.60f); }

/** Colore identita' deterministico per entita' senza tinta esplicita (performer,
    medley, gruppi). Una sola policy condivisa evita divergenze fra footer, mixer,
    timeline, cues e finestre di gestione. `String::hashCode` e' deterministico;
    l'operazione e' O(n) sul nome e non va usata nel callback audio. */
[[nodiscard]] inline juce::Colour stableIdentityColour (
    const juce::String& name) noexcept
{
    if (name.isEmpty()) { return accent(); }
    const auto hueDegrees = static_cast<juce::uint32> (name.hashCode()) % 360u;
    return juce::Colour::fromHSV (static_cast<float> (hueDegrees) / 360.0f,
                                  0.55f, 0.85f, 1.0f);
}

#if AYRA_COMMON_WITH_MULTIVERSE

/** Risolve il colore identita' GUI di un Performer dal suo owner canonico.

    Il nodo participation di una Song non e' una seconda fonte di verita': una
    tinta esplicita vive esclusivamente nel `<PERFORMER>` globale; in sua
    assenza tutte le viste condividono lo stesso fallback deterministico. */
[[nodiscard]] inline juce::Colour performerIdentityColour (
    const ayra::Performer& performer) noexcept
{
    if (! performer.isValid())
        return stableIdentityColour (juce::String());
    return performer.hasColour() ? performer.getColour()
                                 : stableIdentityColour (performer.getName());
}

/** Risolve il colore identita' GUI di una Song. L'override vive soltanto sul
    `<SONG>`; Song Main e ogni altra vista consumano questo stesso resolver. */
[[nodiscard]] inline juce::Colour songIdentityColour (
    const ayra::Song& song) noexcept
{
    if (! song.isValid())
        return stableIdentityColour (juce::String());
    return song.hasColour() ? song.getColour()
                            : stableIdentityColour (song.getName());
}

/** Risolve il colore identita' GUI di un Medley dal nodo canonico `<MEDLEY>`.
    L'assenza dell'override resta derivata dal nome e non viene materializzata
    nel documento. */
[[nodiscard]] inline juce::Colour medleyIdentityColour (
    const ayra::Setlist& setlist, const juce::Uuid medleyUuid)
{
    if (! setlist.isValid())
        return stableIdentityColour (juce::String());
    return setlist.hasMedleyColour (medleyUuid)
             ? setlist.getMedleyColour (medleyUuid)
             : stableIdentityColour (setlist.getMedleyName (medleyUuid));
}

#endif

/// Tone-mapping di un colore ARBITRARIO (es. il colore-traccia che la DAW passa via
/// `updateTrackProperties`, o quello scelto in standalone) in un ACCENT leggibile sul
/// tema corrente. Un colore host puo' essere troppo scuro / troppo chiaro / troppo
/// desaturato per fare da accent: lo riportiamo in una finestra di saturazione e
/// luminosita' usabile PRESERVANDO la tinta (hue), cosi' resta riconoscibile come
/// "il colore di QUELLA traccia" senza rompere il contrasto del chrome.
///   - Dark : S in [0.50, 0.95], B in [0.62, 1.00] (deve risaltare sul fondo scuro).
///   - Light: S in [0.50, 0.95], B in [0.42, 0.78] (un accent troppo chiaro sparirebbe).
/// Idempotente nel range; message thread (e' GUI). NB: NON e' per-istanza di per se' —
/// e' una funzione pura; l'identita' per-istanza la porta `CommonDefaultLookAndFeel`.
[[nodiscard]] inline juce::Colour toReadableAccent (juce::Colour raw) noexcept
{
    const float b = detail::dark() ? juce::jlimit (0.62f, 1.00f, raw.getBrightness())   // risalta sul fondo scuro
                                   : juce::jlimit (0.42f, 0.78f, raw.getBrightness());   // non sbiadisce su fondo chiaro
    // Caso NEUTRO: un colore acromatico (grigio/bianco/nero) ha hue indefinito -> JUCE
    // lo riporta a 0 (= rosso). Forzare la saturazione lo trasformerebbe in un rosso
    // spurio: invece lo teniamo neutro (accent grigio leggibile, nessuna tinta inventata).
    if (raw.getSaturation() < 0.05f)
        return juce::Colour::fromHSV (0.0f, 0.0f, b, 1.0f);
    const float s = juce::jlimit (0.50f, 0.95f, raw.getSaturation());   // identico nei due temi
    return juce::Colour::fromHSV (raw.getHue(), s, b, 1.0f);
}

//==============================================================================
// SEMANTIC — stream tuned per fondo chiaro (CHROME / widget generici).
//  NB: NON sono i colori del grafo. Sono la versione "daylight" per badge,
//  cap di canale, clip cell, ecc. Il grafo usa nodeColour() (piu' saturo).
//==============================================================================
inline juce::Colour streamAudio()  { return juce::Colour (0xff16C95C); }
inline juce::Colour streamMidi()   { return juce::Colour (0xffF03C3C); }
/// Azione distruttiva nel chrome. Oggi condivide il rosso MIDI, ma resta un
/// token semantico indipendente: cambiare la palette MIDI non deve cambiare
/// automaticamente pulsanti Remove/Delete.
inline juce::Colour destructiveAction() { return juce::Colour (0xffF03C3C); }
inline juce::Colour streamVideo()  { return juce::Colour (0xff19B6E6); }
inline juce::Colour streamParam()  { return juce::Colour (0xffE8B400); }
inline juce::Colour streamDmx()    { return juce::Colour (0xffA84FD6); }
inline juce::Colour streamOsc()    { return juce::Colour (0xff4664E6); }
inline juce::Colour streamString() { return juce::Colour (0xffFF8C1A); }

/// Colore "daylight" per il tipo di stream (CHROME). Dispatch O(1) su StreamType.
inline juce::Colour streamColour (StreamType s) noexcept
{
    switch (s)
    {
        case StreamType::audio:  return streamAudio();
        case StreamType::midi:   return streamMidi();
        case StreamType::video:  return streamVideo();
        case StreamType::param:  return streamParam();
        case StreamType::dmx:    return streamDmx();
        case StreamType::osc:    return streamOsc();
        case StreamType::string: return streamString();
    }
    return accent();
}

/** Colore semantico di una Track priva di override esplicito.

    `trackKind` usa il vocabolario persistente di `Track::getTrackKind()`;
    la conversione vive qui affinche' Mixer e menu Settings consumino lo stesso
    fallback. I kind audio, mixed o sconosciuti ricadono su audio. */
[[nodiscard]] inline juce::Colour trackKindIdentityColour (
    const juce::String& trackKind) noexcept
{
    if (trackKind == "midi")   { return streamColour (StreamType::midi); }
    if (trackKind == "dmx")    { return streamColour (StreamType::dmx); }
    if (trackKind == "osc")    { return streamColour (StreamType::osc); }
    if (trackKind == "image")  { return streamColour (StreamType::video); }
    if (trackKind == "param")  { return streamColour (StreamType::param); }
    if (trackKind == "string") { return streamColour (StreamType::string); }
    return streamColour (StreamType::audio);
}

//==============================================================================
// NODE PALETTE — la palette ESATTA del node graph (NodePalette di ayra_performer,
// headers/ayra_ColorPalette.h). Riprodotta 1:1 per preservare la grafica dei nodi.
// Usata SOLO dalle primitive interne al canvas (drawNodeCard/drawNodePin/
// drawConnectionCable/drawNodeListItem). NON usare nel chrome.
//==============================================================================
namespace node
{
    // Base cosmica.
    inline juce::Colour outerVoid()     { return juce::Colour::fromRGB (12, 12, 12); }   // nero profondo (vignette/fondo)
    inline juce::Colour innerAbyss()    { return juce::Colour::fromRGB (26, 36, 36); }   // verde-scuro freddo
    inline juce::Colour shadowReactor() { return juce::Colour::fromRGB (18, 24, 24); }   // grigio quasi nero (ombre/ring pin)
    inline juce::Colour neonSurge()     { return juce::Colour::fromRGB (70, 255, 220); } // turchese neon (meter "safe")
    inline juce::Colour quantumFlare()  { return juce::Colour::fromRGB (255, 20, 60); }  // rosso vivido (meter "danger"/clip)
    inline juce::Colour plasmaGlow()    { return juce::Colour::fromRGB (255, 102, 102); }// rosso chiaro
    inline juce::Colour quantumPurple() { return juce::Colour::fromRGB (130, 50, 160); } // viola profondo
    inline juce::Colour chromeAlloy()   { return juce::Colour::fromRGB (150, 180, 190); }// blu-grigio
    inline juce::Colour title()         { return juce::Colours::white; }                 // testo nodo (bianco)

    // Colori per stream (pin/cavi/categoria nodo) — saturi su nero.
    inline juce::Colour audio()  { return juce::Colour::fromRGB (0, 255, 100); }  // verde brillante
    inline juce::Colour midi()   { return juce::Colour::fromRGB (255, 60, 60); }  // rosso saturo
    inline juce::Colour video()  { return juce::Colour::fromRGB (0, 200, 255); }  // ciano
    inline juce::Colour param()  { return juce::Colour::fromRGB (255, 220, 0); }  // giallo caldo
    inline juce::Colour dmx()    { return juce::Colour::fromRGB (180, 80, 220); } // viola
    inline juce::Colour osc()    { return juce::Colour::fromRGB (70, 100, 255); } // blu
    inline juce::Colour string() { return juce::Colour::fromRGB (255, 140, 0); }  // arancio
}

/// Colore di FABBRICA della NodePalette per il tipo di stream (GRAFO). Dispatch O(1).
/// E' il default immutabile: `nodeColour()` lo usa quando non c'e' override utente.
inline juce::Colour nodeColourDefault (StreamType s) noexcept
{
    switch (s)
    {
        case StreamType::audio:  return node::audio();
        case StreamType::midi:   return node::midi();
        case StreamType::video:  return node::video();
        case StreamType::param:  return node::param();
        case StreamType::dmx:    return node::dmx();
        case StreamType::osc:    return node::osc();
        case StreamType::string: return node::string();
    }
    return node::audio();
}

//==============================================================================
// NODE PALETTE OVERRIDE — l'utente puo' ridefinire il colore di ogni tipo di nodo
// dal pannello impostazioni (vedi ayra::AppearanceSettings). Lo store e'
// PROCESS-GLOBAL come `themeMode()`: una sola palette per processo, cosi' le
// istanze Tensor embedded in Multiverse seguono automaticamente lo stesso schema.
// Solo message thread (scritto dal pannello, letto dal canvas in paint()).
//==============================================================================
namespace detail
{
    struct NodePaletteOverride
    {
        std::array<juce::uint32, (size_t) numStreamTypes> argb {};   // valido solo se set[i]
        std::array<bool,         (size_t) numStreamTypes> set  {};   // false = usa il default
    };

    inline NodePaletteOverride& nodePaletteOverride() noexcept
    {
        static NodePaletteOverride o;   // zero-init: nessun override all'avvio
        return o;
    }
}

/// Colore EFFETTIVO della NodePalette per il tipo di stream (GRAFO): override
/// utente se presente, altrimenti il default di fabbrica. Dispatch O(1).
inline juce::Colour nodeColour (StreamType s) noexcept
{
    const auto i = (size_t) s;
    auto& o = detail::nodePaletteOverride();
    if (i < o.set.size() && o.set[i]) { return juce::Colour (o.argb[i]); }
    return nodeColourDefault (s);
}

/// Imposta l'override del colore per un tipo di stream (message thread).
inline void setNodeColourOverride (StreamType s, juce::Colour c) noexcept
{
    const auto i = (size_t) s;
    auto& o = detail::nodePaletteOverride();
    if (i >= o.set.size()) { return; }
    o.argb[i] = c.getARGB();
    o.set[i]  = true;
}

/// Rimuove l'override per un tipo di stream: torna al default di fabbrica.
inline void clearNodeColourOverride (StreamType s) noexcept
{
    const auto i = (size_t) s;
    auto& o = detail::nodePaletteOverride();
    if (i < o.set.size()) { o.set[i] = false; }
}

/// Azzera TUTTI gli override: l'intera palette torna ai default di fabbrica.
inline void clearAllNodeColourOverrides() noexcept
{
    detail::nodePaletteOverride() = detail::NodePaletteOverride {};
}

/// Vero se il tipo di stream ha un override attivo.
[[nodiscard]] inline bool hasNodeColourOverride (StreamType s) noexcept
{
    const auto i = (size_t) s;
    auto& o = detail::nodePaletteOverride();
    return i < o.set.size() && o.set[i];
}

} // namespace ayra::tokens
