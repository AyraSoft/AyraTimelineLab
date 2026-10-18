/*
  ==============================================================================

   ayra_common / widgets / controls / mix_scope_strip

  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Un solo input controller per entrambe le view: il fader usa esattamente la
// geometria dipinta, il pan conserva la sensibilita' relativa del mixer.
// Slider fornisce accessibility e valore; non gli deleghiamo una seconda
// geometria mouse indipendente dal LookAndFeel.
class MixScopeStrip::ControlSlider final : public juce::Slider
{
public:
    ControlSlider (MixScopeStrip& strip, Control kind) : owner (strip), control (kind)
    {
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        editor.setJustificationType (juce::Justification::centred);
        editor.onTextChange = [this]
        {
            // Il testo iniziale puo' essere quantizzato (MIDI): confermarlo
            // senza editarlo non deve spostare il valore canonico continuo.
            if (! panTextContextValid || editor.getText() == initialEditorText
                || owner.panContextGeneration != editorGeneration
                || owner.panDisplayIdentity != editorDisplayIdentity) { return; }
            float value = 0.0f;
            if (! (owner.panParser ? owner.panParser (editor.getText(), value)
                                   : parsePanText (editor.getText(), value))) { return; }
            panTextContextValid = false;
            commit (value);
        };
        editor.onEditorHide = [this]
        {
            // JUCE chiama onEditorHide prima di onTextChange al commit.
            // L'invalidazione semantica avviene in invalidateTextEdit; un
            // commit valido consuma il contesto nel callback del testo.
            editor.setVisible (false);
            repaint();
        };
        addChildComponent (editor);
    }

    void invalidateTextEdit()
    {
        panTextContextValid = false;
        if (editor.isBeingEdited()) { editor.hideEditor (true); }
        editor.setVisible (false);
    }

    void paint (juce::Graphics& g) override
    {
        if (control == Control::gain)
            owner.getLAF().drawMixScopeStripFader (g, getLocalBounds(), owner);
        else
            owner.getLAF().drawMixScopeStripPan (g, getLocalBounds(), owner);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! accepts (event)) { return; }
        startValue = (float) getValue();
        startPoint = event.position;
        dragging = true;
        if (isShowing()) { grabKeyboardFocus(); }
        owner.notifyGestureStart (control, GestureMode::relative);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragging) { return; }
        const auto delta = event.position - startPoint;
        const float value = control == Control::gain
            ? StripFaderGeometry { getLocalBounds().toFloat(), owner.orientation() }
                .draggedPosition (startValue, delta)
            : StripPanGeometry::draggedValue (startValue, delta);
        setValue (value, juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) { return; }
        dragging = false;
        owner.notifyGestureEnd (control);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! accepts (event)) { return; }
        // JUCE puo' consegnare il double-click dopo il secondo mouseDown:
        // chiude prima il drag aperto, senza annidare gesture host/undo.
        if (dragging)
        {
            dragging = false;
            owner.notifyGestureEnd (control);
        }
        const bool wantsTextEdit = control == Control::pan
            && (event.mods.isAltDown() || event.mods.isShiftDown())
            && panReadoutRect (getLocalBounds().toFloat()).contains (event.getPosition());
        if (wantsTextEdit)
        {
            editor.setBounds (panReadoutRect (getLocalBounds().toFloat()).expanded (6, 4).constrainedWithin (getLocalBounds()));
            initialEditorText = owner.panFormatter ? owner.panFormatter ((float) getValue())
                                                  : panValueToText ((float) getValue());
            editorGeneration = owner.panContextGeneration;
            editorDisplayIdentity = owner.panDisplayIdentity;
            panTextContextValid = true;
            editor.setText (initialEditorText, juce::dontSendNotification);
            editor.setVisible (true);
            editor.showEditor();
        }
        else
            commit (control == Control::gain ? owner.gainDefaultPosition() : 0.0f);
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (dragging || ! accepts (event)) { return; }
        const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0f) { return; }
        const float step = event.mods.isAltDown() ? 0.001f : 0.01f;
        commit ((float) getValue() + (delta > 0.0f ? step : -step));
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const int code = key.getKeyCode();
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey
            || code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey)
        {
            const float step = key.getModifiers().isAltDown() ? 0.001f : 0.01f;
            commit ((float) getValue() + (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey ? -step : step));
            return true;
        }
        if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
        {
            commit ((float) (code == juce::KeyPress::homeKey ? getMinimum() : getMaximum()));
            return true;
        }
        return juce::Slider::keyPressed (key);
    }

private:
    bool accepts (const juce::MouseEvent& event)
    {
        return isEnabled() && owner.allowGesture (control, event);
    }
    void commit (float value)
    {
        owner.notifyGestureStart (control);
        const bool explicitMixed = control == Control::gain && owner.gainMixed
            && juce::approximatelyEqual ((float) getValue(), value);
        setValue (value, juce::sendNotificationSync);
        if (explicitMixed)
        {
            owner.gainMixed = false;
            owner.setGainValue (owner.gainValueForPosition (value));
            owner.notifyChanged (control);
        }
        owner.notifyGestureEnd (control);
    }
    MixScopeStrip& owner;
    Control control;
    juce::Label editor;
    juce::String initialEditorText;
    std::uint64_t editorGeneration = 0;
    int editorDisplayIdentity = 0;
    bool panTextContextValid = false;
    float startValue = 0.0f;
    juce::Point<float> startPoint;
    bool dragging = false;
};

class MixScopeStrip::SpatialPreviewButton final : public juce::Button
{
public:
    explicit SpatialPreviewButton (MixScopeStrip& strip)
        : juce::Button ("Spatial pan preview"), owner (strip)
    {
        setWantsKeyboardFocus (true);
        field.setMode (PositionField2D::Mode::preview);
        field.setReadoutsVisible (false);
        field.setCaptionsVisible (false);
        field.setShowGrid (true);
        field.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (field);
    }

    void setPreview (const juce::Point<float> nextPosition,
                     const bool shouldBeCircular)
    {
        field.getPad().setPointColour (owner.getAccentColour());
        const auto normalized = juce::Point<float> {
            (nextPosition.x + 1.0f) * 0.5f,
            (nextPosition.y + 1.0f) * 0.5f
        };
        static_cast<void> (field.setDomain (shouldBeCircular
            ? XYPad::Domain::circle ({ 0.5f, 0.5f }, 0.5f)
            : XYPad::Domain::rectangle()));
        field.setPointPositionUnsnapped (normalized, juce::dontSendNotification);
    }

    void setMarkers (std::vector<PositionField2D::Marker> markers)
    {
        field.setMarkers (std::move (markers));
    }

    void paintButton (juce::Graphics&, bool, bool) override {}
    void resized() override { field.setBounds (getLocalBounds()); }

private:
    MixScopeStrip& owner;
    PositionField2D field;
};

juce::Button& MixScopeStrip::getPanEditorButton() noexcept
{
    return *panEditorButton;
}

bool MixScopeStrip::allowGesture (Control control, const juce::MouseEvent& event)
{
    return (! gestureFilter || gestureFilter (control, event)) && ! event.mods.isPopupMenu();
}
void MixScopeStrip::ReadoutLabel::mouseDown (const juce::MouseEvent& event)
{
    if (owner.allowGesture (Control::gain, event)) { juce::Label::mouseDown (event); }
}
void MixScopeStrip::ReadoutLabel::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (owner.allowGesture (Control::gain, event)) { juce::Label::mouseDoubleClick (event); }
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripBackground (
    juce::Graphics& g, const int width, const int height, MixScopeStrip& strip)
{
    getCommonLAF (strip).drawChannelStripBackground (
        g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f));
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripHeader (
    juce::Graphics& g, const juce::Rectangle<int> bounds, MixScopeStrip& strip)
{
    const auto accent = strip.getAccentColour();
    const bool bus = strip.getHeaderStyle() == HeaderStyle::bus;
    const auto headerInk = bus
        ? tokens::componentColour (strip, busHeaderTextColourId,
                                   tokens::textPrimary().interpolatedWith (accent, 0.25f))
        : tokens::textOnTint (accent);
    if (bus)
    {
        const auto cap = bounds.toFloat();
        const auto bed = tokens::componentColour (strip, busHeaderBackgroundColourId,
            tokens::surfaceRecessed().interpolatedWith (accent, 0.09f));
        g.setColour (bed);
        g.fillRoundedRectangle (cap, 6.0f);
        g.setColour (tokens::componentColour (strip, busHeaderBorderColourId,
                                               accent.withAlpha (0.32f)));
        g.drawRoundedRectangle (cap.reduced (0.5f), 5.5f, 1.0f);

        g.setColour (headerInk);
    }
    else
    {
        getCommonLAF (strip).drawColourCap (g, bounds.toFloat(), accent, {});
        g.setColour (headerInk);
    }

    if (! strip.hasHeaderLeadingComponent())
    {
        const auto& requested = strip.getHeaderGlyph();
        const auto& glyph = requested.isValid() ? requested : glyphs::sum;
        if (requested.isValid() || bus)
            glyphs::drawGlyph (g, glyph,
                               strip.getHeaderGlyphBounds().toFloat().reduced (2.0f),
                               headerInk);
    }
    g.setFont (tokens::type::section().withHeight (11.0f));
    g.drawText (strip.getTitle(), strip.getTitleBounds().reduced (3, 0),
                juce::Justification::centred, true);
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripControlLabels (
    juce::Graphics& g, juce::Rectangle<int> gainArea,
    juce::Rectangle<int> panArea, MixScopeStrip& strip)
{
    juce::ignoreUnused (g, gainArea, panArea, strip);
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripFader (
    juce::Graphics& g, const juce::Rectangle<int> area, MixScopeStrip& strip)
{
    const auto orientation = strip.getLayout() == Layout::tall
        ? StripOrientation::vertical : StripOrientation::horizontal;
    if (strip.gainPresentation.scaleMarks.empty())
        drawStripFaderScale (
            g, area, orientation, strip.gainPresentation.maximumDb);
    else
        drawStripFaderScale (
            g, area, std::span<const StripFaderScaleMark> (
                strip.gainPresentation.scaleMarks), orientation);
    drawStripFader (g, area, (float) strip.gain->getValue(), strip.getAccentColour().withMultipliedAlpha (strip.gainMixed ? 0.45f : 1.0f), strip, orientation);
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripPan (
    juce::Graphics& g, const juce::Rectangle<int> area, MixScopeStrip& strip)
{
    drawPanKnob (g, area.toFloat(), strip.getPan(),
                 strip.pan->isMouseOverOrDragging(), strip.isPanGestureActive(),
                 strip.panFormatter ? strip.panFormatter (strip.getPan()) : juce::String {},
                 strip.panStartLabel, strip.panEndLabel);
}

void MixScopeStrip::LookAndFeelMethods::drawMixScopeStripReadout (
    juce::Graphics& g, const juce::Rectangle<int> area, MixScopeStrip& strip)
{
    drawDbChip (g, area, ! strip.gain->isEnabled()
        ? juce::String ("--") : strip.getGainReadoutText());
}

MixScopeStrip::MixScopeStrip()
    : gain (std::make_unique<ControlSlider> (*this, Control::gain)),
      pan (std::make_unique<ControlSlider> (*this, Control::pan)), gainEditor (*this)
{
    gain->setSliderStyle (juce::Slider::LinearHorizontal);
    gain->setRange (0.0, 1.0, 0.0);
    gain->setValue (faderDbToPos (0.0f), juce::dontSendNotification);
    gain->setName ("Gain");

    pan->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan->setRange (-1.0, 1.0, 0.0);
    pan->setDoubleClickReturnValue (true, 0.0);
    pan->setName ("Pan");
    pan->setTitle ("Pan / balance");
    pan->setTooltip ("Pan / balance control");

    primaryToggle.setLetter ("M", tokens::muteIndicator());
    primaryToggle.setTooltip ("Primary mix toggle");
    routeButton.setName ("Routing");
    routeButton.setGlyph (glyphs::snapConnections);
    routeButton.setShowIdleBackground (true);
    routeButton.setTooltip ("Open Routing");
    settingsButton.setName ("Settings");
    settingsButton.setGlyph (glyphs::settings);
    settingsButton.setShowIdleBackground (true);
    settingsButton.setTooltip ("Open Settings");

    for (auto* component : { static_cast<juce::Component*> (gain.get()),
                             static_cast<juce::Component*> (pan.get()),
                             static_cast<juce::Component*> (&primaryToggle),
                             static_cast<juce::Component*> (&routeButton),
                             static_cast<juce::Component*> (&settingsButton) })
    {
        component->setWantsKeyboardFocus (true);
        addAndMakeVisible (component);
    }

    routeButton.setVisible (false);
    settingsButton.setVisible (false);
    graphButton.setGlyph (glyphs::window);
    graphButton.setShowIdleBackground (true);
    graphButton.setTooltip ("Open graph");
    graphButton.onClick = [this]
    {
        listeners.call ([this] (Listener& listener) { listener.mixScopeGraphRequested (this); });
        if (onGraphRequested) { onGraphRequested(); }
    };
    addChildComponent (graphButton);

    panEditorButton = std::make_unique<SpatialPreviewButton> (*this);
    panEditorButton->setName ("Spatial pan preview");
    panEditorButton->setTooltip ("Open spatial pan editor");
    panEditorButton->onClick = [this] { notifyPanEditorRequested(); };
    addChildComponent (*panEditorButton);

    const auto configureMeterBank = [this] (
        MeterBankView& bank, const MeterBank role,
        const char* const roleText)
    {
        bank.roleLabel = roleText;
        bank.meter.setName (
            role == MeterBank::input ? "Input meter" : "Output meter");
        bank.meter.setOrientation (
            AudioMeterComponent::Orientation::horizontal);
        bank.meter.setScale (AudioMeterComponent::Scale::decibels);
        bank.meter.setFloorDecibels (kFaderMinDb);
        bank.meter.setScaleMarksVisible (false);
        bank.meter.setReadoutVisible (false);
        bank.meter.onSourceGenerationChanged = [this, role]
        {
            syncMeterBankMetadata (role);
            resized();
        };

        bank.viewport.setName (
            role == MeterBank::input ? "Input meter viewport"
                                     : "Output meter viewport");
        bank.viewport.setViewedComponent (&bank.meter, false);
        bank.viewport.setScrollBarThickness (7);
        bank.viewport.setScrollOnDragEnabled (false);
        addChildComponent (bank.viewport);

        bank.summary.setName (
            role == MeterBank::input ? "Input meter summary"
                                     : "Output meter summary");
        bank.summary.setJustificationType (juce::Justification::centredLeft);
        bank.summary.setFont (
            tokens::type::caption().withHeight (10.0f).boldened());
        bank.summary.setMinimumHorizontalScale (1.0f);
        bank.summary.setInterceptsMouseClicks (false, false);
        addChildComponent (bank.summary);
    };
    configureMeterBank (inputMeter, MeterBank::input, "IN");
    configureMeterBank (outputMeter, MeterBank::output, "OUT");

    gainEditor.setJustificationType (juce::Justification::centred);
    gainEditor.setEditable (false, true, false);
    gainEditor.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
    gainEditor.onTextChange = [this]
    {
        float value = 0.0f;
        if (! parseGainText (gainEditor.getText(), value))
        {
            gainEditor.setText (
                formatGainEditorText (displayedGainValue),
                juce::dontSendNotification);
            repaint (readoutBounds);
            return;
        }
        if (! gainMixed && juce::approximatelyEqual (value, displayedGainValue))
        {
            gainEditor.setText (
                formatGainEditorText (displayedGainValue),
                juce::dontSendNotification);
            repaint (readoutBounds);
            return;
        }
        notifyGestureStart (Control::gain);
        // Il valore numerico non e' limitato dalla corsa visiva: la
        // presentation decide il clamp della sola maniglia.
        gainMixed = false;
        setGainValue (value);
        notifyChanged (Control::gain);
        notifyGestureEnd (Control::gain);
    };
    gainEditor.onEditorHide = [this]
    {
        // JUCE notifica la chiusura prima di consegnare onTextChange: non
        // ripristinare qui il testo, altrimenti il valore appena digitato viene
        // perso. Il callback sopra normalizza sia commit validi sia input errati.
        repaint (readoutBounds);
    };
    addAndMakeVisible (gainEditor);

    gain->onValueChange = [this]
    {
        gainMixed = false;
        displayedGainValue = gainValueForPosition ((float) gain->getValue());
        gainEditor.setText (
            formatGainEditorText (displayedGainValue),
            juce::dontSendNotification);
        repaint (readoutBounds);
        notifyChanged (Control::gain);
    };
    pan->onValueChange = [this] { notifyChanged (Control::pan); };
    soloToggle.setLetter ("S", tokens::soloIndicator());
    soloToggle.setTooltip ("Solo");
    soloToggle.onToggled = [this] (bool value)
    {
        listeners.call ([this, value] (Listener& listener) { listener.mixScopeSoloChanged (this, value); });
        if (onSoloChanged) { onSoloChanged (value); }
    };
    addChildComponent (soloToggle);

    primaryToggle.onToggled = [this] (bool) { notifyChanged (Control::primaryToggle); };
    routeButton.onClick = [this]
    {
        notifyRouteRequested();
    };
    settingsButton.onClick = [this]
    {
        notifySettingsRequested();
    };

    setLayout (Layout::wide);
    resetGainValuePresentation();
    setGainValue (0.0f);
    setSize (360, 76);
}

MixScopeStrip::~MixScopeStrip()
{
    inputMeter.meter.onSourceGenerationChanged = {};
    outputMeter.meter.onSourceGenerationChanged = {};
    detachMeterSources();
    gain->onValueChange = {};
    pan->onValueChange = {};
}

void MixScopeStrip::setTitle (juce::String newTitle)
{
    if (title == newTitle) { return; }
    title = std::move (newTitle);
    repaint (headerBounds);
}

void MixScopeStrip::setStatusText (juce::String newStatus)
{
    if (status == newStatus) { return; }
    status = std::move (newStatus);
    repaint (headerBounds);
}

void MixScopeStrip::setAccentColour (const juce::Colour colour)
{
    if (getAccentColour() == colour) { return; }
    setColour (accentColourId, colour);
    gain->setColour (juce::Slider::trackColourId, colour);
    repaint();
}

juce::Colour MixScopeStrip::getAccentColour() const noexcept
{
    return tokens::componentColour (*this, accentColourId, tokens::accent());
}

void MixScopeStrip::setGainValuePresentation (
    GainValuePresentation presentation)
{
    jassert (presentation.isValid());
    if (! presentation.isValid())
    {
        resetGainValuePresentation();
        return;
    }
    gainPresentation = std::move (presentation);
    applyGainValuePresentation();
}

void MixScopeStrip::resetGainValuePresentation()
{
    GainValuePresentation presentation;
    presentation.valueToPosition = [] (const float value)
    {
        return faderDbToPos (value);
    };
    presentation.positionToValue = [] (const float position)
    {
        return faderPosToDb (position);
    };
    presentation.formatEditorText = [] (const float value)
    {
        return value <= kSilenceDb
            ? juce::String ("-inf dB")
            : juce::String (value, 1) + " dB";
    };
    presentation.formatReadoutText = [] (const float value)
    {
        return value <= kSilenceDb
            ? juce::String ("-inf") : juce::String (value, 1);
    };
    presentation.parseText = [] (const juce::String& text, float& value)
    {
        return parseDbText (text, value);
    };
    presentation.defaultValue = 0.0f;
    presentation.title = "Mix gain in decibels";
    presentation.tooltip = "Gain in dB";
    gainPresentation = std::move (presentation);
    applyGainValuePresentation();
}

void MixScopeStrip::applyGainValuePresentation()
{
    if (! gainPresentation.isValid()) { return; }

    gain->setTitle (gainPresentation.title);
    gain->setTooltip (gainPresentation.tooltip);
    gain->setDoubleClickReturnValue (true, gainDefaultPosition());
    gain->textFromValueFunction = [this] (const double position)
    {
        return formatGainEditorText (
            gainValueForPosition ((float) position));
    };
    gain->valueFromTextFunction = [this] (const juce::String& text)
    {
        float value = displayedGainValue;
        return parseGainText (text, value)
            ? (double) gainPositionForValue (value)
            : gain->getValue();
    };
    gain->setValue (
        gainPositionForValue (displayedGainValue),
        juce::dontSendNotification);
    if (! gainEditor.isBeingEdited())
        gainEditor.setText (
            formatGainEditorText (displayedGainValue),
            juce::dontSendNotification);
    repaint (readoutBounds);
}

bool MixScopeStrip::parseGainText (
    const juce::String& text, float& value) const
{
    return gainPresentation.isValid()
        && gainPresentation.parseText (text, value)
        && std::isfinite (value);
}

juce::String MixScopeStrip::formatGainEditorText (
    const float value) const
{
    if (gainMixed) { return "Misto"; }
    return gainPresentation.isValid()
        ? gainPresentation.formatEditorText (value) : juce::String {};
}

float MixScopeStrip::gainDefaultPosition() const
{
    return gainPositionForValue (
        gainPresentation.isValid() ? gainPresentation.defaultValue : 0.0f);
}

void MixScopeStrip::setGainValue (const float value)
{
    if (! std::isfinite (value)) { return; }
    displayedGainValue = value;
    gain->setValue (
        gainPositionForValue (value), juce::dontSendNotification);
    if (! gainEditor.isBeingEdited())
        gainEditor.setText (
            formatGainEditorText (value), juce::dontSendNotification);
    repaint (readoutBounds);
}

float MixScopeStrip::getGainValue() const noexcept
{
    return displayedGainValue;
}

float MixScopeStrip::gainValueForPosition (const float position) const
{
    return gainPresentation.isValid()
        ? gainPresentation.positionToValue (
              juce::jlimit (0.0f, 1.0f, position))
        : 0.0f;
}

float MixScopeStrip::gainPositionForValue (const float value) const
{
    if (! gainPresentation.isValid()) { return 0.0f; }
    return juce::jlimit (
        0.0f, 1.0f, gainPresentation.valueToPosition (value));
}

juce::String MixScopeStrip::getGainReadoutText() const
{
    if (gainMixed) { return "Misto"; }
    return gainPresentation.isValid()
        ? gainPresentation.formatReadoutText (displayedGainValue)
        : juce::String {};
}

void MixScopeStrip::setGainMixed (const bool mixed)
{
    if (gainMixed == mixed) { return; }
    gainMixed = mixed;
    gain->setTooltip (mixed ? "Misto: thumb provvisorio. Un edit assegna tutti i canali attivi."
                            : gainPresentation.tooltip);
    setGainValue (displayedGainValue);
    gain->repaint();
}

void MixScopeStrip::setPanDisplayCodec (
    std::function<juce::String (float)> format,
    std::function<bool (const juce::String&, float&)> parse,
    juce::String start, juce::String end,
    const std::uint64_t generation, const int displayIdentity)
{
    if (! format || ! parse || generation != panContextGeneration || displayIdentity != panDisplayIdentity
        || start != panStartLabel || end != panEndLabel)
        pan->invalidateTextEdit();
    panContextGeneration = generation;
    panDisplayIdentity = displayIdentity;
    panFormatter = std::move (format);
    panParser = std::move (parse);
    panStartLabel = std::move (start);
    panEndLabel = std::move (end);
    pan->repaint();
}

void MixScopeStrip::setGainDb (const float value)
{
    setGainValue (value);
}

void MixScopeStrip::setPan (const float value)
{
    const float next = juce::jlimit (-1.0f, 1.0f, value);
    if (next != static_cast<float> (pan->getValue())) { pan->invalidateTextEdit(); }
    pan->setValue (next, juce::dontSendNotification);
}

void MixScopeStrip::setPrimaryToggle (const bool value)
{
    primaryToggle.setToggleState (value, juce::dontSendNotification);
}

float MixScopeStrip::getGainDb() const noexcept
{
    return getGainValue();
}
float MixScopeStrip::getPan() const noexcept { return (float) pan->getValue(); }
bool MixScopeStrip::getPrimaryToggle() const noexcept { return primaryToggle.getToggleState(); }

void MixScopeStrip::setPrimaryToggleLabel (juce::String label,
                                            const juce::Colour activeColour)
{
    primaryToggle.setName (label);
    primaryToggle.setLetter (std::move (label), activeColour);
}

void MixScopeStrip::setGainTooltip (juce::String tooltip)
{
    gain->setTooltip (std::move (tooltip));
}

void MixScopeStrip::setPanTooltip (juce::String tooltip)
{
    pan->setTooltip (tooltip);
    panEditorButton->setTooltip (std::move (tooltip));
}

void MixScopeStrip::setSpatialPanPreview (
    const juce::Point<float> position, const bool circular)
{
    panEditorButton->setPreview (position, circular);
}

void MixScopeStrip::setSpatialPanMarkers (
    std::vector<PositionField2D::Marker> markers)
{
    panEditorButton->setMarkers (std::move (markers));
}

void MixScopeStrip::setPrimaryToggleTooltip (juce::String tooltip)
{
    primaryToggle.setTooltip (std::move (tooltip));
}

void MixScopeStrip::setPrimaryToggleVisible (const bool shouldBeVisible)
{
    if (primaryToggle.isVisible() == shouldBeVisible) { return; }
    primaryToggle.setVisible (shouldBeVisible);
    resized();
}

void MixScopeStrip::setRouteVisible (const bool shouldBeVisible)
{
    if (routeButton.isVisible() == shouldBeVisible) { return; }
    routeButton.setVisible (shouldBeVisible);
    resized();
}

void MixScopeStrip::setLayout (const Layout newLayout)
{
    if (layout == newLayout && gain->getSliderStyle()
            == (newLayout == Layout::tall ? juce::Slider::LinearVertical
                                           : juce::Slider::LinearHorizontal))
        return;

    layout = newLayout;
    gain->setSliderStyle (
        layout == Layout::tall ? juce::Slider::LinearVertical
                               : juce::Slider::LinearHorizontal);
    const auto meterOrientation =
        layout == Layout::tall ? AudioMeterComponent::Orientation::vertical
                               : AudioMeterComponent::Orientation::horizontal;
    inputMeter.meter.setOrientation (meterOrientation);
    outputMeter.meter.setOrientation (meterOrientation);
    resized();
    repaint();
    notifyPreferredSizeChangedIfNeeded();
}

int MixScopeStrip::meterBankExtent (const MeterBank bank) const noexcept
{
    const auto& view = meterBankView (bank);
    if (! view.viewport.isVisible()) { return 0; }
    return meterBankPadding * 2
         + juce::jmax (1, view.meter.getNumChannels())
             * minimumMeterChannelExtent;
}

int MixScopeStrip::getPreferredWidthForLayout (
    const Layout requestedLayout) const noexcept
{
    if (requestedLayout != Layout::tall) { return minimumWideWidth; }
    int bankWidth = 0;
    int bankCount = 0;
    for (const auto bank : { MeterBank::input, MeterBank::output })
    {
        const int extent = meterBankExtent (bank);
        if (extent <= 0) { continue; }
        bankWidth += extent;
        ++bankCount;
    }
    if (bankCount == 0) { return preferredWidth; }
    constexpr int outerPadding = 14;
    constexpr int centreWidth = 92;
    return juce::jmax (preferredWidth,
        outerPadding + centreWidth + bankWidth + bankCount * meterBankGap);
}

int MixScopeStrip::getPreferredHeightForLayout (
    const Layout requestedLayout) const noexcept
{
    if (requestedLayout == Layout::tall) { return minimumTallHeight; }
    int height = minimumWideHeight;
    int visibleBanks = 0;
    for (const auto bank : { MeterBank::input, MeterBank::output })
    {
        const int extent = meterBankExtent (bank);
        if (extent <= 0) { continue; }
        height += juce::jmax (0, extent - 28);
        ++visibleBanks;
    }
    if (visibleBanks > 1)
        height += 28 + meterBankGap;
    return height;
}

void MixScopeStrip::notifyPreferredSizeChangedIfNeeded()
{
    const int width = getPreferredWidthForLayout (layout);
    const int height = getPreferredHeightForLayout (layout);
    if (width == lastPreferredWidth && height == lastPreferredHeight) { return; }
    lastPreferredWidth = width;
    lastPreferredHeight = height;
    if (onPreferredSizeChanged) { onPreferredSizeChanged(); }
}

void MixScopeStrip::setHeaderStyle (const HeaderStyle newStyle)
{
    if (headerStyle == newStyle) { return; }
    headerStyle = newStyle;
    resized();
    repaint();
}

void MixScopeStrip::setHeaderGlyph (const glyphs::Glyph& glyph)
{
    headerGlyph = glyph;
    resized();
    repaint (headerBounds);
}

void MixScopeStrip::addListener (Listener* listener) { listeners.add (listener); }
void MixScopeStrip::removeListener (Listener* listener) { listeners.remove (listener); }

void MixScopeStrip::notifyRouteRequested()
{
    const juce::Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& listener)
    {
        listener.mixScopeRouteRequested (this);
    });
    if (checker.shouldBailOut()) { return; }

    const auto routingRequested = onRoutingRequested;
    if (routingRequested) { routingRequested(); }
}

void MixScopeStrip::notifySettingsRequested()
{
    const juce::Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& listener)
    {
        listener.mixScopeSettingsRequested (this);
    });
    if (checker.shouldBailOut()) { return; }

    const auto settingsRequested = onSettingsRequested;
    if (settingsRequested) { settingsRequested(); }
}

void MixScopeStrip::notifyPanEditorRequested()
{
    const juce::Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& listener)
    {
        listener.mixScopePanEditorRequested (this);
    });
    if (checker.shouldBailOut()) { return; }

    const auto requested = onPanEditorRequested;
    if (requested) { requested(); }
}

void MixScopeStrip::paint (juce::Graphics& g)
{
    auto& laf = getLAF();
    laf.drawMixScopeStripBackground (g, getWidth(), getHeight(), *this);
    if (selected)
    {
        g.setColour (tokens::accent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), tokens::radius::card, 1.5f);
    }
    laf.drawMixScopeStripHeader (g, headerBounds, *this);
    laf.drawMixScopeStripControlLabels (
        g, gainSectionVisible ? gainBounds : juce::Rectangle<int> {},
        panBounds, *this);
    if (gainSectionVisible && ! gainEditor.isBeingEdited())
        laf.drawMixScopeStripReadout (g, readoutBounds, *this);
}

void MixScopeStrip::resized()
{
    clearMeterBankBounds();
    auto area = getLocalBounds().reduced (7);
    if (cues != nullptr)
    {
        const int reservedBody = layout == Layout::tall
            ? 206 + (accessory != nullptr ? accessoryHeight + 4 : 0)
            : 80 + (accessory != nullptr ? accessoryHeight + 4 : 0);
        const int height = juce::jlimit (0, juce::jmax (0, area.getHeight() - reservedBody), cuesHeight);
        cues->setBounds (area.removeFromTop (height));
        if (height > 0) { area.removeFromTop (4); }
    }

    headerBounds = area.removeFromTop (22);
    auto headerContent = headerBounds;
    if (layout == Layout::wide)
    {
        readoutBounds = headerContent.removeFromRight (54).withSizeKeepingCentre (54, 16);
        headerContent.removeFromRight (4);
    }
    // Graph: stessa posizione nel cap per qualunque consumer.
    graphButton.setBounds (headerContent.removeFromRight (22).withSizeKeepingCentre (18, 18));
    if (layout == Layout::wide && settingsButton.isVisible())
    {
        settingsButton.setBounds (
            headerContent.removeFromRight (24)
                .withSizeKeepingCentre (20, 20));
        headerContent.removeFromRight (4);
    }
    if (layout == Layout::wide && routeButton.isVisible())
    {
        routeButton.setBounds (
            headerContent.removeFromRight (24)
                .withSizeKeepingCentre (20, 20));
        headerContent.removeFromRight (4);
    }
    headerGlyphBounds = {};
    if (headerLeading != nullptr)
    {
        headerContent.removeFromLeft (4);
        headerLeading->setBounds (headerContent.removeFromLeft (headerLeadingWidth).withSizeKeepingCentre (18, 18));
        headerContent.removeFromLeft (4);
    }
    else if (headerGlyph.isValid() || headerStyle == HeaderStyle::bus)
    {
        headerContent.removeFromLeft (5);
        headerGlyphBounds = headerContent.removeFromLeft (18)
            .withSizeKeepingCentre (14, 14);
        headerContent.removeFromLeft (3);
    }
    titleBounds = headerContent;
    area.removeFromTop (4);

    if (layout == Layout::tall)
    {
        auto controls = area;
        if (inputMeter.viewport.isVisible())
        {
            const int width = meterBankExtent (MeterBank::input);
            layoutMeterBank (MeterBank::input,
                             controls.removeFromLeft (width));
            controls.removeFromLeft (meterBankGap);
        }
        if (outputMeter.viewport.isVisible())
        {
            const int width = meterBankExtent (MeterBank::output);
            layoutMeterBank (MeterBank::output,
                             controls.removeFromRight (width));
            controls.removeFromRight (meterBankGap);
        }

        auto panRow = controls.removeFromTop (42);
        auto toggleColumn = panRow.removeFromRight (44);
        panRow.removeFromRight (3);
        auto toggles = toggleColumn.withSizeKeepingCentre (40, 40);
        auto topToggles = toggles.removeFromTop (18);
        primaryToggle.setBounds (topToggles.removeFromLeft (18));
        topToggles.removeFromLeft (4);
        soloToggle.setBounds (topToggles);
        toggles.removeFromTop (4);
        auto bottomToggles = toggles.removeFromTop (18);
        if (routeButton.isVisible())
            routeButton.setBounds (bottomToggles.removeFromLeft (18));
        else
            bottomToggles.removeFromLeft (18);
        bottomToggles.removeFromLeft (4);
        if (settingsButton.isVisible())
            settingsButton.setBounds (bottomToggles);

        panBounds = {};
        pan->setBounds ({});
        panEditorButton->setBounds ({});
        if (hasPanAffordance())
        {
            panBounds = panRow.withSizeKeepingCentre (42, 42);
            if (pan->isVisible())
                pan->setBounds (panBounds);
            else
                panEditorButton->setBounds (
                    panBounds.withSizeKeepingCentre (38, 38));
        }
        controls.removeFromTop (4);
        readoutBounds = controls.removeFromBottom (16);
        controls.removeFromBottom (2);
        if (accessory != nullptr)
        {
            accessory->setBounds (controls.removeFromTop (accessoryHeight));
            controls.removeFromTop (4);
        }
        area = controls;
    }
    else
    {
        // Gli extra hanno una riga propria: non cambiano l'ordine gain-pan-M.
        if (accessory != nullptr)
        {
            accessory->setBounds (area.removeFromBottom (accessoryHeight));
            area.removeFromBottom (4);
        }
        if (inputMeter.viewport.isVisible())
        {
            const int height = meterBankExtent (MeterBank::input);
            layoutMeterBank (MeterBank::input,
                             area.removeFromTop (height));
            area.removeFromTop (meterBankGap);
        }
        if (outputMeter.viewport.isVisible())
        {
            const int height = meterBankExtent (MeterBank::output);
            layoutMeterBank (MeterBank::output,
                             area.removeFromBottom (height));
            area.removeFromBottom (meterBankGap);
        }
        auto toggles = area.removeFromRight (18);
        toggles = toggles.withSizeKeepingCentre (18, juce::jmin (40, toggles.getHeight()));
        primaryToggle.setBounds (toggles.removeFromTop (18));
        toggles.removeFromTop (4);
        soloToggle.setBounds (toggles.removeFromTop (18));
        area.removeFromRight (4);
        panBounds = {};
        pan->setBounds ({});
        panEditorButton->setBounds ({});
        if (hasPanAffordance())
        {
            panBounds = area.removeFromRight (46).withSizeKeepingCentre (
                42, juce::jmin (42, area.getHeight()));
            if (pan->isVisible())
                pan->setBounds (panBounds);
            else
                panEditorButton->setBounds (
                    panBounds.withSizeKeepingCentre (38, 38));
            area.removeFromRight (6);
        }
    }
    gainEditor.setBounds (gainSectionVisible ? readoutBounds
                                             : juce::Rectangle<int> {});
    gainBounds = gainSectionVisible ? area : juce::Rectangle<int> {};
    gain->setBounds (gainBounds);
}

void MixScopeStrip::setGainVisible (const bool visible)
{
    if (gainSectionVisible == visible) { return; }
    gainSectionVisible = visible;
    gain->setVisible (visible);
    gainEditor.setVisible (visible);
    resized();
    repaint();
}

void MixScopeStrip::setPanVisible (const bool visible)
{
    if (panSectionVisible == visible) { return; }
    panSectionVisible = visible;
    updatePanPresentationVisibility();
    resized();
}

void MixScopeStrip::setPanPresentation (
    const PanPresentation presentation)
{
    if (panPresentation == presentation) { return; }
    panPresentation = presentation;
    updatePanPresentationVisibility();
    resized();
    repaint (panBounds);
}

bool MixScopeStrip::hasPanAffordance() const noexcept
{
    return pan->isVisible() || panEditorButton->isVisible();
}

void MixScopeStrip::updatePanPresentationVisibility()
{
    const bool scalar = panSectionVisible
                     && panPresentation == PanPresentation::scalar;
    const bool editor = panSectionVisible
                     && panPresentation == PanPresentation::editor;
    pan->setVisible (scalar);
    panEditorButton->setVisible (editor);
    pan->setEnabled (panControlEnabled);
    panEditorButton->setEnabled (panControlEnabled);
}

void MixScopeStrip::setSettingsVisible (bool visible)
{
    if (settingsButton.isVisible() == visible) { return; }
    settingsButton.setVisible (visible);
    resized();
}

void MixScopeStrip::setSoloVisible (bool visible)
{
    if (soloToggle.isVisible() == visible) { return; }
    soloToggle.setVisible (visible);
    resized();
}
void MixScopeStrip::setControlsEnabled (bool gainEnabled, bool panEnabled)
{
    // Non usare isEnabled() come no-op guard: JUCE restituisce false anche
    // quando e' disabilitato un parent. Dobbiamo comunque scrivere lo stato
    // LOCALE richiesto, cosi' resta corretto quando il parent viene riabilitato.
    gain->setEnabled (gainEnabled);
    gainEditor.setEnabled (gainEnabled);
    panControlEnabled = panEnabled;
    pan->setEnabled (panEnabled);
    panEditorButton->setEnabled (panEnabled);
    repaint (readoutBounds);
}
void MixScopeStrip::setSelected (bool value)
{
    if (selected == value) { return; }
    selected = value;
    repaint();
}

void MixScopeStrip::setInaudible (bool value)
{
    if (inaudible == value) { return; }
    inaudible = value;
    gain->setAlpha (inaudible ? 0.45f : 1.0f);
    pan->setAlpha (inaudible ? 0.45f : 1.0f);
    panEditorButton->setAlpha (inaudible ? 0.45f : 1.0f);
}
void MixScopeStrip::setAccessoryComponent (juce::Component* component, int height)
{
    const int nextHeight = juce::jmax (0, height);
    if (accessory == component && accessoryHeight == nextHeight) { return; }

    if (accessory != component)
    {
        if (accessory != nullptr) { removeChildComponent (accessory); }
        accessory = component;
        if (accessory != nullptr) { addAndMakeVisible (accessory); }
    }
    accessoryHeight = nextHeight;
    resized();
}
void MixScopeStrip::setHeaderLeadingComponent (juce::Component* component, int width)
{
    const int nextWidth = juce::jmax (0, width);
    if (headerLeading == component && headerLeadingWidth == nextWidth) { return; }

    if (headerLeading != component)
    {
        if (headerLeading != nullptr) { removeChildComponent (headerLeading); }
        headerLeading = component;
        if (headerLeading != nullptr) { addAndMakeVisible (headerLeading); }
    }
    headerLeadingWidth = nextWidth;
    resized();
}
void MixScopeStrip::setCueComponent (juce::Component* component, int height)
{
    const int nextHeight = juce::jmax (0, height);
    if (cues == component && cuesHeight == nextHeight) { return; }

    if (cues != component)
    {
        if (cues != nullptr) { removeChildComponent (cues); }
        cues = component;
        if (cues != nullptr) { addAndMakeVisible (cues); }
    }
    cuesHeight = nextHeight;
    resized();
}
float MixScopeStrip::getGainPosition() const noexcept { return (float) gain->getValue(); }
juce::Slider& MixScopeStrip::getSlider (Control control) noexcept
{
    jassert (control != Control::primaryToggle);
    return control == Control::gain ? *gain : *pan;
}
void MixScopeStrip::mouseDown (const juce::MouseEvent& event)
{
    listeners.call ([this, &event] (Listener& listener) { listener.mixScopeBackgroundPressed (this, event); });
    if (onBackgroundPressed) { onBackgroundPressed (event); }
}
void MixScopeStrip::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! headerBounds.contains (event.getPosition())) { return; }
    listeners.call ([this] (Listener& listener) { listener.mixScopeTitleEditRequested (this); });
    if (onTitleEditRequested) { onTitleEditRequested(); }
}

void MixScopeStrip::setGraphVisible (const bool visible)
{
    if (graphButton.isVisible() == visible) { return; }
    graphButton.setVisible (visible);
    resized();
}
void MixScopeStrip::setGraphEnabled (const bool enabled, juce::String tooltip)
{
    if (graphButton.isEnabled() != enabled) { graphButton.setEnabled (enabled); }
    if (graphButton.getTooltip() != tooltip) { graphButton.setTooltip (std::move (tooltip)); }
}
void MixScopeStrip::setMeterVisible (const bool visible)
{
    if (meterSectionVisible == visible) { return; }
    meterSectionVisible = visible;
    updateMeterBankVisibility();
    resized();
}

void MixScopeStrip::setMeterBankVisible (
    const MeterBank bank, const bool visible)
{
    auto& view = meterBankView (bank);
    if (view.enabled == visible) { return; }
    view.enabled = visible;
    updateMeterBankVisibility();
    resized();
}

bool MixScopeStrip::isMeterBankVisible (
    const MeterBank bank) const noexcept
{
    return meterBankView (bank).viewport.isVisible();
}

void MixScopeStrip::attachMeterSource (
    const MeterBank bank, AudioLevelMeterSource<float>* source)
{
    auto& view = meterBankView (bank);
    if (view.source == source)
    {
        updateMeterBankVisibility();
        return;
    }

    view.source = source;
    view.layoutIdentifier.clear();
    view.channelLabels.clear();
    view.summary.setText ({}, juce::dontSendNotification);
    view.summary.setTooltip ({});
    view.meter.attachSource (source);
    updateMeterBankVisibility();
    syncMeterBankMetadata (bank);
    resized();
}

void MixScopeStrip::detachMeterSource (const MeterBank bank)
{
    attachMeterSource (bank, nullptr);
}

void MixScopeStrip::detachMeterSources()
{
    attachMeterSource (MeterBank::input, nullptr);
    attachMeterSource (MeterBank::output, nullptr);
}

AudioMeterComponent& MixScopeStrip::getMeterComponent (
    const MeterBank bank) noexcept
{
    return meterBankView (bank).meter;
}

const AudioMeterComponent& MixScopeStrip::getMeterComponent (
    const MeterBank bank) const noexcept
{
    return meterBankView (bank).meter;
}

int MixScopeStrip::getMeterBankNumChannels (
    const MeterBank bank) const noexcept
{
    return meterBankView (bank).meter.getNumChannels();
}

juce::String MixScopeStrip::getMeterBankChannelLabel (
    const MeterBank bank, const int channel) const
{
    const auto& labels = meterBankView (bank).channelLabels;
    return juce::isPositiveAndBelow (channel, labels.size())
        ? labels[channel] : juce::String {};
}

const juce::String& MixScopeStrip::getMeterBankLayoutIdentifier (
    const MeterBank bank) const noexcept
{
    return meterBankView (bank).layoutIdentifier;
}

MixScopeStrip::MeterBankView& MixScopeStrip::meterBankView (
    const MeterBank bank) noexcept
{
    return bank == MeterBank::input ? inputMeter : outputMeter;
}

const MixScopeStrip::MeterBankView& MixScopeStrip::meterBankView (
    const MeterBank bank) const noexcept
{
    return bank == MeterBank::input ? inputMeter : outputMeter;
}

void MixScopeStrip::syncMeterBankMetadata (const MeterBank bank)
{
    auto& view = meterBankView (bank);
    if (view.source == nullptr)
    {
        view.meter.setNumChannels (0);
        view.layoutIdentifier.clear();
        view.channelLabels.clear();
        view.summary.setText ({}, juce::dontSendNotification);
        view.summary.setTooltip (juce::String {});
        notifyPreferredSizeChangedIfNeeded();
        return;
    }

    // Suspend/offscreen rilascia la lease, ma non cambia la source associata:
    // conserva quindi gli ultimi metadata della stessa generation finche' non
    // arriva un rebind semantico o un detach esplicito.
    if (view.meter.getObservedSourceGenerationId() == 0u)
        return;

    view.layoutIdentifier.clear();
    view.channelLabels.clear();
    if (const auto* const layout =
            view.meter.getObservedChannelLayout())
        view.layoutIdentifier = layout->identifier();

    const int channels = view.meter.getNumChannels();
    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* const descriptor =
            view.meter.getObservedChannelDescriptor (channel);
        juce::String label;
        if (descriptor != nullptr)
        {
            label = descriptor->label.trim();
            if (label.isEmpty())
                label = descriptor->identifier.trim();
        }
        if (label.isEmpty())
            label = juce::String (channel + 1);
        view.channelLabels.add (label);
    }

    juce::StringArray indexedLabels;
    for (int channel = 0; channel < view.channelLabels.size(); ++channel)
        indexedLabels.add (
            juce::String (channel + 1) + ":" + view.channelLabels[channel]);

    const auto separator = juce::String::charToString (0x00b7);
    const auto summary = view.roleLabel + " " + separator + " "
                       + juce::String (view.channelLabels.size());
    view.summary.setText (summary, juce::dontSendNotification);

    juce::String tooltip = view.roleLabel + " meter";
    if (view.layoutIdentifier.isNotEmpty())
        tooltip += " [" + view.layoutIdentifier + "]";
    if (indexedLabels.isEmpty())
        tooltip += "\nNo channels";
    else
        for (const auto& label : indexedLabels)
            tooltip += "\n" + label;
    view.summary.setTooltip (tooltip);
    notifyPreferredSizeChangedIfNeeded();
}

void MixScopeStrip::updateMeterBankVisibility()
{
    const auto update = [this] (MeterBankView& view)
    {
        const bool visible = meterSectionVisible
                          && view.enabled
                          && view.source != nullptr;
        view.summary.setVisible (visible);
        view.viewport.setVisible (visible);
        view.meter.setVisible (true);
        // Refresh anche in hide: l'AudioMeterComponent deve osservare subito
        // l'ancestor nascosto e rilasciare la ReaderLease, non al tick timer.
        view.meter.refreshSourceNow();
    };

    update (inputMeter);
    update (outputMeter);
    notifyPreferredSizeChangedIfNeeded();
}

void MixScopeStrip::clearMeterBankBounds()
{
    inputMeter.viewport.setBounds ({});
    inputMeter.summary.setBounds ({});
    outputMeter.viewport.setBounds ({});
    outputMeter.summary.setBounds ({});
}

void MixScopeStrip::layoutMeterBank (
    const MeterBank bank, juce::Rectangle<int> bounds)
{
    auto& view = meterBankView (bank);
    if (! view.viewport.isVisible() || bounds.isEmpty()) { return; }
    bounds = layout == Layout::tall
        ? bounds.reduced (meterBankPadding, 0)
        : bounds.reduced (0, meterBankPadding);
    if (bounds.isEmpty()) { return; }

    if (layout == Layout::tall)
    {
        view.summary.setBounds (
            bounds.removeFromTop (juce::jmin (14, bounds.getHeight())));
        view.viewport.setBounds (bounds);
        view.viewport.setScrollBarsShown (false, false, false, false);
        view.meter.setBounds (0, 0, juce::jmax (1, bounds.getWidth()),
                              juce::jmax (1, bounds.getHeight()));
        return;
    }

    view.summary.setBounds (
        bounds.removeFromLeft (juce::jmin (48, bounds.getWidth())));
    view.viewport.setBounds (bounds);
    view.viewport.setScrollBarsShown (false, false, false, false);
    view.meter.setBounds (0, 0, juce::jmax (1, bounds.getWidth()),
                          juce::jmax (1, bounds.getHeight()));
}

MixScopeStrip::LookAndFeelMethods& MixScopeStrip::getLAF()
{
    if (auto* look = dynamic_cast<LookAndFeelMethods*> (&getLookAndFeel()))
        return *look;
    static LookAndFeelMethods fallback;
    return fallback;
}

void MixScopeStrip::notifyChanged (const Control control)
{
    listeners.call ([this, control] (Listener& listener)
    {
        listener.mixScopeControlChanged (this, control);
    });
    if (onControlChanged) { onControlChanged (control); }
}

void MixScopeStrip::notifyGestureStart (const Control control, const GestureMode mode)
{
    gestureMode = mode;
    if (control == Control::gain) { gainGestureActive = true; }
    if (control == Control::pan) { panGestureActive = true; }
    listeners.call ([this, control] (Listener& listener)
    {
        listener.mixScopeGestureStarted (this, control);
    });
    if (onGestureStart) { onGestureStart (control); }
}

void MixScopeStrip::notifyGestureEnd (const Control control)
{
    if (control == Control::gain) { gainGestureActive = false; }
    if (control == Control::pan) { panGestureActive = false; }
    listeners.call ([this, control] (Listener& listener)
    {
        listener.mixScopeGestureEnded (this, control);
    });
    if (onGestureEnd) { onGestureEnd (control); }
}

} // namespace ayra
