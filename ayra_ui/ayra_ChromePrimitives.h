/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_ChromePrimitives.h
 * @brief Primitivi di disegno Chrome condivisi: shadow cache melatonin +
 *        ricette di superficie riusabili (raised/recessed/flat).
 *
 * # PROBLEMA
 * `melatonin::DropShadow` / `InnerShadow` cachano l'immagine sfocata DENTRO
 * l'istanza (chiave = path origin-agnostic + parametri). Se l'istanza e' una
 * variabile LOCALE creata ad ogni `paint`, la cache muore ogni frame -> il blur
 * (costoso) viene ricalcolato sempre. Era il collo di bottiglia del LAF.
 *
 * # SOLUZIONE
 * Teniamo le istanze VIVE in una mappa, keyed per geometria+parametri+tema:
 *   (w, h, corner, blur, offsetX, offsetY, colourARGB, inner, spread, mode)
 * Poiche' melatonin normalizza l'origine del path, due widget della stessa
 * dimensione ma posizione diversa CONDIVIDONO la stessa cache. Il primo render
 * calcola il blur, i successivi (stesso frame o frame futuri) sono blit.
 *
 * # USO
 * @code
 *   namespace tk = ayra::tokens;
 *   auto& ds = ayra::shadowCache().drop ({ w, h, corner }, colour, blur, {ox,oy});
 *   ds.render (g, path);   // render PRIMA del fill (raised)
 *
 *   auto& is = ayra::shadowCache().inner ({ w, h, corner }, colour, blur, {ox,oy});
 *   is.render (g, path);   // render DOPO il fill (recessed)
 * @endcode
 *
 * # CONTRATTO DEL RIFERIMENTO
 * Il riferimento ritornato da `drop()`/`inner()` e' valido solo fino alla
 * PROSSIMA chiamata sulla STESSA cache (drop->drops, inner->inners): al
 * raggiungimento del cap una entry viene evict-ata. Regola: renderizza subito,
 * non conservare il riferimento attraverso un'altra `drop()`/`inner()`. Tutti i
 * call-site usano la forma one-liner `shadowCache().drop(...).render(...)`, che
 * e' sempre sicura.
 *
 * # THREAD
 * Solo message thread (come tutto il LookAndFeel). Nessun lock.
 *
 * @note `<JuceHeader.h>`, `<melatonin_blur/melatonin_blur.h>`, `<unordered_map>`
 *       e `<memory>` sono inclusi dal file root del modulo (`ayra_common.h`).
 *       Dipende da `ayra_Tokens.h` per `tokens::detail::dark()`.
 */

namespace ayra
{

/// Dimensioni del rounded-rect che genera il path dell'ombra (per la chiave).
struct ShadowGeom
{
    int w = 0, h = 0, corner = 0;
};

/// Parametri di una drop-shadow dopo il fit al margine disponibile.
struct FittedShadow { int blur = 0; juce::Point<int> offset {}; int spread = 0; };

/** Riduce blur/offset/spread di una DROP-shadow cosi' che la sua estensione massima
    (blur + |offset| + spread) stia dentro `marginPx` px: la distanza tra il bordo
    della forma e i bounds del componente. Senza questo, una soft-shadow piu' grande
    del margine viene CLIPPATA dai bounds rettangolari del componente -> "scatola"
    netta dietro la forma arrotondata (artefatto "squadrato dietro / tondo davanti").
    Le INNER-shadow non ne hanno bisogno: rendono dentro il path arrotondato. */
/** Configura la geometria/typography di un TextEditor Ayra senza congelarne i
    colori. La palette resta interamente nel LookAndFeel host, quindi il cambio
    Light/Dark propaga correttamente anche agli editor gia' costruiti. */
inline void configureTextEditorChrome (
    juce::TextEditor& editor,
    juce::Font font = tokens::type::body(),
    juce::Justification justification = juce::Justification::centredLeft,
    int leftIndent = 12,
    int topIndent = 0)
{
    editor.setFont (font);
    editor.setJustification (justification);
    editor.setIndents (leftIndent, topIndent);
}

inline FittedShadow fitShadow (int blur, juce::Point<int> offset, int spread, float marginPx) noexcept
{
    // La coda gaussiana del blur si estende oltre il "raggio" nominale: contiamo
    // ~1.8x il blur perche' la soft-shadow svanisca davvero entro il margine
    // (altrimenti resta una coda visibile clippata in un rettangolo).
    const float reach = 1.8f * (float) blur
                      + (float) juce::jmax (std::abs (offset.x), std::abs (offset.y))
                      + (float) juce::jmax (0, spread);
    const float maxReach = juce::jmax (0.0f, marginPx);

    if (reach <= maxReach || reach <= 0.0f)
        return { blur, offset, spread };

    const float s = maxReach / reach;
    return { juce::jmax (0, (int) std::floor ((float) blur * s)),
             { (int) std::round ((float) offset.x * s), (int) std::round ((float) offset.y * s) },
             juce::jmax (0, (int) std::floor ((float) spread * s)) };
}

class ShadowCache
{
    /// Find-or-emplace con eviction di UNA entry, condiviso da drop()/inner().
    /// Backstop di memoria: si rimuove UNA sola entry, mai clear() totale.
    /// clear() distruggerebbe d'un colpo tutte le istanze (incluse quelle gia'
    /// usate in questo frame) -> cliff di re-blur. Rimuovendo un elemento DIVERSO
    /// da quello che stiamo per restituire, il riferimento ritornato (nodo appena
    /// emplace-ato) resta sempre valido. `makeNew` costruisce la shadow al miss.
    ///
    /// DEFINITO in testa alla classe (private di default) perche' ha return type
    /// dedotto (auto&) e drop()/inner() lo chiamano: un template con return type
    /// dedotto deve essere visibile (definito) prima del primo uso nella TU.
    template <typename Map, typename Factory>
    static auto& getOrCreate (Map& m, juce::uint64 key, Factory makeNew)
    {
        auto it = m.find (key);
        if (it == m.end())
        {
            if (m.size() >= kMaxEntries) { m.erase (m.begin()); }
            it = m.emplace (key, makeNew()).first;
        }
        return *it->second;
    }

public:
    /// DropShadow persistente per (geom, colore, blur, offset, spread).
    /// Inizializza i parametri al primo uso; melatonin invalida la propria
    /// cache interna solo se un valore cambia davvero.
    melatonin::DropShadow& drop (ShadowGeom g, juce::Colour colour, int blur,
                                 juce::Point<int> offset, int spread = 0)
    {
        const auto key = makeKey (g, colour, blur, offset, /*inner*/ false, spread);
        return getOrCreate (drops, key, [&]
        { return std::make_unique<melatonin::DropShadow> (colour, blur, offset, spread); });
    }

    /// InnerShadow persistente, stessa logica.
    melatonin::InnerShadow& inner (ShadowGeom g, juce::Colour colour, int blur,
                                   juce::Point<int> offset)
    {
        const auto key = makeKey (g, colour, blur, offset, /*inner*/ true);
        return getOrCreate (inners, key, [&]
        { return std::make_unique<melatonin::InnerShadow> (colour, blur, offset); });
    }

    /// Svuota la cache. Da chiamare al flip del tema (le entry del tema vecchio
    /// non verranno piu' riusate: il salt nel key le tiene vive in RAM per nulla)
    /// e ogni volta che si vuole forzare il rilascio delle bitmap sfocate.
    void clear() noexcept { drops.clear(); inners.clear(); }

private:
    /// Tetto morbido: se una mappa supera questo numero di entry distinte (es.
    /// zoom continuo dei nodi -> una DropShadow per ogni dimensione/corner), la
    /// svuotiamo. Bound O(1) sulla memoria; i frame successivi ripagano il blur.
    /// In UI a dimensioni fisse non scatta MAI.
    static constexpr size_t kMaxEntries = 512;

    static juce::uint64 makeKey (ShadowGeom g, juce::Colour c, int blur,
                                 juce::Point<int> off, bool isInner, int spread = 0) noexcept
    {
        // Hash compatto deterministico (FNV-1a). Includiamo la modalita' tema
        // come salt cosi' Light/Dark non si pestano quando un colore coincide.
        juce::uint64 k = 1469598103934665603ull; // FNV offset basis
        auto mix = [&k] (juce::uint64 v) noexcept { k = (k ^ v) * 1099511628211ull; };
        mix ((juce::uint64) (juce::uint32) g.w);
        mix ((juce::uint64) (juce::uint32) g.h);
        mix ((juce::uint64) (juce::uint32) g.corner);
        mix ((juce::uint64) (juce::uint32) blur);
        mix ((juce::uint64) (juce::uint32) (off.x & 0xffff) | ((juce::uint64) (off.y & 0xffff) << 16));
        mix ((juce::uint64) c.getARGB());
        mix ((juce::uint64) (juce::uint32) spread);
        mix ((juce::uint64) (isInner ? 1u : 0u));
        mix ((juce::uint64) (tokens::detail::dark() ? 2u : 1u));
        return k;
    }

    std::unordered_map<juce::uint64, std::unique_ptr<melatonin::DropShadow>>  drops;
    std::unordered_map<juce::uint64, std::unique_ptr<melatonin::InnerShadow>> inners;
};

/// Istanza condivisa (message-thread only).
inline ShadowCache& shadowCache()
{
    static ShadowCache instance;
    return instance;
}

//==============================================================================
/** Disegna un cerchio chrome in rilievo. Il pressed state sostituisce le ombre
    esterne con inner-shadow: e' la base canonica di knob e thumb circolari. */
inline void drawRaisedCircle (juce::Graphics& g, juce::Rectangle<float> bounds,
                              juce::Colour fill, bool pressed, int liftBoost)
{
    juce::Path circle;
    circle.addEllipse (bounds);

    const ShadowGeom geom { juce::roundToInt (bounds.getWidth()),
                            juce::roundToInt (bounds.getHeight()),
                            juce::roundToInt (bounds.getWidth() * 0.5f) };

    if (! pressed)
    {
        const int darkBlur = tokens::depth::raisedBlurDark + liftBoost;
        const int darkOff  = tokens::depth::raisedOffset + (liftBoost > 0 ? 1 : 0);

        shadowCache().drop (geom, tokens::shadowDark(), darkBlur,
                            { darkOff, darkOff }).render (g, circle);
        shadowCache().drop (geom, tokens::shadowLight(),
                            tokens::depth::raisedBlurLight,
                            { -tokens::depth::raisedOffset,
                              -tokens::depth::raisedOffset }).render (g, circle);
    }

    g.setColour (fill);
    g.fillPath (circle);

    if (pressed)
    {
        shadowCache().inner (geom, tokens::shadowRecessedIn(),
                             tokens::depth::recessedBlurIn,
                             { tokens::depth::recessedOffset,
                               tokens::depth::recessedOffset }).render (g, circle);
        shadowCache().inner (geom, tokens::shadowRecessedHi(),
                             tokens::depth::recessedBlurHi,
                             { -tokens::depth::recessedOffset,
                               -tokens::depth::recessedOffset }).render (g, circle);
    }

    tokens::drawCircleRims (g, bounds);
}

/** Disegna un path chrome incassato. Usato da well circolari e groove lineari
    per mantenere identica la profondita' visiva in ogni controllo. */
inline void fillDualRecessedPath (juce::Graphics& g, const juce::Path& shape,
                                  juce::Colour fill)
{
    g.setColour (fill);
    g.fillPath (shape);

    const auto bounds = shape.getBounds();
    const ShadowGeom geom {
        juce::roundToInt (bounds.getWidth()),
        juce::roundToInt (bounds.getHeight()),
        juce::roundToInt (juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f)
    };

    shadowCache().inner (geom, tokens::shadowRecessedIn(),
                         tokens::depth::recessedBlurIn,
                         { tokens::depth::recessedOffset,
                           tokens::depth::recessedOffset }).render (g, shape);
    shadowCache().inner (geom, tokens::shadowRecessedHi(),
                         tokens::depth::recessedBlurHi,
                         { -tokens::depth::recessedOffset,
                           -tokens::depth::recessedOffset }).render (g, shape);
}

/** Colori e opzioni della primitiva rotary. I default corrispondono al knob
    Project; i widget speciali possono cambiare i colori senza duplicarne la
    geometria o la ricetta chrome. */
struct RotaryKnobStyle
{
    juce::Colour wellColour    { tokens::surfaceRecessed() };
    juce::Colour trackColour   { tokens::borderHairline() };
    juce::Colour valueColour   { tokens::accent() };
    juce::Colour glowColour    { tokens::accentSoft() };
    juce::Colour pointerColour { tokens::accent() };
    bool drawPointer = true;
};

/** Primitiva grafica canonica di ogni rotary knob Ayra.

    `valueProportion` e `neutralProportion` sono normalizzati in [0, 1]. L'arco
    attivo parte dal neutro: 0 per controlli unipolari, 0.5 per i pan bipolari.
    Il caller resta owner di valore, interazione, label e readout. */
inline void drawRotaryKnob (juce::Graphics& g, juce::Rectangle<float> bounds,
                            float valueProportion, float rotaryStartAngle,
                            float rotaryEndAngle, float neutralProportion,
                            const RotaryKnobStyle& style, bool enabled,
                            bool highlighted, bool dragging)
{
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    if (diameter <= 10.0f) { return; }

    const bool isHighlighted = enabled && highlighted;
    const bool isDragging = enabled && dragging;

    bounds = bounds.withSizeKeepingCentre (diameter, diameter);
    const float radius = diameter * 0.5f;
    const auto centre = bounds.getCentre();
    const float wellRadius = radius * 0.92f;
    const auto wellBounds = juce::Rectangle<float> (wellRadius * 2.0f,
                                                     wellRadius * 2.0f)
                                .withCentre (centre);

    juce::Path wellPath;
    wellPath.addEllipse (wellBounds);
    fillDualRecessedPath (g, wellPath, style.wellColour);

    const float value = juce::jlimit (0.0f, 1.0f, valueProportion);
    const float neutral = juce::jlimit (0.0f, 1.0f, neutralProportion);
    const float span = rotaryEndAngle - rotaryStartAngle;
    const float valueAngle = rotaryStartAngle + value * span;
    const float neutralAngle = rotaryStartAngle + neutral * span;
    const float activeStart = juce::jmin (neutralAngle, valueAngle);
    const float activeEnd = juce::jmax (neutralAngle, valueAngle);
    const float arcRadius = juce::jmax (1.0f, wellRadius - 4.0f);
    constexpr float lineWidth = 3.0f;
    const juce::PathStrokeType stroke (lineWidth,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (enabled ? style.trackColour : style.trackColour.withAlpha (0.5f));
    g.strokePath (track, stroke);

    if (activeEnd - activeStart > 0.0001f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 activeStart, activeEnd, true);
        if (isDragging)
        {
            g.setColour (style.glowColour);
            g.strokePath (activeArc,
                          juce::PathStrokeType (lineWidth * 3.0f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        }

        g.setColour (enabled ? style.valueColour : tokens::textDisabled());
        g.strokePath (activeArc, stroke);
    }

    const float knobRadius = arcRadius * 0.62f;
    const auto knobBounds = juce::Rectangle<float> (knobRadius * 2.0f,
                                                     knobRadius * 2.0f)
                                .withCentre (centre);
    drawRaisedCircle (g, knobBounds, tokens::surfaceRaised(), isDragging,
                      isHighlighted ? tokens::depth::hoverBlurBoost : 0);

    if (style.drawPointer)
    {
        const float tickInner = knobRadius * 0.30f;
        const float tickOuter = knobRadius * 0.86f;
        const float dx = std::sin (valueAngle);
        const float dy = -std::cos (valueAngle);

        g.setColour (enabled ? style.pointerColour : tokens::textDisabled());
        g.drawLine ({ centre.x + dx * tickInner, centre.y + dy * tickInner,
                      centre.x + dx * tickOuter, centre.y + dy * tickOuter },
                    2.0f);
    }
}

//==============================================================================
/** RICETTA COERENTE per una superficie chrome RECESSED (well, track, gutter,
    group box): fill recessed + inner-shadow duale (dalla cache) + bordo hairline.

    E' il gemello di `tokens::fillRaisedSurface`: quello vive nei token perche' e'
    shadow-free (gradiente + bevel); QUESTO vive qui perche' richiede la shadow
    cache. Insieme sono i due mattoni del rilievo neumorfico — un solo punto di
    tuning per OGNI superficie incassata del design system.

    @param fill  base della superficie (default: tokens::surfaceRecessed()). */
inline void fillRecessedSurface (juce::Graphics& g, juce::Rectangle<float> area, float corner,
                                 juce::Colour fill = tokens::surfaceRecessed())
{
    juce::Path p;
    p.addRoundedRectangle (area, corner);
    g.setColour (fill);
    g.fillPath (p);

    const ShadowGeom geom { juce::roundToInt (area.getWidth()),
                            juce::roundToInt (area.getHeight()),
                            juce::roundToInt (corner) };
    shadowCache().inner (geom, tokens::shadowRecessedIn(), tokens::depth::recessedBlurIn,
                         { tokens::depth::recessedOffset, tokens::depth::recessedOffset }).render (g, p);

    // Hairline INSET di mezzo spessore: lo stroke e' centrato sul path, quindi se
    // disegnato sul bordo esatto di `area` la sua meta' esterna verrebbe clippata
    // ai bounds rettangolari del componente. Insettandolo resta tutto dentro.
    const auto ri = area.reduced (0.5f);
    juce::Path pi; pi.addRoundedRectangle (ri, juce::jmax (0.0f, corner - 0.5f));
    g.setColour (tokens::borderHairline());
    g.strokePath (pi, juce::PathStrokeType (1.0f));
}


//==============================================================================
// BACKGROUND PRIMITIVES — tre superfici riusabili, argomento SOLO il rettangolo.
//
// Sono i mattoni condivisi che le ricette LAF (`drawAppWindowHeader`,
// `drawSidebarNavItem`, `drawSplitPanelBackground`) e i Component generici
// chiamano per garantire coerenza visiva senza duplicare logica di
// colore/ombra/corner in tutto il sistema.
//
//  fillFlatBackground → gradiente verticale + hairline bottom, niente angoli.
//  fillDarkCard       → dark recessed + inner-shadow + hairline, radius::card.
//  fillLightCard      → light raised  + dual-rim,               radius::card.
//==============================================================================

/** @brief Superficie piatta (strip, toolbar, header): gradiente verticale
    (surfaceBaseTop → surfaceBaseBot) senza angoli arrotondati + hairline crisp
    in basso.

    Nessun arrotondamento: la strip copre tipicamente l'intera larghezza del
    pannello host e i suoi bordi sono tagliati dal clip dei bounds superiori.

    @param area  Bounds della strip (già in coordinate locali). */
inline void fillFlatBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    tokens::fillWindowBackground (g, area);
    g.setColour (tokens::borderHairline());
    g.fillRect (area.getX(), area.getBottom() - 1.0f, area.getWidth(), 1.0f);
}

/** @brief Card incassata scura: fill `surfaceRecessed` + inner-shadow duale
    dalla cache + hairline, angoli `radius::card`.

    Gemello di `fillLightCard`: stesso corner, look scuro/incassato.
    Corrisponde visivamente a un well o una superficie nav recessed.

    @param area  Bounds della card (già ridotti dal caller per il padding). */
inline void fillDarkCard (juce::Graphics& g, juce::Rectangle<float> area)
{
    fillRecessedSurface (g, area, tokens::radius::card);
}

/** @brief Card raised chiara: gradiente sottile + dual-rim, angoli `radius::card`.

    Gemello di `fillDarkCard`: stesso corner, look chiaro/in rilievo.
    Corrisponde visivamente a un pannello di contenuto o una sezione raised.

    @param area  Bounds della card (già ridotti dal caller per il padding). */
inline void fillLightCard (juce::Graphics& g, juce::Rectangle<float> area)
{
    tokens::fillRaisedSurface (g, area, tokens::radius::card, tokens::surfaceRaised());
}

//==============================================================================
//  Chevron / caret crisp — sorgente UNICA del design system
//------------------------------------------------------------------------------
// Caret a due segmenti (">" / "v"), pensato per `strokePath` sottile (NON
// `fillPath`): coerente con il chevron del ComboBox e dei picker. `arm` = mezza
// larghezza del segno. Sostituisce i triangoli pieni "pesanti" disegnati a mano.
//==============================================================================

/** Chevron verso destra (stato compresso). Vertice a destra, centrato in `area`. */
[[nodiscard]] inline juce::Path makeChevronRight (juce::Rectangle<float> area, float arm) noexcept
{
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const auto half = arm * 0.5f;

    juce::Path p;
    p.startNewSubPath (cx - half, cy - arm);
    p.lineTo          (cx + half, cy);
    p.lineTo          (cx - half, cy + arm);
    return p;
}

/** Chevron verso il basso (stato espanso). Vertice in basso, centrato in `area`. */
[[nodiscard]] inline juce::Path makeChevronDown (juce::Rectangle<float> area, float arm) noexcept
{
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const auto half = arm * 0.5f;

    juce::Path p;
    p.startNewSubPath (cx - arm, cy - half);
    p.lineTo          (cx,       cy + half);
    p.lineTo          (cx + arm, cy - half);
    return p;
}

/** @brief Disegna un chevron di disclosure (espandi/comprimi) crisp e leggero.

    Look coerente con il ComboBox: stroke arrotondato sottile, NON un triangolo
    pieno. `expanded` orienta il segno (giu' = aperto, destra = chiuso).

    @param g         Contesto grafico.
    @param area      Box in cui centrare il segno (di norma ~10-12px).
    @param expanded  true -> chevron giu'; false -> chevron a destra.
    @param colour    Tinta del tratto.
    @param strokeW   Spessore del tratto (default 1.6f, come i picker). */
inline void drawDisclosureChevron (juce::Graphics& g, juce::Rectangle<float> area,
                                   bool expanded, juce::Colour colour, float strokeW = 1.6f)
{
    // makeChevron* centra il segno sul centro di `area` usando `arm`: la
    // dimensione di area conta solo per il centro, nessun box intermedio serve.
    constexpr float arm = 3.0f;
    const auto p = expanded ? makeChevronDown (area, arm) : makeChevronRight (area, arm);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

/** @brief Banda-testata IN CIMA a un contenitore arrotondato: fill clippato al body
    path, cosi' gli angoli SUPERIORI ereditano ESATTAMENTE la curva del contenitore
    (concentrici, nessun raggio proprio da accordare) e il bordo inferiore taglia dritto
    con una hairline di separazione.

    Perche' esiste: riempire la testata con un rounded-rect pieno a raggio proprio (es.
    fillRecessedSurface a radius::button dentro un body a radius::card) crea due difetti
    visivi:
      1) gli angoli usano un raggio diverso dal contenitore -> le curvature "non
         combaciano" (archi non concentrici ai vertici);
      2) arrotonda ANCHE gli angoli inferiori, che stanno a meta' del corpo -> la banda
         sembra una pill che galleggia dentro il contenitore.
    Clippando al body ed emettendo un semplice rect, gli angoli alti sono per costruzione
    quelli del contenitore e il basso e' un taglio netto: un solo raggio in gioco.

    Va chiamata DOPO il fill del corpo e PRIMA dello stroke del bordo, cosi' il bordo del
    contenitore resta crisp SOPRA la banda.

    @param g        Contesto grafico.
    @param band     Bounds della banda (top del contenitore).
    @param bodyClip Path arrotondato del contenitore (lo stesso usato per fill/stroke).
    @param fill     Colore di riempimento della banda.
    @param divider  true -> hairline di separazione sul bordo inferiore (default true). */
inline void fillHeaderBand (juce::Graphics& g, juce::Rectangle<float> band,
                            const juce::Path& bodyClip, juce::Colour fill, bool divider = true)
{
    const juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (bodyClip);
    g.setColour (fill);
    g.fillRect (band);
    if (divider)
    {
        g.setColour (tokens::borderHairline());
        g.fillRect (band.getX(), band.getBottom() - 1.0f, band.getWidth(), 1.0f);
    }
}

/** @brief Spine accent verticale sul bordo SINISTRO di un contenitore arrotondato, ad
    ALTEZZA PIENA e CLIPPATA al suo body path.

    I gruppi (PerformerGroup del mixer, MedleyGroup del setlist) codificano il colore del
    gruppo con una spine colorata a sinistra che corre da CIMA a FONDO. Clippata al body:
    gli angoli alto-sx e basso-sx seguono ESATTAMENTE la stessa curva del contenitore
    (identici, nessun mismatch), il tratto centrale e' dritto. Una barra "nuda"
    sborderebbe oltre gli angoli tondi; una spine solo-testata si fermerebbe a meta'
    lasciando il corner basso-sx "vuoto". E' lo stesso trattamento della stripe delle
    song row del setlist, esteso a tutto il contenitore del gruppo.

    Nota layout: la spine vive nel gutter sinistro del contenitore (i figli sono inset di
    almeno il padding di gruppo), quindi non viene coperta dal contenuto.

    Uso:
    @code
    juce::Path body; body.addRoundedRectangle (area.reduced (0.75f), corner);
    // ... fillPath(body), fillHeaderBand(...), strokePath(body) ...
    ayra::drawAccentSpine (g, area, body, groupColour);   // area = bounds pieni del gruppo
    @endcode

    @param g         Contesto grafico.
    @param container Bounds PIENI del contenitore (la spine ne copre l'altezza).
    @param bodyClip  Path arrotondato del contenitore a cui clippare.
    @param colour    Tinta piena del gruppo/performer.
    @param width     Larghezza della spine (default 4px). */
inline void drawAccentSpine (juce::Graphics& g, juce::Rectangle<float> container,
                             const juce::Path& bodyClip, juce::Colour colour,
                             float width = 4.0f)
{
    const juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (bodyClip);
    g.setColour (colour);
    g.fillRect (container.getX(), container.getY(), width, container.getHeight());
}

} // namespace ayra
