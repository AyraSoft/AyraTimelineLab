/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / strip_parts
   ----------------------------------------------
   Parti CONDIVISE delle channel strip dell'ecosistema (Mixer, pannello Cues).

   ## Perche' esiste
   La channel strip del Mixer definisce il linguaggio visivo "canonico" di
   fader/pan/toggle dell'app. Quando il pannello Cues ha dovuto replicare le
   stesse strip, i disegni vivevano come membri privati di MixerPanel.cpp:
   questo file li promuove a primitive riusabili cosi' OGNI vista a strip
   condivide gli stessi pixel (niente doppie implementazioni che divergono).

   ## Cosa contiene
   - `StripToggleButton`  toggle a pillola (lettera / power IEC / glyph) usato
                          per M/S/Bypass/Buffer/Oversample nel mixer e per
                          enable/latency + stream nei return e nei cue.
   - `StripKnobGeometry`  corpo circolare + fascia readout condivisi da Panpot,
                          tempo e volume click.
   - `drawPanKnob`        knob bipolare del pan (arco dal centro, tacca ore 12,
                          pointer, label L/R + readout). SEMPRE circolare:
                          inscrive un quadrato centrato nell'area ricevuta.
   - `drawStripFaderScale` scala dB "da console" ai lati del track del fader
                          (tacche major/minor della legge condivisa ayra_FaderLaw,
                          0 dB in accent, label unsigned stile hardware).

   ## Come si usa (snippet)
   @code
   // paint() di una strip verticale:
   ayra::drawPanKnob (g, panArea.toFloat(), panValue);          // -1..+1
   ayra::drawStripFaderScale (g, faderArea);                    // PRIMA del fader
   getCommonLAF (*this).drawFader (g, faderArea.toFloat(), faderDbToPos (db), tint);
   @endcode

   ## Invarianti
   - `StripFaderGeometry` e' la geometria unica di thumb, scala e trascinamento.
     Il LAF e l'input controller la consumano; non ne replicano le costanti.
   - Solo message thread (painting/GUI).

   NOTA amalgama: zero #include (dottrina moduli Ayra).

  ==============================================================================
*/

namespace ayra
{

//==============================================================================
/** Toggle compatto a pillola delle channel strip (canonico dell'ecosistema).

    Tre modalita' di disegno:
    - LETTERA ("M"/"S"/"P"...): convenzione DAW per mute/solo e stati sintetici.
    - POWER: simbolo IEC disegnato a mano (arco + barra), per enable/bypass.
    - GLYPH del design system (stack, thunderCircle, midi, dmx, ...).

    A riposo: pillola recessed. ON: wash pieno del colore di stato (testo nero).
    Ghost (`setGhostActive`): wash tenue "attivo implicito" senza toggle reale
    (es. mute indotto dal solo di un'altra traccia). Hover/down: velo bianco.

    Il layout e' a carico del chiamante: il bottone riempie i bounds che riceve
    (nel mixer: griglia 2 righe; nei return/cue: celle a dimensione FISSA, mai
    stretchate). */
class StripToggleButton final : public juce::Button
{
public:
    StripToggleButton() : juce::Button ({}) { setClickingTogglesState (true); }

    void setLetter (juce::String l, juce::Colour onCol)
    {
        label = std::move (l); onColour = onCol; mode = Mode::letter; repaint();
    }
    void setPower (juce::Colour onCol)
    {
        onColour = onCol; mode = Mode::power; repaint();
    }
    void setGlyph (const glyphs::Glyph& glyph, juce::Colour onCol)
    {
        glyphPath = glyph.path(); onColour = onCol; mode = Mode::glyph; repaint();
    }

    /// "Attivo implicito": disegna un wash TENUE del colore di stato SENZA che il
    /// bottone sia realmente toggled (es. il mute di una traccia silenziata da un
    /// solo altrove). Distinto dal wash pieno dell'on esplicito.
    void setGhostActive (bool shouldGhost) { if (ghostOn != shouldGhost) { ghostOn = shouldGhost; repaint(); } }
    [[nodiscard]] bool isGhostActive() const noexcept { return ghostOn; }

    std::function<void (bool)> onToggled;
    void clicked() override { if (onToggled) { onToggled (getToggleState()); } }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        const float corner = juce::jmin (5.0f, area.getHeight() * 0.30f);

        fillRecessedSurface (g, area, corner);
        if (on)
        {
            g.setColour (onColour.withAlpha (0.92f));
            g.fillRoundedRectangle (area, corner);
        }
        else if (ghostOn)
        {
            g.setColour (onColour.withAlpha (0.32f));
            g.fillRoundedRectangle (area, corner);
        }
        else if (highlighted || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.10f : 0.05f));
            g.fillRoundedRectangle (area, corner);
        }

        const auto fg = on      ? juce::Colours::black.withAlpha (0.88f)
                      : ghostOn ? tokens::textPrimary()
                                : tokens::textSecondary();
        g.setColour (fg);

        switch (mode)
        {
            case Mode::power:
            {
                const auto c = area.getCentre();
                const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.26f;
                juce::Path arc;   // arco IEC con gap in alto + barra verticale
                arc.addCentredArc (c.x, c.y + r * 0.12f, r, r, 0.0f,
                                   juce::degreesToRadians (42.0f), juce::degreesToRadians (318.0f), true);
                g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawLine (c.x, c.y - r * 0.85f, c.x, c.y + r * 0.18f, 1.6f);
                break;
            }
            case Mode::glyph:
            {
                if (glyphPath.isEmpty()) { break; }
                auto box = area.reduced (juce::jmin (area.getWidth(), area.getHeight()) * 0.28f);
                auto p = glyphPath;
                p.applyTransform (p.getTransformToScaleToFit (box, true, juce::Justification::centred));
                g.fillPath (p);
                break;
            }
            case Mode::letter:
            default:
            {
                g.setFont (tokens::type::section().withHeight (juce::jmin (13.0f, area.getHeight() * 0.66f)).boldened());
                g.drawText (label, getLocalBounds(), juce::Justification::centred, false);
                break;
            }
        }
    }

private:
    enum class Mode { letter, power, glyph };

    juce::String label;
    juce::Path   glyphPath;
    juce::Colour onColour { tokens::accent() };
    Mode         mode = Mode::letter;
    bool         ghostOn = false;   ///< wash tenue "attivo implicito" (vedi setGhostActive)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripToggleButton)
};

//==============================================================================
/** Geometria condivisa dei knob compatti in stile channel strip.

    Riserva una fascia inferiore al readout e inscrive il corpo rotary in un
    quadrato centrato nell'area restante. Panpot, tempo e volume click devono
    consumare questa primitive: nessun consumer replica dimensioni o centraggi. */
struct StripKnobGeometry
{
    static constexpr float readoutHeight = 9.0f;

    [[nodiscard]] static juce::Rectangle<float> bodyBounds (
        juce::Rectangle<float> area) noexcept
    {
        area.removeFromBottom (readoutHeight);
        const float dimension = juce::jmin (area.getWidth(), area.getHeight());
        return juce::Rectangle<float> (dimension, dimension)
            .withCentre (area.getCentre());
    }

    [[nodiscard]] static juce::Rectangle<float> readoutBounds (
        juce::Rectangle<float> area) noexcept
    {
        return area.removeFromBottom (readoutHeight);
    }
};

//==============================================================================
/** Knob bipolare del pan, canonico delle channel strip.

    Disegna nel rettangolo `area`: corpo circolare raised + doppio rim, track
    recessed a 270 gradi, arco attivo DAL CENTRO (ore 12) verso il valore,
    tacca centrale, pointer + hub, label L/R e readout (C / L## / R##) nella
    fascia inferiore (9px riservati).

    SEMPRE circolare: il knob inscrive un quadrato centrato in cio' che resta
    dell'area (mai un'ellisse -- il bug "panpot ovale" nasceva da chi passava
    rettangoli non quadrati a un draw che non inscriveva).

    @param panValue    -1 (full L) .. +1 (full R); 0 = centro.
    @param highlighted true durante hover/focus visivo.
    @param dragging    true durante il gesto: il corpo appare premuto. */
void drawPanKnob (juce::Graphics& g, juce::Rectangle<float> area,
                  float panValue, bool highlighted = false,
                  bool dragging = false, const juce::String& readout = {},
                  const juce::String& startLabel = "L", const juce::String& endLabel = "R");

/** Rettangolo del READOUT del valore di pan (C / L## / R##) dentro `area`: la
    porzione centrale della fascia inferiore (9px) del knob, cioe' dove
    `drawPanKnob` disegna il numero. Fonte unica della geometria per l'hit-test e
    il posizionamento dell'editor del doppio-click (input testuale del pan),
    cosi' il campo cade ESATTAMENTE sul valore disegnato. */
[[nodiscard]] juce::Rectangle<int> panReadoutRect (juce::Rectangle<float> area) noexcept;

/** Formatta un pan (-1..+1) come il readout del knob: "C" / "L##" / "R##"
    (## = percentuale 0..100). Testo iniziale del campo di edit. */
[[nodiscard]] juce::String panValueToText (float panValue);

/** Parsa il testo di un readout di pan in un valore -1..+1. Accetta "C"/"c",
    "L##"/"R##" (percentuale 0..100), o un numero con segno (|n|<=1 = -1..+1,
    altrimenti percentuale -100..+100; negativo = L, positivo = R). Ritorna false
    se non interpretabile (input vuoto/garbage): il chiamante non deve scrivere.
    @param outPan  clampato a [-1,+1] in uscita. */
[[nodiscard]] bool parsePanText (const juce::String& text, float& outPan) noexcept;

//==============================================================================
/** Chip di collapse/espansione dei gruppi (performer nel mixer, gruppi-sorgente
    nel pannello Cues). Va posizionato ALLA DESTRA del nome del gruppo.

    Pillola recessed quadrata (inscritta nell'area) con rim hairline e chevron
    stroke: punta a DESTRA quando ripiegato ("apri, dispiega verso destra"), a
    SINISTRA quando aperto ("richiudi verso il nome") -- coerente col layout
    orizzontale dei gruppi. `highlighted` = hover: wash accent tenue + chevron
    primario. L'hit-test resta del chiamante (usa la stessa area passata qui). */
void drawCollapseChip (juce::Graphics& g, juce::Rectangle<float> area,
                       bool collapsed, bool highlighted = false);

//==============================================================================
/** Readout dB canonico sotto il fader di una strip (mixer + pannello Cues).

    Chip recessed compatto con il valore centrato (type::value 10pt): via di
    mezzo fra il vecchio readout "grande" del mixer e quello minuscolo dei cue.
    `text` gia' formattato dal chiamante ("-12.3", "-inf", "--", testo del
    parametro...): il chip non decide la precisione. `alpha` = dim della strip. */
void drawDbChip (juce::Graphics& g, juce::Rectangle<int> area,
                 const juce::String& text, float alpha = 1.0f);

//==============================================================================
enum class StripOrientation { vertical, horizontal };

/** Geometria di input canonica del panpot condiviso. Il gesto combina i due
    assi, raggiunge l'intera corsa con un drag compatto e aggancia esattamente
    il centro, evitando valori residui prossimi a zero. */
struct StripPanGeometry
{
    static constexpr float fullScaleDragDistance = 64.0f;
    static constexpr float centreSnap = 0.045f;

    [[nodiscard]] static float draggedValue (float start,
                                              juce::Point<float> delta) noexcept
    {
        const float value = juce::jlimit (-1.0f, 1.0f, start
            + (delta.x - delta.y) / fullScaleDragDistance);
        return std::abs (value) <= centreSnap ? 0.0f : value;
    }
};

/** Geometria canonica del fader, in coordinate locali logiche (prima del DPI).
    Disegno, tacche e input usano la stessa corsa del CENTRO del thumb.
    Un drag relativo conserva il punto di presa; non salta al click. */
struct StripFaderGeometry
{
    juce::Rectangle<float> bounds;
    StripOrientation orientation = StripOrientation::vertical;
    static constexpr float endInset = 4.0f;
    static constexpr float thumbLength = 18.0f;
    static constexpr float thumbWidth = 32.0f;
    static constexpr float unitySnap = 0.018f;

    [[nodiscard]] float travel() const noexcept
    {
        return juce::jmax (0.0f, (orientation == StripOrientation::vertical
            ? bounds.getHeight() : bounds.getWidth()) - 2.0f * endInset - thumbLength);
    }
    [[nodiscard]] float centre (float position) const noexcept
    {
        const bool vertical = orientation == StripOrientation::vertical;
        return (vertical ? bounds.getY() : bounds.getX()) + endInset + thumbLength * 0.5f
            + (vertical ? 1.0f - position : position) * travel();
    }
    [[nodiscard]] float draggedPosition (float start, juce::Point<float> delta) const noexcept
    {
        if (travel() <= 0.0f) { return start; }
        const float pos = juce::jlimit (0.0f, 1.0f, start
            + (orientation == StripOrientation::vertical ? -delta.y : delta.x) / travel());
        return std::abs (pos - kFaderUnityPos) <= unitySnap ? kFaderUnityPos : pos;
    }
};

/** Tacca di una scala fader non-dB. `position` e' normalizzata 0..1 sulla
    stessa corsa del centro del thumb; una label vuota identifica una tacca
    minor. `emphasized` usa l'accent del tema (per esempio 100% = unity). */
struct StripFaderScaleMark
{
    float position = 0.0f;
    juce::String label;
    bool emphasized = false;
};

/** Scala dB "da console" ai lati del track di un fader.

    Taglietti simmetrici major/minor secondo la legge condivisa (ayra_FaderLaw)
    + etichette unsigned stile hardware a sinistra ("+6", poi 12/24/...), con lo
    0 dB (unity) evidenziato in accent. La geometria replica drawFader del LAF
    (track = area.reduced(0,4), thumb 18px): ogni taglietto e' allineato al
    CENTRO del thumb per quella posizione. Va dipinta PRIMA del fader: il thumb
    copre i segni al passaggio (comportamento standard DAW).

    No-op sotto 60px sull'asse del fader. In verticale: label con corsa >= 90px
    e larghezza >= 50px. In orizzontale: label dritte sopra il track, con corsa
    >= 180px e altezza >= 36px per non accavallare i major. */
void drawStripFaderScale (juce::Graphics& g, juce::Rectangle<int> faderArea,
                         StripOrientation = StripOrientation::vertical,
                         float maximumDb = kFaderMaxDb);

/** Variante presentation-driven della scala. Il consumer fornisce soltanto
    posizioni/label semantiche; layout, allineamento e stile restano canonici. */
void drawStripFaderScale (
    juce::Graphics& g, juce::Rectangle<int> faderArea,
    std::span<const StripFaderScaleMark> marks,
    StripOrientation = StripOrientation::vertical);

/** Stessa ricetta LAF del mixer, ruotata per il fader orizzontale. La scala
    resta separata per mantenere le etichette leggibili in entrambi gli assi. */
void drawStripFader (juce::Graphics&, juce::Rectangle<int>, float position,
                     juce::Colour, juce::Component&, StripOrientation);

} // namespace ayra
