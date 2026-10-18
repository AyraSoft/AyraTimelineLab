/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / widgets / controls / AppWindowHeader (impl)
   ------------------------------------------------------------------
   Regola moduli Ayra: nessun include qui; dipendenze visibili da ayra_common.h.

  ==============================================================================
*/

namespace ayra
{

AppWindowHeader::AppWindowHeader (glyphs::Glyph headerIcon,
                                  juce::String headerTitle)
    : icon (headerIcon), title (std::move (headerTitle))
{
}

void AppWindowHeader::setIcon (glyphs::Glyph newIcon)
{
    icon = newIcon;
    repaint();
}

void AppWindowHeader::setTitle (juce::String newTitle)
{
    if (title == newTitle) { return; }
    title = std::move (newTitle);
    repaint();
}

template <typename ButtonType>
ButtonType& AppWindowHeader::addOwnedAction (
    std::unique_ptr<ButtonType> button, const int leadingGap,
    const bool trailing)
{
    static_assert (std::is_base_of_v<ShapeIconButton, ButtonType>);
    auto* raw = button.release();
    const int insertionIndex = trailing
        ? actions.size() : actions.size() - trailingActionCount;
    actions.insert (insertionIndex, raw);
    leadingGaps.insert (insertionIndex, juce::jmax (0, leadingGap));
    if (trailing) { ++trailingActionCount; }
    addAndMakeVisible (*raw);
    resized();
    return *raw;
}

ShapeIconButton& AppWindowHeader::addAction (
    const glyphs::Glyph& glyph, juce::String tooltip, const int leadingGap)
{
    return addActionInternal (
        glyph, std::move (tooltip), leadingGap, false);
}

ShapeIconButton& AppWindowHeader::addTrailingAction (
    const glyphs::Glyph& glyph, juce::String tooltip, const int leadingGap)
{
    return addActionInternal (
        glyph, std::move (tooltip), leadingGap, true);
}

ShapeIconButton& AppWindowHeader::addActionInternal (
    const glyphs::Glyph& glyph, juce::String tooltip,
    const int leadingGap, const bool trailing)
{
    auto button = std::make_unique<ShapeIconButton>();
    button->setGlyph (glyph);
    button->setIconPadding (1.0f / 6.0f);
    button->setDrawBackground (true);
    button->setShowIdleBackground (false);
    button->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (tooltip.isNotEmpty()) { button->setTooltip (tooltip); }
    return addOwnedAction (std::move (button), leadingGap, trailing);
}

AddButton& AppWindowHeader::addAddAction (
    juce::String tooltip, const int leadingGap)
{
    return addOwnedAction (
        std::make_unique<AddButton> (std::move (tooltip)), leadingGap);
}

void AppWindowHeader::setActionVisible (
    ShapeIconButton& action, const bool shouldBeVisible)
{
    jassert (actions.contains (&action));
    if (! actions.contains (&action) || action.isVisible() == shouldBeVisible)
        return;

    action.setVisible (shouldBeVisible);
    resized();
}

void AppWindowHeader::paint (juce::Graphics& g)
{
    getCommonLAF (*this).drawAppWindowHeader (
        g, getLocalBounds().toFloat(), icon, title);
}

void AppWindowHeader::resized()
{
    auto area = getLocalBounds().reduced (horizontalInset, 0);

    for (int i = actions.size(); --i >= 0;)
    {
        auto* action = actions[i];
        if (action == nullptr || ! action->isVisible()) { continue; }

        action->setBounds (
            area.removeFromRight (actionSize)
                .withSizeKeepingCentre (actionSize, actionSize));

        bool hasVisiblePredecessor = false;
        for (int previous = i; --previous >= 0;)
        {
            if (actions[previous] != nullptr && actions[previous]->isVisible())
            {
                hasVisiblePredecessor = true;
                break;
            }
        }

        if (hasVisiblePredecessor)
            area.removeFromRight (leadingGaps.getUnchecked (i));
    }
}

} // namespace ayra
