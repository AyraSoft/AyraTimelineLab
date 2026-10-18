namespace ayra
{

namespace tk = tokens;

void CommonDefaultLookAndFeel::drawTimelineLane (juce::Graphics& g, const TimelineLaneVisual& v)
{
    const auto geom = computeTimelineLaneGeometry (v.bounds);
    const auto base = tk::streamColour (v.streamType);
    const auto tint = v.suspended ? base.darker().darker() : base;

    if (v.selected)
    {
        g.setColour (tint.withAlpha (0.12f));
        g.fillRect (geom.contentBounds);
    }

    if (v.watermark.isNotEmpty())
    {
        g.setColour (v.selected ? tk::textPrimary().withAlpha (0.42f)
                                : tk::textSecondary().withAlpha (0.22f));
        g.setFont (tk::type::section());
        g.drawText (v.watermark.toUpperCase(),
                    geom.contentBounds.reduced (12.0f, 5.0f).toNearestInt(),
                    juce::Justification::topLeft, true);
    }

    const auto spine = geom.spineBounds.reduced (0.0f, 1.0f);
    g.setColour (tint);
    g.fillRect (spine);

    const bool spineActive = v.selected
                          || v.hoveredZone == TimelineLaneZone::spineSelectAndReorder;
    if (spineActive)
    {
        g.setColour (juce::Colours::white.withAlpha (v.selected ? 0.95f : 0.65f));
        g.drawRect (spine.reduced (0.5f), v.selected ? 1.5f : 1.0f);

        g.setColour (tk::surfaceRecessed());
        for (int i = -1; i <= 1; ++i)
        {
            g.fillEllipse (spine.getCentreX() - 1.2f,
                           spine.getCentreY() + static_cast<float> (i) * 4.0f - 1.2f,
                           2.4f, 2.4f);
        }
    }

    const bool resizeHover = v.hoveredZone == TimelineLaneZone::bottomResizeDivider;
    g.setColour (resizeHover ? tk::accent() : tk::borderHairline());
    g.fillRect (v.bounds.getX(),
                v.bounds.getBottom() - (resizeHover ? 2.0f : 1.0f),
                v.bounds.getWidth(),
                resizeHover ? 2.0f : 1.0f);
}

void CommonDefaultLookAndFeel::drawTimelineAutomationCurveUnderlay (
    juce::Graphics& g, const juce::Path& line, const juce::Rectangle<float> bounds,
    const TimelineCurveUnderlayStyle style)
{
    if (line.isEmpty() || bounds.isEmpty()) { return; }

    juce::Path fill = line;
    const bool fillAbove = style == TimelineCurveUnderlayStyle::darkAbove;
    const float edgeY = fillAbove ? bounds.getY() : bounds.getBottom();
    fill.lineTo (bounds.getRight(), edgeY);
    fill.lineTo (bounds.getX(), edgeY);
    fill.closeSubPath();

    if (style != TimelineCurveUnderlayStyle::tempoGradientBelow)
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillPath (fill);
        return;
    }

    const auto tempoTint = timeline_visual_style::tempoCurveTint();
    juce::ColourGradient gradient (tempoTint.withAlpha (0.22f),
                                   bounds.getX(), bounds.getY(),
                                   tempoTint.withAlpha (0.02f),
                                   bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillPath (fill);
}

void CommonDefaultLookAndFeel::drawTimelineAutomationCurveStroke (
    juce::Graphics& g, const juce::Path& line, const juce::Colour lineColour)
{
    if (line.isEmpty()) { return; }
    g.setColour (juce::Colour (0x55000000));
    g.strokePath (line, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (lineColour.withAlpha (0.95f));
    g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void CommonDefaultLookAndFeel::drawTimelineKeyframeLaneBackground (
    juce::Graphics& g, const juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty()) { return; }
    g.setColour (tk::surfaceRecessed());
    g.fillRect (bounds);
    g.setColour (tk::borderHairline().withAlpha (0.7f));
    g.fillRect (bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
}

void CommonDefaultLookAndFeel::drawTimelineKeyframeEvent (
    juce::Graphics& g, const juce::Rectangle<float> laneBounds,
    const juce::Rectangle<float> eventBounds, const juce::String& label,
    const juce::Colour sourceColour, const bool selected, const bool hovered,
    const bool suspended)
{
    if (laneBounds.isEmpty() || eventBounds.isEmpty()) { return; }

    const bool active = selected || hovered;
    const auto baseColour = suspended ? sourceColour.withSaturation (0.10f)
                                      : sourceColour;
    const auto colour = active ? baseColour.brighter (0.25f) : baseColour;
    const float corner = juce::jmin (5.0f, eventBounds.getHeight() * 0.35f);

    g.setColour (colour.withAlpha (0.95f));
    g.fillRect (eventBounds.getX(), laneBounds.getY(), 1.5f, laneBounds.getHeight());

    juce::ColourGradient gradient (colour.brighter (0.12f),
                                   eventBounds.getX(), eventBounds.getY(),
                                   colour.darker (0.14f),
                                   eventBounds.getX(), eventBounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (eventBounds, corner);

    g.setColour (active ? tk::accent()
                        : colour.darker (0.5f).withAlpha (0.9f));
    g.drawRoundedRectangle (eventBounds.reduced (0.5f), corner,
                            active ? 1.6f : 1.0f);

    g.setColour (tk::textOnTint (colour));
    g.setFont (tk::type::caption().boldened());
    g.drawText (label, eventBounds.toNearestInt(), juce::Justification::centred, true);
}

} // namespace ayra
