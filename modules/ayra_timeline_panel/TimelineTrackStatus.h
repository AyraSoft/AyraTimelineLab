#pragma once
#include <array>
#include <cstddef>

namespace ayra::timeline_panel
{
/** Read-only projection of the canonical track/Tensor, not per-lane switches.
    The host resolves effective runtime states. In particular, Monitoring::Auto
    is a policy, not proof that monitoring is active; canvasActive is not the
    visibility of this timeline row. Unknown/unavailable states stay false. */
struct TrackStatus
{
    bool shutdown=false, bypassed=false, canvasActive=false, monitoringActive=false;
    bool bufferized=false, oversampled=false, latencyCompensated=false;
    bool operator==(const TrackStatus&) const = default;
};
enum class TrackIndicator : std::size_t
{ shutdown, bypass, canvas, monitoring, bufferize, oversampling, latencyCompensation, count };
inline constexpr std::size_t trackIndicatorCount=static_cast<std::size_t>(TrackIndicator::count);
inline std::array<bool,trackIndicatorCount> activeTrackIndicators(const TrackStatus& s) noexcept
{return {s.shutdown,s.bypassed,s.canvasActive,s.monitoringActive,s.bufferized,s.oversampled,s.latencyCompensated};}
} // namespace ayra::timeline_panel
