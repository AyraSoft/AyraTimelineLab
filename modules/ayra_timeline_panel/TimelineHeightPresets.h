/** Presentation-only vertical levels. Never changes media or the time axis.
    Keep these levels shared between the viewport and the individual canvases. */
#pragma once
#include <cmath>
namespace ayra::timeline_panel::height_presets
{
inline constexpr int compact = 24;  // same as a single row of point events
inline constexpr int midiEditor = 256;
inline constexpr int expanded = 400;
[[nodiscard]] constexpr int next (float current, bool midi) noexcept
{
    if (!(current >= 0.0f) || !std::isfinite(current)) return compact;
    if (midi)
        return current < (compact + midiEditor) * 0.5f ? midiEditor
             : current < (midiEditor + expanded) * 0.5f ? expanded : compact;
    return current < (compact + expanded) * 0.5f ? expanded : compact;
}
} // namespace ayra::timeline_panel::height_presets
