/** Shared logical-pixel metrics. Lane height, body height and curve height
    are deliberately distinct; painting and input consume the same result. */
#pragma once
#include <algorithm>
namespace ayra::timeline_panel::visual_metrics
{
inline constexpr int eventPadding = 3;
inline constexpr int eventBody = 18;
inline constexpr int cutRailExtent = 8;
inline constexpr int pointRow = 2 * eventPadding + eventBody;
inline constexpr int spanRow = pointRow + cutRailExtent;
inline constexpr int warpControlsLaneHeight = 64;
inline constexpr int previousCompactLaneHeight = 96;
inline constexpr int minimumEnvelopeBodyHeight = 46;
struct ClipRowGeometry
{
    float top = 0, body = 0;
    bool warpControls = false;
    static ClipRowGeometry make(float height, bool warp, bool cut) noexcept
    {
        ClipRowGeometry g;
        g.warpControls = warp && height >= warpControlsLaneHeight;
        g.top = g.warpControls ? 12.0f : static_cast<float>(eventPadding);
        const float bottom = static_cast<float>(eventPadding + (cut ? cutRailExtent : 0));
        g.body = std::max(0.0f, height - g.top - bottom);
        return g;
    }
};
} // namespace ayra::timeline_panel::visual_metrics
