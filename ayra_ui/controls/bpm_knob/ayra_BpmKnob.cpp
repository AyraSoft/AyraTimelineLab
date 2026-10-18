/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / BpmKnob (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (token gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

namespace
{
    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;   // 225 deg (basso-sx)
    constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;   // 495 deg (basso-dx) -> sweep 270
}

//==============================================================================
BpmKnob::BpmKnob()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    // Editor da tastiera del valore (doppio-click sul disco). A riposo il numero
    // lo disegna paint() -> testo del label trasparente (niente doppio disegno);
    // colori "editing" per il campo attivo. Figlio nascosto finche' non si edita.
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setColour (juce::Label::backgroundColourId,            juce::Colours::transparentBlack);
    valueLabel.setColour (juce::Label::textColourId,                  juce::Colours::transparentBlack);
    valueLabel.setEditable (false, false, false);
    valueLabel.onEditorHide = [this] { valueLabel.setVisible (false); repaint(); };
    valueLabel.onTextChange = [this]
    {
        // Applica solo se il testo INIZIA con cifra/segno/punto: getDoubleValue su
        // "abc5" darebbe 0 -> azzererebbe il knob al minimo. applyBpm clampa gia' al range.
        const auto t = valueLabel.getText().trim();
        if (t.isNotEmpty())
        {
            const auto c0 = t[0];
            if (juce::CharacterFunctions::isDigit (c0) || c0 == '-' || c0 == '+' || c0 == '.')
                applyBpm (t.getDoubleValue(), juce::sendNotificationSync);
        }
    };
    addChildComponent (valueLabel);
}

BpmKnob::~BpmKnob() = default;

//==============================================================================
void BpmKnob::setRange (double newMin, double newMax) noexcept
{
    const double nextMin = juce::jmin (newMin, newMax);
    const double nextMax = juce::jmax (newMin, newMax);
    if (juce::approximatelyEqual (minBpm, nextMin)
        && juce::approximatelyEqual (maxBpm, nextMax))
        return;

    minBpm = nextMin;
    maxBpm = nextMax;
    const double before = bpm;
    applyBpm (bpm, juce::dontSendNotification);
    if (juce::approximatelyEqual (before, bpm)) { repaint(); }
}

void BpmKnob::setBpm (double newBpm, juce::NotificationType notify)
{
    applyBpm (newBpm, notify);
}

void BpmKnob::addListener (Listener* l)    { listeners.add (l); }
void BpmKnob::removeListener (Listener* l) { listeners.remove (l); }

//==============================================================================
void BpmKnob::applyBpm (double newBpm, juce::NotificationType notify)
{
    newBpm = juce::jlimit (minBpm, maxBpm, newBpm);

    const bool changed = ! juce::approximatelyEqual (newBpm, bpm);
    if (! changed) { return; }
    bpm = newBpm;
    repaint();

    if (notify != juce::dontSendNotification)
    {
        listeners.call ([this] (Listener& l) { l.bpmKnobValueChanged (this, bpm); });
        if (onBpmChanged) { onBpmChanged (bpm); }
    }
}

double BpmKnob::proportion() const noexcept
{
    return maxBpm > minBpm ? (bpm - minBpm) / (maxBpm - minBpm) : 0.0;
}

//==============================================================================
juce::Rectangle<float> BpmKnob::knobDisc() const noexcept
{
    return StripKnobGeometry::bodyBounds (getLocalBounds().toFloat());
}

juce::Rectangle<float> BpmKnob::readoutBounds() const noexcept
{
    return StripKnobGeometry::readoutBounds (getLocalBounds().toFloat());
}

//==============================================================================
void BpmKnob::paint (juce::Graphics& g)
{
    auto        knob   = knobDisc();
    RotaryKnobStyle knobStyle;
    knobStyle.wellColour = tokens::componentColour (*this, backgroundColourId, tokens::surfaceRecessed());
    knobStyle.trackColour = tokens::componentColour (*this, arcBackgroundColourId, tokens::borderHairline());
    knobStyle.valueColour = getCommonLAF (*this).accentOr (tokens::componentColour (*this, arcColourId, tokens::accent()));
    knobStyle.glowColour = knobStyle.valueColour.withAlpha (0.30f);
    knobStyle.pointerColour = tokens::componentColour (*this, valueTextColourId, tokens::textPrimary());
    drawRotaryKnob (g, knob.reduced (1.5f), (float) proportion(), kStartAngle, kEndAngle,
                    0.0f, knobStyle, isEnabled(), isMouseOverOrDragging(),
                    isMouseButtonDown());

    // Readout inferiore come il Panpot: valore + unita' in una sola riga.
    if (! valueLabel.isVisible())
    {
        g.setColour (tokens::componentColour (*this, valueTextColourId, tokens::textPrimary()));
        g.setFont (tokens::type::value().withHeight (8.5f));
        auto text = juce::String (juce::roundToInt (bpm));
        if (caption.isNotEmpty()) { text += " " + caption; }
        g.drawText (text, readoutBounds(), juce::Justification::centred, false);
    }
}

//==============================================================================
void BpmKnob::mouseDown (const juce::MouseEvent& e)
{
    dragStartBpm = bpm;
    dragStartY   = e.y;
    repaint (knobDisc().toNearestInt().expanded (2));
}

void BpmKnob::mouseDrag (const juce::MouseEvent& e)
{
    // su = piu' veloce. Shift = fine (0.1 BPM/px), altrimenti 0.5 BPM/px.
    const double perPixel = e.mods.isShiftDown() ? 0.1 : 0.5;
    applyBpm (dragStartBpm + (double) (dragStartY - e.y) * perPixel, juce::sendNotificationSync);
}

void BpmKnob::mouseUp (const juce::MouseEvent&)
{
    repaint (knobDisc().toNearestInt().expanded (2));
}

void BpmKnob::mouseEnter (const juce::MouseEvent&)
{
    repaint (knobDisc().toNearestInt().expanded (2));
}

void BpmKnob::mouseExit (const juce::MouseEvent&)
{
    repaint (knobDisc().toNearestInt().expanded (2));
}

void BpmKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) { return; }
    applyBpm (bpm + (-wheel.deltaY > 0.0f ? 1.0 : -1.0), juce::sendNotificationSync);
}

void BpmKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (readoutBounds().contains (e.position)) { beginValueEdit(); return; }

    applyBpm (defaultBpm, juce::sendNotificationSync);
}

void BpmKnob::beginValueEdit()
{
    valueLabel.setBounds (readoutBounds().expanded (1.0f, 2.0f).toNearestInt());
    valueLabel.setFont (tokens::type::value().withHeight (9.0f));
    valueLabel.setText (juce::String (juce::roundToInt (bpm)), juce::dontSendNotification);
    valueLabel.setVisible (true);
    valueLabel.showEditor();
    if (auto* ed = valueLabel.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
    repaint();
}

} // namespace ayra
