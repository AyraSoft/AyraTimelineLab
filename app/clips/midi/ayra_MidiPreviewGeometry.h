/** Pure, allocation-free note overview geometry. Preview is not a small editor:
    it uses the available height, fits every pitch, and preserves visible onsets.
    Coordinates are local to the piano-roll rectangle; no document is modified. */
#pragma once
#include <algorithm>
#include <cmath>

namespace ayra::lab::clips::midi_preview
{
inline float headerHeight(float clipHeight) noexcept
{
    // Reserve a 16px header only when at least 16px remain for the overview.
    // The clip title remains available through the existing tooltip/card.
    return clipHeight >= 32.0f ? 16.0f : 0.0f;
}
struct NoteBounds
{
    float x=0, y=0, width=0, height=0;
    float radius=0;
    bool empty() const noexcept { return width<=0 || height<=0; }
};
inline NoteBounds project(float width, float height, float start, float end,
                          int pitch, int minimumPitch, int maximumPitch,
                          bool drum) noexcept
{
    if (!std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0
        || !std::isfinite(start) || !std::isfinite(end) || end<start
        || end<=0 || start>=1 || pitch<minimumPitch || pitch>maximumPitch
        || minimumPitch>maximumPitch)
        return {};
    const float onset=std::clamp(start,0.0f,1.0f)*width;
    const float finish=std::clamp(end,0.0f,1.0f)*width;
    const float markHeight=std::min(height,drum ? 3.0f : 2.0f);
    const float markWidth=std::min(width,drum ? markHeight : std::max(1.5f,finish-onset));
    const float pitchPosition=maximumPitch==minimumPitch ? 0.5f
        : static_cast<float>(maximumPitch-pitch)/static_cast<float>(maximumPitch-minimumPitch);
    NoteBounds out;
    out.width=markWidth;
    out.height=markHeight;
    out.x=std::clamp(onset-(drum ? markWidth*0.5f : 0.0f),0.0f,width-markWidth);
    out.y=pitchPosition*(height-markHeight);
    out.radius=drum ? std::min(markWidth,markHeight)*0.5f : 0.0f;
    return out;
}
} // namespace ayra::lab::clips::midi_preview
