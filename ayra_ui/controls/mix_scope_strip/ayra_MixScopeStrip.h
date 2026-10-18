/*
  ==============================================================================

   ayra_common / widgets / controls / mix_scope_strip

   Strip compatta e riusabile per un livello logico di mix. Non possiede stato
   di dominio: visualizza valori e inoltra gesti semantici al consumer.

  ==============================================================================
*/

namespace ayra
{

class MixScopeStrip final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    /** Geometria del controllo. `wide` e' una riga compatta; `tall` usa la
        stessa anatomia verticale delle channel strip del Mixer. */
    enum class Layout
    {
        wide,
        tall
    };

    /** Gerarchia visiva della testata, indipendente dal routing. `bus` indica
        una somma con cap incassato e simbolo di somma; l'app sceglie il ruolo.
        Non modifica controlli, selezione o stato di dominio. */
    enum class HeaderStyle { channel, bus };

    enum ColourIds
    {
        backgroundColourId = 0x3b00201,
        titleColourId      = 0x3b00202,
        statusColourId     = 0x3b00203,
        accentColourId     = 0x3b00204,
        routeColourId      = 0x3b00205,
        busHeaderBackgroundColourId = 0x3b00206,
        busHeaderTextColourId       = 0x3b00207,
        busHeaderBorderColourId     = 0x3b00208
    };

    enum class Control
    {
        gain,
        pan,
        primaryToggle
    };

    /** Bank meter neutri rispetto al dominio applicativo. Il consumer decide
        cosa rappresentino i due boundary; il widget possiede soltanto IN/OUT. */
    enum class MeterBank
    {
        input,
        output
    };

    /** Presentazione compatta del controllo spaziale. Il widget non conosce
        Panpot, layout speaker o modalita' DSP: il consumer sceglie la forma. */
    enum class PanPresentation
    {
        hidden,
        scalar,
        editor
    };

    /** Il drag applica un delta (eventualmente linked dal consumer); reset,
        testo e tastiera assegnano un valore al solo controllo indirizzato. */
    enum class GestureMode { relative, absolute };
    [[nodiscard]] GestureMode getGestureMode() const noexcept { return gestureMode; }

    /** Presentazione opzionale del controllo gain.

        Il widget continua a lavorare internamente con una posizione normalizzata
        0..1, ma il consumer puo' assegnarle un'unita' diversa dai dB senza
        duplicare geometria, mouse tracking o readout. Il default resta la legge
        fader dB canonica. Tutte le callback vivono sul Message Thread. */
    struct GainValuePresentation
    {
        std::function<float (float)> valueToPosition;
        std::function<float (float)> positionToValue;
        std::function<juce::String (float)> formatEditorText;
        std::function<juce::String (float)> formatReadoutText;
        std::function<bool (const juce::String&, float&)> parseText;
        /** Scala visuale opzionale. Vuota = scala dB canonica; le posizioni
            sono normalizzate e condividono la geometria del thumb. */
        std::vector<StripFaderScaleMark> scaleMarks;
        float defaultValue = 0.0f;
        float maximumDb = kFaderMaxDb;
        juce::String title;
        juce::String tooltip;

        [[nodiscard]] bool isValid() const noexcept
        {
            return valueToPosition != nullptr
                && positionToValue != nullptr
                && formatEditorText != nullptr
                && formatReadoutText != nullptr
                && parseText != nullptr;
        }
    };

    /** Interfaccia di disegno adottabile dalla LookAndFeel dell'app. I default
        inoltrano alle primitive canoniche di ayra_common. */
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;
        virtual void drawMixScopeStripBackground (juce::Graphics&, int width,
                                                  int height, MixScopeStrip&);
        virtual void drawMixScopeStripHeader (juce::Graphics&,
                                              juce::Rectangle<int>,
                                              MixScopeStrip&);
        virtual void drawMixScopeStripControlLabels (juce::Graphics&,
                                                     juce::Rectangle<int> gain,
                                                     juce::Rectangle<int> pan,
                                                     MixScopeStrip&);
        virtual void drawMixScopeStripFader (juce::Graphics&, juce::Rectangle<int>, MixScopeStrip&);
        virtual void drawMixScopeStripPan (juce::Graphics&, juce::Rectangle<int>, MixScopeStrip&);
        virtual void drawMixScopeStripReadout (juce::Graphics&, juce::Rectangle<int>, MixScopeStrip&);
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void mixScopeControlChanged (MixScopeStrip*, Control) = 0;
        virtual void mixScopeGestureStarted (MixScopeStrip*, Control) {}
        virtual void mixScopeGestureEnded (MixScopeStrip*, Control) {}
        virtual void mixScopeRouteRequested (MixScopeStrip*) {}
        virtual void mixScopeSettingsRequested (MixScopeStrip*) {}
        virtual void mixScopeGraphRequested (MixScopeStrip*) {}
        virtual void mixScopePanEditorRequested (MixScopeStrip*) {}
        virtual void mixScopeSoloChanged (MixScopeStrip*, bool) {}
        virtual void mixScopeBackgroundPressed (MixScopeStrip*, const juce::MouseEvent&) {}
        virtual void mixScopeTitleEditRequested (MixScopeStrip*) {}
    };

    MixScopeStrip();
    ~MixScopeStrip() override;

    void setTitle (juce::String);
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    void setHeaderStyle (HeaderStyle);
    [[nodiscard]] HeaderStyle getHeaderStyle() const noexcept { return headerStyle; }
    /** Identita' semantica non interattiva nella testata. Se assente, una testata
        `bus` continua a usare il simbolo di somma canonico. */
    void setHeaderGlyph (const glyphs::Glyph&);
    [[nodiscard]] const glyphs::Glyph& getHeaderGlyph() const noexcept { return headerGlyph; }
    [[nodiscard]] juce::Rectangle<int> getHeaderGlyphBounds() const noexcept { return headerGlyphBounds; }

    void setStatusText (juce::String);
    [[nodiscard]] const juce::String& getStatusText() const noexcept { return status; }

    void setAccentColour (juce::Colour);
    [[nodiscard]] juce::Colour getAccentColour() const noexcept;

    /** Installa una scala/unita' alternativa per il fader. Il valore corrente
        viene reinterpretato nella nuova presentation; il consumer imposta poi
        il proprio valore canonico con setGainValue(). */
    void setGainValuePresentation (GainValuePresentation);
    /** Ripristina la legge canonica dB. */
    void resetGainValuePresentation();

    void setGainValue (float);
    /** Stato di sola vista: il thumb a unity e' provvisorio, senza scrittura. */
    void setGainMixed (bool);
    [[nodiscard]] bool isGainMixed() const noexcept { return gainMixed; }
    void setPanDisplayCodec (std::function<juce::String (float)> format,
                            std::function<bool (const juce::String&, float&)> parse,
                            juce::String start = "L", juce::String end = "R",
                            std::uint64_t generation = 0, int displayIdentity = 0);
    [[nodiscard]] float getGainValue() const noexcept;
    [[nodiscard]] float gainValueForPosition (float position) const;
    [[nodiscard]] float gainPositionForValue (float value) const;
    [[nodiscard]] juce::String getGainReadoutText() const;
    [[nodiscard]] std::span<const StripFaderScaleMark>
        getGainScaleMarks() const noexcept
    {
        return gainPresentation.scaleMarks;
    }

    /** API dB storica del widget: equivale a set/getGainValue con la
        presentation dB di default. */
    void setGainDb (float);
    void setPan (float);
    void setPrimaryToggle (bool);
    [[nodiscard]] float getGainDb() const noexcept;
    [[nodiscard]] float getPan() const noexcept;
    [[nodiscard]] bool getPrimaryToggle() const noexcept;

    void setPrimaryToggleLabel (juce::String, juce::Colour activeColour);
    void setGainTooltip (juce::String);
    void setPanTooltip (juce::String);
    void setPrimaryToggleTooltip (juce::String);
    void setPrimaryToggleVisible (bool);
    void setRouteVisible (bool);
    /** Azione Settings opzionale. Nel layout tall occupa la cella sotto Solo,
        affiancata al Routing che resta sotto Mute. */
    void setSettingsVisible (bool);
    /** Azione Graph opzionale della strip; disponibilita' e apertura sono del consumer. */
    void setGraphVisible (bool);
    void setGraphEnabled (bool, juce::String tooltip);
    /** Visibilita' globale dell'area meter. I bank senza source restano assenti. */
    void setMeterVisible (bool);
    void setMeterBankVisible (MeterBank, bool);
    [[nodiscard]] bool isMeterBankVisible (MeterBank) const noexcept;
    /** Il fader esiste soltanto quando il consumer ha un target Gain vivo. */
    void setGainVisible (bool);
    [[nodiscard]] bool isGainVisible() const noexcept { return gainSectionVisible; }
    /** La preferenza di sezione resta separata dalla forma del controllo. */
    void setPanVisible (bool);
    void setPanPresentation (PanPresentation);
    [[nodiscard]] PanPresentation getPanPresentation() const noexcept
    {
        return panPresentation;
    }
    /** Aggiorna la sola preview derivata del controllo spaziale. Le coordinate
        restano nel dominio canonico `[-1,+1]`; nessuno stato viene authored dal
        widget finche' il consumer non riceve `onPanEditorRequested`. */
    void setSpatialPanPreview (juce::Point<float> position, bool circular);
    void setSpatialPanMarkers (std::vector<PositionField2D::Marker>);
    void setControlsEnabled (bool gainEnabled, bool panEnabled);
    void setSoloVisible (bool);
    void setSelected (bool);
    void setInaudible (bool);
    /** Slot borrowed sotto i controlli comuni. Nessuna ownership trasferita:
        il consumer sgancia gli slot prima del proprio teardown. */
    void setAccessoryComponent (juce::Component*, int preferredHeight);
    /** Slot borrowed nel cap, immediatamente a sinistra del titolo. */
    void setHeaderLeadingComponent (juce::Component*, int preferredWidth);
    [[nodiscard]] bool hasHeaderLeadingComponent() const noexcept { return headerLeading != nullptr; }
    /** Slot borrowed sempre sopra il cap e quindi sopra il nome traccia. */
    void setCueComponent (juce::Component*, int preferredHeight);
    [[nodiscard]] StripToggleButton& getMuteButton() noexcept { return primaryToggle; }
    [[nodiscard]] StripToggleButton& getSoloButton() noexcept { return soloToggle; }
    [[nodiscard]] juce::Button& getRouteButton() noexcept { return routeButton; }
    [[nodiscard]] juce::Button& getSettingsButton() noexcept { return settingsButton; }
    [[nodiscard]] juce::Button& getGraphButton() noexcept { return graphButton; }
    [[nodiscard]] juce::Button& getPanEditorButton() noexcept;
    [[nodiscard]] juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    [[nodiscard]] float getGainPosition() const noexcept;
    [[nodiscard]] juce::Slider& getSlider (Control) noexcept;

    // Minimi condivisi da consumer e layout: nessuna soglia parallela nei pannelli.
    static constexpr int preferredWidth = 128;
    static constexpr int minimumTallHeight = 280;
    static constexpr int minimumWideWidth = 360;
    static constexpr int minimumWideHeight = 118;
    /** Extent minimo sul cross-axis per evitare canali sub-pixel. Il bank
        allarga la preferred size del widget; lo scroll resta al contenitore. */
    static constexpr int minimumMeterChannelExtent = 10;
    static constexpr int meterBankPadding = 4;
    static constexpr int meterBankGap = 8;
    /** Preferred size del contenuto completo. In modalita' tall la larghezza
        cresce con IN e OUT; in wide cresce l'altezza. I contenitori esterni
        applicano lo scrolling, quindi il widget non comprime i canali. */
    [[nodiscard]] int getPreferredWidthForLayout (Layout) const noexcept;
    [[nodiscard]] int getPreferredHeightForLayout (Layout) const noexcept;
    /** Borrowed, Message Thread. Ogni AudioMeterComponent mantiene la propria
        ReaderLease generation-safe; il consumer garantisce soltanto il lifetime
        della source fino al detach/rebind. */
    void attachMeterSource (MeterBank, AudioLevelMeterSource<float>*);
    void detachMeterSource (MeterBank);
    void detachMeterSources();

    [[nodiscard]] AudioMeterComponent& getMeterComponent (MeterBank) noexcept;
    [[nodiscard]] const AudioMeterComponent&
        getMeterComponent (MeterBank) const noexcept;
    [[nodiscard]] int getMeterBankNumChannels (MeterBank) const noexcept;
    [[nodiscard]] juce::String getMeterBankChannelLabel (
        MeterBank, int channel) const;
    [[nodiscard]] const juce::String&
        getMeterBankLayoutIdentifier (MeterBank) const noexcept;

    void setLayout (Layout);
    [[nodiscard]] Layout getLayout() const noexcept { return layout; }

    [[nodiscard]] bool isGainGestureActive() const noexcept { return gainGestureActive; }
    [[nodiscard]] bool isPanGestureActive() const noexcept { return panGestureActive; }

    void addListener (Listener*);
    void removeListener (Listener*);

    std::function<void (Control)> onControlChanged;
    std::function<void (Control)> onGestureStart;
    std::function<void (Control)> onGestureEnd;
    std::function<void()> onRoutingRequested;
    std::function<void()> onSettingsRequested;
    std::function<void()> onGraphRequested;
    std::function<void()> onPanEditorRequested;
    std::function<void()> onPreferredSizeChanged;
    std::function<void (bool)> onSoloChanged;
    std::function<void (const juce::MouseEvent&)> onBackgroundPressed;
    std::function<void()> onTitleEditRequested;
    /** Policy app-specifica: puo' consumare popup/selezione senza iniziare un
        gesto. Riceve coordinate locali del controllo; non cambia geometria. */
    std::function<bool (Control, const juce::MouseEvent&)> gestureFilter;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    class ControlSlider;
    class SpatialPreviewButton;
    struct MeterBankView final
    {
        AudioMeterComponent meter;
        juce::Viewport viewport;
        juce::Label summary;
        AudioLevelMeterSource<float>* source = nullptr; // borrowed
        juce::String roleLabel;
        juce::String layoutIdentifier;
        juce::StringArray channelLabels;
        bool enabled = true;
    };

    class ReadoutLabel final : public juce::Label
    {
    public:
        explicit ReadoutLabel (MixScopeStrip& strip) : owner (strip) {}
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
    private:
        MixScopeStrip& owner;
    };

    [[nodiscard]] LookAndFeelMethods& getLAF();
    [[nodiscard]] StripOrientation orientation() const noexcept
    {
        return layout == Layout::tall ? StripOrientation::vertical : StripOrientation::horizontal;
    }
    void notifyChanged (Control);
    void notifyGestureStart (Control, GestureMode = GestureMode::absolute);
    void notifyGestureEnd (Control);
    void notifyRouteRequested();
    void notifySettingsRequested();
    void notifyPanEditorRequested();
    [[nodiscard]] bool hasPanAffordance() const noexcept;
    void updatePanPresentationVisibility();
    bool allowGesture (Control, const juce::MouseEvent&);
    void applyGainValuePresentation();
    [[nodiscard]] bool parseGainText (const juce::String&, float&) const;
    [[nodiscard]] juce::String formatGainEditorText (float) const;
    [[nodiscard]] float gainDefaultPosition() const;
    [[nodiscard]] MeterBankView& meterBankView (MeterBank) noexcept;
    [[nodiscard]] const MeterBankView& meterBankView (MeterBank) const noexcept;
    void syncMeterBankMetadata (MeterBank);
    void updateMeterBankVisibility();
    void layoutMeterBank (MeterBank, juce::Rectangle<int>);
    void clearMeterBankBounds();
    [[nodiscard]] int meterBankExtent (MeterBank) const noexcept;
    void notifyPreferredSizeChangedIfNeeded();

    juce::String title { "Mix" };
    juce::String status;
    std::unique_ptr<ControlSlider> gain;
    std::unique_ptr<ControlSlider> pan;
    StripToggleButton primaryToggle, soloToggle;
    ShapeIconButton routeButton;
    ShapeIconButton settingsButton;
    ShapeIconButton graphButton;
    std::unique_ptr<SpatialPreviewButton> panEditorButton;
    MeterBankView inputMeter;
    MeterBankView outputMeter;
    ReadoutLabel gainEditor;
    juce::Rectangle<int> readoutBounds;
    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> headerGlyphBounds;
    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> gainBounds;
    juce::Rectangle<int> panBounds;
    juce::ListenerList<Listener> listeners;
    bool gainGestureActive = false;
    bool panGestureActive = false;
    GestureMode gestureMode = GestureMode::absolute;
    GainValuePresentation gainPresentation;
    bool gainMixed = false;
    std::function<juce::String (float)> panFormatter;
    std::function<bool (const juce::String&, float&)> panParser;
    juce::String panStartLabel { "L" }, panEndLabel { "R" };
    std::uint64_t panContextGeneration = 0;
    int panDisplayIdentity = 0;
    float displayedGainValue = 0.0f; ///< valore semantico del binding; puo' eccedere la corsa visiva
    Layout layout = Layout::wide;
    HeaderStyle headerStyle = HeaderStyle::channel;
    glyphs::Glyph headerGlyph;
    juce::Component* accessory = nullptr;
    juce::Component* headerLeading = nullptr;
    juce::Component* cues = nullptr;
    int accessoryHeight = 0, headerLeadingWidth = 0, cuesHeight = 0;
    bool selected = false, inaudible = false;
    bool meterSectionVisible = false;
    bool gainSectionVisible = true;
    bool panSectionVisible = true;
    bool panControlEnabled = true;
    PanPresentation panPresentation = PanPresentation::scalar;
    int lastPreferredWidth = preferredWidth;
    int lastPreferredHeight = minimumWideHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixScopeStrip)
};

} // namespace ayra
