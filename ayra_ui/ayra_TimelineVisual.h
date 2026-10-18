#pragma once

namespace ayra
{

namespace timeline_visual_style
{
/** Tinta canonica del velo sotto le curve di automazione. E' la tinta Tempo
    adottata dal linguaggio Multiverse, mentre stroke/nodi restano TimelineLab. */
[[nodiscard]] inline juce::Colour tempoCurveTint() noexcept
{
    return juce::Colour (0xff2f9e8f);
}

/** Varianti cromatiche di confronto per lo stroke TimelineLab. */
enum class CurveLineColourVariant : std::uint8_t
{
    light,
    tempoTeal,
    accentAmber
};

[[nodiscard]] inline juce::Colour curveLineColour (
    const CurveLineColourVariant variant) noexcept
{
    switch (variant)
    {
        case CurveLineColourVariant::tempoTeal:   return tempoCurveTint().brighter (0.28f);
        case CurveLineColourVariant::accentAmber: return tokens::accent();
        case CurveLineColourVariant::light:       return juce::Colours::white;
    }

    jassertfalse;
    return juce::Colours::white;
}
} // namespace timeline_visual_style

/** Riempimento associato a una curva. Le clip usano un velo scuro uniforme;
    le lane continue possono adottare il gradiente Tempo di Multiverse. */
enum class TimelineCurveUnderlayStyle : std::uint8_t
{
    darkBelow,
    darkAbove,
    tempoGradientBelow
};

enum class TimelineLaneZone
{
    none,
    spineSelectAndReorder,
    bottomResizeDivider
};

struct TimelineLaneGeometry
{
    juce::Rectangle<float> spineBounds;
    juce::Rectangle<float> contentBounds;
    juce::Rectangle<float> dividerBounds;
};

[[nodiscard]] inline TimelineLaneGeometry computeTimelineLaneGeometry (
    juce::Rectangle<float> bounds) noexcept
{
    constexpr float spineWidth = 10.0f;
    TimelineLaneGeometry geometry;
    geometry.spineBounds = bounds.removeFromLeft (spineWidth);
    geometry.contentBounds = bounds;
    geometry.dividerBounds = juce::Rectangle<float> (
        bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
    return geometry;
}

struct TimelineLaneVisual
{
    juce::Rectangle<float> bounds;
    juce::String watermark;
    tokens::StreamType streamType { tokens::StreamType::audio };
    bool selected = false;
    bool suspended = false;
    TimelineLaneZone hoveredZone { TimelineLaneZone::none };
};

} // namespace ayra
