/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / tab_strip / TabStrip (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui (tipi, token, glifi, chrome primitives
   gia' visibili via ayra_common.h).
  ==============================================================================
*/

namespace ayra
{

//==============================================================================
// Prefisso `kTs` obbligatorio: nell'amalgama tutti i blocchi `namespace {}`
// confluiscono in un unico unnamed namespace -> nomi univoci nel modulo.
namespace
{
    constexpr float kTsPadX     = 14.0f;   ///< padding orizzontale interno alla tab.
    constexpr float kTsMinW     = 64.0f;   ///< larghezza minima di una tab.
    constexpr float kTsIconSize = 16.0f;
    constexpr float kTsIconGap  = 7.0f;    ///< spazio icona <-> label.
    constexpr float kTsBadgeGap = 7.0f;    ///< spazio label <-> badge.
    constexpr float kTsFadeW    = 22.0f;   ///< larghezza fade-edge quando scrollabile.
    constexpr int   kTsScrollH  = 8;       ///< altezza dello scrollbar orizzontale (solo se overflow).

    /// Larghezza del badge per un conteggio (caption + padding); 0 se count <= 0.
    float tsBadgeWidthFor (int count) noexcept
    {
        if (count <= 0) { return 0.0f; }
        return juce::jmax (18.0f,
                           juce::GlyphArrangement::getStringWidth (tokens::type::caption(),
                                                                   juce::String (count)) + 12.0f);
    }
}

//==============================================================================
TabStrip::TabStrip()
{
    setWantsKeyboardFocus (false);

    hScroll.setAutoHide (false);
    hScroll.addListener (this);
    addChildComponent (hScroll);   // reso visibile solo quando c'e' overflow.
}

TabStrip::~TabStrip()
{
    hScroll.removeListener (this);
}

//==============================================================================
float TabStrip::measureTabWidth (const Tab& t, float textW) const
{
    float w = kTsPadX + textW + kTsPadX;
    if (t.icon.isValid()) { w += kTsIconSize + kTsIconGap; }
    if (t.badge > 0)       { w += kTsBadgeGap + tsBadgeWidthFor (t.badge); }

    return juce::jmax (kTsMinW, w);
}

void TabStrip::relayout()
{
    slots.clear();
    slots.reserve (tabs.size());

    // Cache della larghezza testo (body font) per tab: 1:1 con `tabs`, calcolata
    // qui una sola volta e riusata in paint -> niente getStringWidth per-tab a ogni
    // frame durante hover/scroll. Stessa font (tokens::type::body()), risultato
    // identico al ricalcolo.
    tabTextW.clear();
    tabTextW.reserve (tabs.size());
    for (const auto& t : tabs)
        tabTextW.push_back (juce::GlyphArrangement::getStringWidth (tokens::type::body(), t.label));

    const float availW = (float) getWidth();

    float natural = 0.0f;
    for (size_t i = 0; i < tabs.size(); ++i) { natural += measureTabWidth (tabs[i], tabTextW[i]); }

    // FILL: se tutte le tab ci stanno, le allarghiamo proporzionalmente per
    // riempire la striscia (look "tab che riempiono", nessuno scroll). Altrimenti
    // larghezza naturale + scroll (con scrollbar trascinabile).
    scrollable = (natural > availW + 0.5f) && ! tabs.empty();

    const float h     = (float) tabAreaHeight();
    const float scale = (! scrollable && natural > 0.0f) ? (availW / natural) : 1.0f;

    float x = 0.0f;
    for (size_t i = 0; i < tabs.size(); ++i)
    {
        const float w = measureTabWidth (tabs[i], tabTextW[i]) * scale;
        slots.push_back ({ x, 0.0f, w, h });
        x += w;
    }
    contentW = x;

    if (! scrollable) { scrollX = 0.0f; }
    clampScroll();
    updateScrollBar();
}

int TabStrip::tabAreaHeight() const noexcept
{
    return getHeight() - (scrollable ? kTsScrollH : 0);
}

void TabStrip::updateScrollBar()
{
    hScroll.setVisible (scrollable);
    if (! scrollable) { return; }

    hScroll.setBounds (0, getHeight() - kTsScrollH, getWidth(), kTsScrollH);
    hScroll.setRangeLimits  (0.0, (double) contentW, juce::dontSendNotification);
    hScroll.setCurrentRange ((double) scrollX, (double) getWidth(), juce::dontSendNotification);
}

void TabStrip::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    scrollX = (float) newRangeStart;
    clampScroll();
    repaint();
}

void TabStrip::clampScroll() noexcept
{
    const float maxScroll = juce::jmax (0.0f, contentW - (float) getWidth());
    scrollX = juce::jlimit (0.0f, maxScroll, scrollX);
}

void TabStrip::ensureVisible (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) slots.size())) { return; }

    const auto s = slots[(size_t) index];
    if (s.getX() < scrollX)
        scrollX = s.getX();
    else if (s.getRight() > scrollX + (float) getWidth())
        scrollX = s.getRight() - (float) getWidth();

    clampScroll();
    hScroll.setCurrentRangeStart ((double) scrollX, juce::dontSendNotification);   // thumb in sync
}

int TabStrip::tabAtX (float xInComp) const
{
    const float cx = xInComp + scrollX;
    for (size_t i = 0; i < slots.size(); ++i)
        if (cx >= slots[i].getX() && cx < slots[i].getRight())
            return (int) i;
    return -1;
}

//==============================================================================
void TabStrip::setTabs (std::vector<Tab> newTabs)
{
    tabs = std::move (newTabs);

    if (selectionMode == SelectionMode::multiFilterWithAll)
    {
        multiSelected.assign (tabs.size(), false);
        selected = tabs.empty() ? -1 : 0;
        if (! tabs.empty()) { multiSelected[0] = true; }
    }
    else
    {
        if (tabs.empty())                        { selected = -1; }
        else if (selected >= (int) tabs.size())  { selected = (int) tabs.size() - 1; }
        else if (selected < 0)                   { selected = 0; }
    }

    hover = -1;
    relayout();
    repaint();
}

void TabStrip::setSelectionMode (const SelectionMode newMode)
{
    if (selectionMode == newMode) { return; }
    selectionMode = newMode;

    if (selectionMode == SelectionMode::multiFilterWithAll)
    {
        multiSelected.assign (tabs.size(), false);
        selected = tabs.empty() ? -1 : 0;
        if (! tabs.empty()) { multiSelected[0] = true; }
    }
    else
    {
        int first = -1;
        for (int i = 0; i < (int) multiSelected.size(); ++i)
            if (multiSelected[(std::size_t) i])
            {
                first = i;
                break;
            }
        selected = tabs.empty() ? -1
                                : juce::jlimit (0, (int) tabs.size() - 1,
                                                first >= 0 ? first : 0);
        multiSelected.clear();
    }
    repaint();
}

bool TabStrip::isTabSelected (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, (int) tabs.size())) { return false; }
    if (selectionMode == SelectionMode::single)
        return index == selected;
    return juce::isPositiveAndBelow (index, (int) multiSelected.size())
        && multiSelected[(std::size_t) index];
}

void TabStrip::setSelectedIndex (int index, juce::NotificationType notify)
{
    const int clamped = tabs.empty() ? -1
                                     : juce::jlimit (0, (int) tabs.size() - 1, index);

    if (selectionMode == SelectionMode::multiFilterWithAll)
    {
        multiSelected.assign (tabs.size(), false);
        if (clamped >= 0) { multiSelected[(std::size_t) clamped] = true; }
        selected = clamped;
        ensureVisible (selected);
        repaint();
        if (notify != juce::dontSendNotification && selected >= 0) { notifySelection(); }
        return;
    }

    if (clamped == selected) { return; }
    selected = clamped;
    ensureVisible (selected);
    repaint();

    if (notify != juce::dontSendNotification && selected >= 0) { notifySelection(); }
}

void TabStrip::addListener (Listener* l)    { listeners.add (l); }
void TabStrip::removeListener (Listener* l) { listeners.remove (l); }

void TabStrip::notifySelection()
{
    listeners.call ([this] (Listener& l) { l.tabStripSelectionChanged (this, selected); });
    if (onTabSelected) { onTabSelected (selected); }
}

void TabStrip::setTabSelected (const int index, const bool shouldBeSelected, juce::NotificationType notify)
{
    if (selectionMode == SelectionMode::single)
    {
        if (shouldBeSelected)
            setSelectedIndex (index, notify);
        else if (selected == index)
            setSelectedIndex (-1, notify);
        return;
    }

    if (! juce::isPositiveAndBelow (index, (int) tabs.size()))
        return;

    if (multiSelected.size() != tabs.size())
        multiSelected.assign (tabs.size(), false);

    if (index == 0)
    {
        if (! shouldBeSelected)
            return;

        std::fill (multiSelected.begin(), multiSelected.end(), false);
        multiSelected[0] = true;
        selected = 0;
    }
    else
    {
        if (multiSelected[(std::size_t) index] == shouldBeSelected)
            return;

        multiSelected[0] = false;
        multiSelected[(std::size_t) index] = shouldBeSelected;

        int specificCount = 0;
        for (int i = 1; i < (int) multiSelected.size(); ++i)
            if (multiSelected[(std::size_t) i]) { ++specificCount; }

        if (specificCount == 0
            || (tabs.size() > 2 && specificCount == (int) tabs.size() - 1))
        {
            std::fill (multiSelected.begin(), multiSelected.end(), false);
            if (! multiSelected.empty()) { multiSelected[0] = true; }
            selected = multiSelected.empty() ? -1 : 0;
        }
        else
        {
            selected = index;
        }
    }

    ensureVisible (index);
    repaint();
    if (notify != juce::dontSendNotification && selected >= 0)
        notifySelection();
}

void TabStrip::setSelectedIndices (const std::vector<int>& indices, juce::NotificationType notify)
{
    if (selectionMode == SelectionMode::single)
    {
        setSelectedIndex (indices.empty() ? -1 : indices.front(), notify);
        return;
    }

    if (multiSelected.size() != tabs.size())
        multiSelected.assign (tabs.size(), false);
    else
        std::fill (multiSelected.begin(), multiSelected.end(), false);

    bool hasZero = false;
    for (const int idx : indices)
    {
        if (idx == 0)
        {
            hasZero = true;
            break;
        }
    }

    if (hasZero || indices.empty())
    {
        if (! multiSelected.empty()) { multiSelected[0] = true; }
        selected = multiSelected.empty() ? -1 : 0;
    }
    else
    {
        int specificCount = 0;
        int lastIdx = 0;
        for (const int idx : indices)
        {
            if (idx > 0 && idx < (int) tabs.size())
            {
                if (! multiSelected[(std::size_t) idx])
                {
                    multiSelected[(std::size_t) idx] = true;
                    ++specificCount;
                    lastIdx = idx;
                }
            }
        }

        if (specificCount == 0
            || (tabs.size() > 2 && specificCount == (int) tabs.size() - 1))
        {
            std::fill (multiSelected.begin(), multiSelected.end(), false);
            if (! multiSelected.empty()) { multiSelected[0] = true; }
            selected = multiSelected.empty() ? -1 : 0;
        }
        else
        {
            selected = lastIdx;
        }
    }

    if (selected >= 0) { ensureVisible (selected); }
    repaint();
    if (notify != juce::dontSendNotification && selected >= 0)
        notifySelection();
}

std::vector<int> TabStrip::getSelectedIndices() const
{
    std::vector<int> result;
    if (selectionMode == SelectionMode::single)
    {
        if (selected >= 0 && selected < (int) tabs.size())
            result.push_back (selected);
        return result;
    }

    for (std::size_t i = 0; i < multiSelected.size(); ++i)
        if (multiSelected[i])
            result.push_back ((int) i);

    return result;
}

void TabStrip::toggleMultiFilterTab (const int index)
{
    if (selectionMode != SelectionMode::multiFilterWithAll
        || ! juce::isPositiveAndBelow (index, (int) tabs.size()))
        return;

    if (index == 0)
        setTabSelected (0, true);
    else
        setTabSelected (index, ! isTabSelected (index));
}

//==============================================================================
void TabStrip::resized()
{
    relayout();
}

void TabStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = tabAtX ((float) e.x);
    if (i < 0) { return; }

    if (selectionMode == SelectionMode::multiFilterWithAll)
    {
        toggleMultiFilterTab (i);
        return;
    }

    if (i != selected) { setSelectedIndex (i, juce::sendNotification); }
}

void TabStrip::mouseMove (const juce::MouseEvent& e)
{
    const int i = tabAtX ((float) e.x);
    if (i == hover) { return; }

    const int old = hover;
    hover = i;
    repaintTab (old);
    repaintTab (hover);
}

void TabStrip::mouseExit (const juce::MouseEvent&)
{
    if (hover == -1) { return; }
    const int old = hover;
    hover = -1;
    repaintTab (old);
}

void TabStrip::repaintTab (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) slots.size())) { return; }

    auto area = slots[(size_t) index].translated (-scrollX, 0.0f).expanded (3.0f);
    const float maxY = (float) tabAreaHeight();
    area = area.getIntersection ({ 0.0f, 0.0f, (float) getWidth(), maxY });
    if (! area.isEmpty()) { repaint (area.toNearestInt()); }
}

void TabStrip::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! scrollable) { return; }

    const float delta = (std::abs (w.deltaX) > std::abs (w.deltaY)) ? w.deltaX : w.deltaY;
    scrollX -= delta * 80.0f;
    clampScroll();
    hScroll.setCurrentRangeStart ((double) scrollX, juce::dontSendNotification);
    repaint();
}

//==============================================================================
void TabStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    if (scrollable) { area.removeFromBottom ((float) kTsScrollH); }   // spazio per lo scrollbar.

    // Well recessed fisso: i tab scorrono sopra di esso.
    fillRecessedSurface (g, area, tokens::radius::control);

    {
        const juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.toNearestInt());
        g.addTransform (juce::AffineTransform::translation (-scrollX, 0.0f));

        for (size_t i = 0; i < tabs.size(); ++i)
        {
            const auto&  t        = tabs[i];
            const auto   slot     = slots[i];
            const bool   isActive = isTabSelected ((int) i);
            const bool   isHover  = ((int) i == hover);
            const auto   pillCol  = t.accent.isTransparent() ? getCommonLAF (*this).accentOr (tokens::accent()) : t.accent;

            if (isActive)
            {
                const auto pill = slot.reduced (3.0f, 3.0f);
                juce::Path p; p.addRoundedRectangle (pill, juce::jmax (0.0f, tokens::radius::control - 2.0f));
                g.setColour (pillCol);
                g.fillPath (p);
                g.setColour (pillCol.withAlpha (0.35f));
                g.strokePath (p, juce::PathStrokeType (1.0f));
            }
            else if (isHover)
            {
                g.setColour (tokens::surfaceRaised().withAlpha (0.5f));
                g.fillRoundedRectangle (slot.reduced (3.0f, 3.0f), juce::jmax (0.0f, tokens::radius::control - 2.0f));
            }

            const auto activeFg = tokens::textOnTint (pillCol);
            const auto col = isActive ? activeFg : tokens::textSecondary();

            auto inner = slot.reduced (kTsPadX, 0.0f);

            // Badge a destra (es. n. mapping attivi).
            if (t.badge > 0)
            {
                const float bw    = tsBadgeWidthFor (t.badge);
                auto        bArea = inner.removeFromRight (bw).withSizeKeepingCentre (bw, 16.0f);
                inner.removeFromRight (kTsBadgeGap);

                g.setColour (isActive ? activeFg.withMultipliedAlpha (0.25f)
                                      : getCommonLAF (*this).accentOr (tokens::accent()).withAlpha (0.16f));
                g.fillRoundedRectangle (bArea, 8.0f);
                g.setColour (isActive ? activeFg
                                      : getCommonLAF (*this).accentOr (tokens::accent()));
                g.setFont (tokens::type::caption());
                g.drawText (juce::String (t.badge), bArea.toNearestInt(), juce::Justification::centred);
            }

            // Gruppo [icona + label] centrato nello spazio rimasto.
            // Larghezza testo gia' calcolata in relayout (1:1 con tabs/slots).
            const float textW = tabTextW[i];
            const float grpW  = (t.icon.isValid() ? kTsIconSize + kTsIconGap : 0.0f) + textW;
            float gx = inner.getCentreX() - grpW * 0.5f;

            if (t.icon.isValid())
            {
                glyphs::drawGlyph (g, t.icon,
                                   { gx, inner.getCentreY() - kTsIconSize * 0.5f, kTsIconSize, kTsIconSize },
                                   col);
                gx += kTsIconSize + kTsIconGap;
            }

            g.setColour (col);
            g.setFont (tokens::type::body());
            g.drawText (t.label, juce::Rectangle<float> (gx, inner.getY(), textW + 2.0f, inner.getHeight()).toNearestInt(),
                        juce::Justification::centredLeft, false);
        }
    }

    // Fade-edge: indica contenuto scrollato fuori vista (stessa logica per i due lati,
    // direzione codificata dagli estremi del gradiente).
    if (! scrollable) { return; }

    const float maxScroll = juce::jmax (0.0f, contentW - area.getWidth());
    const auto fadeEdge = [&] (float rectX, float opaqueEdgeX, float clearInnerX)
    {
        juce::ColourGradient grad (tokens::surfaceRecessed().withAlpha (0.95f), opaqueEdgeX, 0.0f,
                                   tokens::surfaceRecessed().withAlpha (0.0f),  clearInnerX, 0.0f, false);
        g.setGradientFill (grad);
        g.fillRect (rectX, area.getY() + 1.0f, kTsFadeW, area.getHeight() - 2.0f);
    };
    if (scrollX > 0.5f)
        fadeEdge (area.getX(), area.getX(), area.getX() + kTsFadeW);
    if (scrollX < maxScroll - 0.5f)
        fadeEdge (area.getRight() - kTsFadeW, area.getRight(), area.getRight() - kTsFadeW);
}

} // namespace ayra
