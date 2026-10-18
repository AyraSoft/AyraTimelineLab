/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / DragValueField (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (token gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
DragValueField::DragValueField()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Drag vertically, scroll, or double-click to type; Shift snaps to integers");

    // Editor da tastiera (doppio click): stessa impostazione di MixerPanel::Entry.
    editor.setJustificationType (juce::Justification::centred);
    editor.setFont (valueFont);
    editor.setColour (juce::Label::backgroundColourId,            juce::Colours::transparentBlack);
    editor.setEditable (false, false, false);
    editor.onTextChange  = [this] { commitTypeEdit(); };
    editor.onEditorHide  = [this]
    {
        editor.setVisible (false);
        repaint();
        if (onEditEnd) { onEditEnd(); }
    };
    addChildComponent (editor);
}

DragValueField::~DragValueField() = default;

//==============================================================================
void DragValueField::setRange (double newMin, double newMax, double newInterval) noexcept
{
    minValue = juce::jmin (newMin, newMax);
    maxValue = juce::jmax (newMin, newMax);
    interval = juce::jmax (0.0, newInterval);
    allowedValues.clearQuick();
    apply (value, juce::dontSendNotification);
}

void DragValueField::setAllowedValues (juce::Array<double> values)
{
    allowedValues = std::move (values);
    apply (value, juce::dontSendNotification);
}

void DragValueField::setSensitivity (double perPixel) noexcept { unitsPerPixel = juce::jmax (1.0e-6, perPixel); }
void DragValueField::setStepPixels (int px) noexcept           { stepPixels = juce::jmax (1, px); }
void DragValueField::setDecimalPlaces (int places) noexcept
{
    if (decimalPlaces == places) { return; }
    decimalPlaces = places;
    repaint();
}
void DragValueField::setCaption (const juce::String& t)
{
    if (caption == t) { return; }
    caption = t;
    resized();
    repaint();
}
void DragValueField::setSuffix (const juce::String& t)
{
    if (suffix == t) { return; }
    suffix = t;
    repaint();
}
void DragValueField::setDisplayFormatter (std::function<juce::String (double)> fn) { displayFormatter = std::move (fn); repaint(); }
void DragValueField::setTextParser (std::function<bool (const juce::String&, double&)> fn) { textParser = std::move (fn); }

void DragValueField::addListener (Listener* l)    { listeners.add (l); }
void DragValueField::removeListener (Listener* l) { listeners.remove (l); }

//==============================================================================
double DragValueField::clampValue (double v) const noexcept
{
    if (! allowedValues.isEmpty()) { return allowedValues[nearestAllowedIndex (v)]; }

    v = juce::jlimit (minValue, maxValue, v);
    if (interval > 0.0)
    {
        v = minValue + std::round ((v - minValue) / interval) * interval;
        v = juce::jlimit (minValue, maxValue, v);
    }
    return v;
}

int DragValueField::nearestAllowedIndex (double v) const noexcept
{
    int best = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 0; i < allowedValues.size(); ++i)
    {
        const double d = std::abs (allowedValues[i] - v);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

juce::String DragValueField::textForValue (double v) const
{
    if (displayFormatter) { return displayFormatter (v); }
    juce::String s = (decimalPlaces < 0) ? juce::String (juce::roundToInt (v))
                                         : juce::String (v, decimalPlaces);
    return s + suffix;
}

bool DragValueField::parseText (const juce::String& text, double& out) const
{
    if (textParser) { return textParser (text, out); }

    auto t = text.trim();
    if (! suffix.isEmpty() && t.endsWithIgnoreCase (suffix.trim()))
        t = t.dropLastCharacters (suffix.trim().length()).trim();
    if (t.isEmpty()) { return false; }

    const juce::juce_wchar c0 = t[0];
    if (! (juce::CharacterFunctions::isDigit (c0) || c0 == '-' || c0 == '+' || c0 == '.'))
        return false;

    out = t.getDoubleValue();
    return true;
}

//==============================================================================
void DragValueField::apply (double newValue, juce::NotificationType notify)
{
    newValue = clampValue (newValue);
    const bool changed = ! juce::approximatelyEqual (newValue, value);
    if (! changed) { return; }
    value = newValue;
    repaint();

    if (notify != juce::dontSendNotification)
    {
        listeners.call ([this] (Listener& l) { l.dragValueFieldChanged (this, value); });
        if (onValueChange) { onValueChange (value); }
    }
}

void DragValueField::fireCommit()
{
    if (onValueCommit) { onValueCommit (value); }
}

void DragValueField::setValue (double newValue, juce::NotificationType notify)
{
    apply (newValue, notify);
}

//==============================================================================
void DragValueField::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (tokens::componentColour (*this, backgroundColourId, tokens::surfaceRecessed()));
    g.fillRoundedRectangle (b, 3.0f);

    if (dragging || hover)
    {
        g.setColour (tokens::componentColour (*this, highlightColourId, tokens::accent().withAlpha (0.28f)));
        g.fillRoundedRectangle (b, 3.0f);
    }

    g.setColour (tokens::componentColour (*this, outlineColourId, tokens::borderHairline()));
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    auto inner = b.reduced (4.0f, 2.0f);

    if (caption.isNotEmpty())
    {
        auto capArea = inner.removeFromTop (inner.getHeight() * 0.42f);
        g.setColour (tokens::componentColour (*this, captionColourId, tokens::textSecondary()));
        g.setFont (captionFont);
        g.drawText (caption, capArea.toNearestInt(), juce::Justification::centredLeft, false);
    }

    if (! editor.isVisible())
    {
        g.setColour (tokens::componentColour (*this, textColourId, tokens::textPrimary()));
        g.setFont (valueFont);
        g.drawText (textForValue (value), inner.toNearestInt(),
                    caption.isNotEmpty() ? juce::Justification::centredLeft
                                         : juce::Justification::centred, false);
    }
}

void DragValueField::resized()
{
    // L'editor copre la zona del valore (sotto la caption, se presente).
    auto inner = getLocalBounds().reduced (2);
    if (caption.isNotEmpty()) { inner.removeFromTop ((int) (inner.getHeight() * 0.40f)); }
    editor.setBounds (inner);
}

//==============================================================================
void DragValueField::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { return; }
    dragStartValue = value;
    dragStartIndex = nearestAllowedIndex (value);
    dragStartY     = e.y;
    dragMoved      = false;
}

void DragValueField::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { return; }

    if (! dragging)
    {
        if (e.getDistanceFromDragStart() < 3) { return; }
        dragging = true;
        if (onEditBegin) { onEditBegin(); }
    }
    dragMoved = true;

    if (! allowedValues.isEmpty())
    {
        // Modalita' a lista: scorre l'indice (`stepPixels` px per passo).
        const int steps = (dragStartY - e.y) / stepPixels;
        const int idx   = juce::jlimit (0, allowedValues.size() - 1, dragStartIndex + steps);
        apply (allowedValues[idx], juce::sendNotificationSync);
        return;
    }

    // Modalita' range: delta-di-presa in unita' di valore. Shift = snap a intero
    // (niente cifre decimali).
    double raw = dragStartValue + (double) (dragStartY - e.y) * unitsPerPixel;
    if (e.mods.isShiftDown()) { raw = std::round (raw); }
    apply (raw, juce::sendNotificationSync);
}

void DragValueField::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        repaint();
        if (dragMoved) { fireCommit(); }
        if (onEditEnd) { onEditEnd(); }
    }
    dragMoved = false;
}

void DragValueField::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { return; }
    beginTypeEdit();
}

void DragValueField::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) { return; }
    const int dir = wheel.deltaY > 0.0f ? 1 : -1;

    if (! allowedValues.isEmpty())
    {
        const int idx = juce::jlimit (0, allowedValues.size() - 1, nearestAllowedIndex (value) + dir);
        apply (allowedValues[idx], juce::sendNotificationSync);
    }
    else
    {
        const double step = (interval > 0.0) ? interval : 1.0;
        apply (value + dir * step, juce::sendNotificationSync);
    }
    fireCommit();
}

void DragValueField::mouseEnter (const juce::MouseEvent&) { hover = true;  repaint(); }
void DragValueField::mouseExit  (const juce::MouseEvent&) { hover = false; repaint(); }

//==============================================================================
void DragValueField::beginTypeEdit()
{
    if (editor.isVisible()) { return; }

    // Testo iniziale = numero grezzo (senza suffisso), cosi' e' facile ridigitarlo.
    juce::String plain;
    if (! allowedValues.isEmpty())
        plain = (std::floor (value) == value) ? juce::String ((int) value) : juce::String (value, 1);
    else if (decimalPlaces < 0)
        plain = juce::String (juce::roundToInt (value));
    else if (displayFormatter)
    {
        // Usa il display, ma togli il suffisso SOLO se c'e': `upToFirstOccurrenceOf`
        // con needle vuoto ritorna stringa VUOTA (bug: il campo si apriva vuoto).
        plain = displayFormatter (value);
        if (suffix.isNotEmpty()) { plain = plain.upToFirstOccurrenceOf (suffix.trim(), false, true).trim(); }
    }
    else
    {
        plain = juce::String (value, decimalPlaces);
    }

    editor.setText (plain, juce::dontSendNotification);
    editor.setVisible (true);
    if (onEditBegin) { onEditBegin(); }
    editor.showEditor();
    if (auto* ed = editor.getCurrentTextEditor()) { ed->selectAll(); }
    repaint();
}

void DragValueField::commitTypeEdit()
{
    double parsed = value;
    if (parseText (editor.getText(), parsed))
    {
        apply (parsed, juce::sendNotificationSync);
        fireCommit();
    }
    // Il teardown (setVisible(false) + onEditEnd) lo fa `onEditorHide`. Nota: in
    // juce::Label su Invio/focus-lost `onEditorHide` scatta PRIMA di `onTextChange`,
    // ma qui e' indifferente: `editor.getText()` porta gia' il valore committato.
}

} // namespace ayra
