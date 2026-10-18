/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / TimeSignatureControl (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (token gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

namespace
{
    constexpr int kDenoms[]   = { 1, 2, 4, 8, 16, 32 };
    constexpr int kNumDenoms  = (int) (sizeof (kDenoms) / sizeof (kDenoms[0]));
    constexpr int kMinNum     = 1;
    constexpr int kMaxNum     = 32;
}

//==============================================================================
TimeSignatureControl::TimeSignatureControl()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

TimeSignatureControl::~TimeSignatureControl() = default;

//==============================================================================
int TimeSignatureControl::denominatorIndex (int denom) noexcept
{
    for (int i = 0; i < kNumDenoms; ++i) { if (kDenoms[i] == denom) { return i; } }
    return 2;   // fallback = 4
}

void TimeSignatureControl::setTimeSignature (int n, int d, juce::NotificationType notify)
{
    apply (n, d, notify);
}

void TimeSignatureControl::addListener (Listener* l)    { listeners.add (l); }
void TimeSignatureControl::removeListener (Listener* l) { listeners.remove (l); }

//==============================================================================
void TimeSignatureControl::apply (int n, int d, juce::NotificationType notify)
{
    n = juce::jlimit (kMinNum, kMaxNum, n);
    d = kDenoms[juce::jlimit (0, kNumDenoms - 1, denominatorIndex (d))];

    const bool changed = (n != numerator) || (d != denominator);
    if (! changed) { return; }
    numerator = n;
    denominator = d;
    repaint();

    if (notify != juce::dontSendNotification)
    {
        listeners.call ([this] (Listener& l) { l.timeSignatureChanged (this, numerator, denominator); });
        if (onTimeSignatureChanged) { onTimeSignatureChanged (numerator, denominator); }
    }
}

void TimeSignatureControl::bumpNumerator (int delta, juce::NotificationType notify)
{
    apply (numerator + delta, denominator, notify);
}

void TimeSignatureControl::bumpDenominator (int direction, juce::NotificationType notify)
{
    const int idx = juce::jlimit (0, kNumDenoms - 1, denominatorIndex (denominator) + direction);
    apply (numerator, kDenoms[idx], notify);
}

//==============================================================================
TimeSignatureControl::Zone TimeSignatureControl::zoneAt (juce::Point<int> p) const noexcept
{
    return p.x < getWidth() / 2 ? Zone::numerator : Zone::denominator;
}

//==============================================================================
void TimeSignatureControl::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (tokens::componentColour (*this, backgroundColourId, tokens::surfaceRecessed()));
    g.fillRoundedRectangle (bounds, 4.0f);

    // Feedback hover/drag: evidenzia la meta' attiva (drag ha priorita').
    // Geometria identica a zoneAt(): numeratore = [0, getWidth()/2).
    const Zone active = (dragZone != Zone::none) ? dragZone : hoverZone;
    if (active != Zone::none)
    {
        auto half = getLocalBounds();
        const auto numeratorHalf = half.removeFromLeft (getWidth() / 2);
        const auto zoneRect = (active == Zone::numerator) ? numeratorHalf : half;

        // accentSoft() di default ha gia' alpha bassa: nessun withAlpha extra.
        g.setColour (tokens::componentColour (*this, highlightColourId, tokens::accentSoft()));
        g.fillRoundedRectangle (zoneRect.toFloat().reduced (1.0f), 3.0f);
    }

    // Barra "/" diagonale al centro.
    g.setColour (tokens::componentColour (*this, slashColourId, tokens::textSecondary()));
    g.drawLine (bounds.getCentreX() + 3.0f, bounds.getY() + 6.0f,
                bounds.getCentreX() - 3.0f, bounds.getBottom() - 6.0f, 1.4f);

    // Cifre.
    g.setColour (tokens::componentColour (*this, textColourId, tokens::textPrimary()));
    g.setFont (tokens::type::value().withHeight (juce::jmin (12.0f, bounds.getHeight() * 0.4f)));
    
    g.drawText (juce::String (numerator),   bounds.removeFromLeft(bounds.getWidth()/2).reduced (2),  juce::Justification::centred, false);
    g.drawText (juce::String (denominator), bounds.reduced (2), juce::Justification::centred, false);
}

//==============================================================================
void TimeSignatureControl::mouseDown (const juce::MouseEvent& e)
{
    dragZone       = zoneAt (e.getPosition());
    dragStartValue = (dragZone == Zone::numerator) ? numerator : denominatorIndex (denominator);
    dragStartY     = e.y;
    repaint();
}

void TimeSignatureControl::mouseDrag (const juce::MouseEvent& e)
{
    if (dragZone == Zone::none) { return; }

    // ~12 px per step: drag morbido, niente jitter.
    const int steps = (dragStartY - e.y) / 12;

    if (dragZone == Zone::numerator)
    {
        apply (dragStartValue + steps, denominator, juce::sendNotificationSync);
    }
    else
    {
        const int idx = juce::jlimit (0, kNumDenoms - 1, dragStartValue + steps);
        apply (numerator, kDenoms[idx], juce::sendNotificationSync);
    }
}

void TimeSignatureControl::mouseUp (const juce::MouseEvent& e)
{
    if (dragZone == Zone::none) { return; }

    // Fine drag: torna al feedback hover (o nessuno se il mouse e' uscito).
    dragZone  = Zone::none;
    hoverZone = getLocalBounds().contains (e.getPosition()) ? zoneAt (e.getPosition()) : Zone::none;
    repaint();
}

void TimeSignatureControl::mouseMove (const juce::MouseEvent& e)
{
    const Zone z = zoneAt (e.getPosition());
    if (z != hoverZone) { hoverZone = z; repaint(); }
}

void TimeSignatureControl::mouseExit (const juce::MouseEvent&)
{
    if (hoverZone != Zone::none) { hoverZone = Zone::none; repaint(); }
}

void TimeSignatureControl::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) { return; }
    const int dir = wheel.deltaY > 0.0f ? 1 : -1;

    if (zoneAt (e.getPosition()) == Zone::numerator) { bumpNumerator (dir, juce::sendNotificationSync); }
    else                                             { bumpDenominator (dir, juce::sendNotificationSync); }
}

} // namespace ayra
