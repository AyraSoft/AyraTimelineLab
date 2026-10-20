/** View-only floating identity. Layout follows fixtures/frosted-lens.html.
    No document writes, mouse interception, reserved row height or media policy.
    CSS sizes are logical pixels; the host supplies typography and artwork. */
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace ayra::timeline_panel
{
struct TimelineIdentityContent
{
    juce::String performer, track, type;
    juce::Colour ownerColour, typeColour;
    bool compact = false, onLeft = false, selected = false, showType = true;
    bool operator==(const TimelineIdentityContent&) const = default;
};
struct TimelineIdentityTypography
{
    juce::Font track { juce::FontOptions().withPointHeight(12.5f).withStyle("Medium") };
    juce::Font performer { juce::FontOptions().withPointHeight(9.5f).withStyle("Bold") };
    juce::Font type { juce::FontOptions().withPointHeight(9.5f) };
};
struct TimelineIdentityLayout
{
    // backdrop also defines the STABLE hover footprint, even while invisible.
    juce::Rectangle<int> backdrop, performer, track, type, typeMark;
    bool twoLines = false;
    juce::Font trackFont {juce::FontOptions{}}, performerFont {juce::FontOptions{}}, typeFont {juce::FontOptions{}};
    static constexpr int maxWidth = 229; // CSS max-width 205 + horizontal padding 10 + 14
    static constexpr int minWidth = 144; // CSS min-width 120 + the same padding
    static constexpr int normalHeight = 40;

    static int measuredWidth(const juce::Font& font, const juce::String& text)
    {
        if (text.isEmpty()) return 0;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, 0, 0);
        return juce::roundToInt(std::ceil(glyphs.getBoundingBox(0, -1, true).getWidth())) + 2;
    }
    [[nodiscard]] static TimelineIdentityLayout make(
        juce::Rectangle<int> area, const TimelineIdentityContent& content,
        const TimelineIdentityTypography& fonts)
    {
        TimelineIdentityLayout out;
        out.trackFont = fonts.track;
        out.performerFont = fonts.performer;
        out.typeFont = fonts.type;
        if (area.isEmpty()) return out;
        out.twoLines = !content.compact && area.getHeight() >= normalHeight;
        const int insetX = juce::jmin(8, area.getWidth() / 8);
        const int available = juce::jmax(0, area.getWidth() - 2 * insetX);
        const int typeWidth = content.showType ? measuredWidth(fonts.type, content.type) : 0;
        const int typeGroup = typeWidth > 0 ? typeWidth + 9 : 0; // 5px dot + 4px gap
        const int metaWidth = content.performer.isEmpty() ? typeGroup
            : measuredWidth(fonts.performer, content.performer.toUpperCase()) + 10 + typeGroup;
        const int titleWidth = measuredWidth(fonts.track, content.track);
        const int natural = out.twoLines ? juce::jmax(metaWidth, titleWidth)
                                         : titleWidth + (typeGroup > 0 ? 10 + typeGroup : 0);
        const int width = juce::jmin(available, juce::jlimit(minWidth, maxWidth, natural + 38));
        const int height = juce::jmin(area.getHeight(), out.twoLines ? normalHeight : 22);
        const int top = area.getY() + (area.getHeight() - height) / 2;
        out.backdrop = {content.onLeft ? area.getX() + insetX : area.getRight() - insetX - width,
                        top, width, height};
        // Leave breathing room beyond the fully transparent left corners.
        const int leftPad = juce::jmin(24, width / 5), rightPad = juce::jmin(14, width / 5);
        const auto inner = out.backdrop.withTrimmedLeft(leftPad).withTrimmedRight(rightPad);
        const auto fit = [&](juce::Rectangle<int> r) { return r.getIntersection(out.backdrop); };
        if (out.twoLines)
        {
            const auto meta = fit({inner.getX(), top + 5, inner.getWidth(), 12});
            // Type has priority over performer in a narrow metadata row; track
            // retains its own full width. Text never stretches or wraps.
            const int groupWidth = juce::jmin(typeGroup, meta.getWidth());
            if (groupWidth >= 14)
            {
                const auto group = meta.withLeft(meta.getRight() - groupWidth);
                out.typeMark = {group.getX(), group.getCentreY() - 2, 5, 5};
                out.type = group.withTrimmedLeft(9);
            }
            if (content.performer.isNotEmpty())
                out.performer = meta.withWidth(juce::jmax(0, meta.getWidth() - groupWidth - (groupWidth > 0 ? 10 : 0)));
            out.track = fit({inner.getX(), top + 19, inner.getWidth(), 15});
        }
        else
        {
            // Global/short lanes do not grow to accommodate a name. One line,
            // track first; type drops out before the title becomes illegible.
            out.track = fit({inner.getX(), top + (height - 15) / 2, inner.getWidth(), 15});
            if (typeGroup > 0 && inner.getWidth() >= typeGroup + 60)
            {
                const auto group = out.track.withLeft(inner.getRight() - typeGroup);
                out.typeMark = {group.getX(), group.getCentreY() - 2, 5, 5};
                out.type = group.withTrimmedLeft(9);
                out.track.setWidth(juce::jmax(0, out.track.getWidth() - typeGroup - 10));
            }
        }
        return out;
    }
};

/** Hide only after a genuine clip hit within the lens. Once suppressed, stay
    suppressed over gaps/deleted/moved clips until the pointer leaves the same
    footprint. The painter's visibility never participates in hit testing. */
class TimelineIdentityHoverState
{
public:
    [[nodiscard]] bool hidden() const noexcept { return suppressed; }
    bool update(bool inside, bool clipUnderPointer) noexcept
    {
        const bool next = inside && (suppressed || clipUnderPointer);
        const bool changed = next != suppressed;
        suppressed = next;
        return changed;
    }
    void reset() noexcept { suppressed = false; }
private:
    bool suppressed = false;
};
} // namespace ayra::timeline_panel
