/** @file TimelineIdentity.h
    Presentation-only identity overlay. No reserved row height, document writes,
    provider palette, image blur or interactive child belongs to this layout.
    Text is ellipsised, never horizontally squeezed. The host owns typography.
*/
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace ayra::timeline_panel
{
struct TimelineIdentityContent
{
    juce::String primary, secondary;
    juce::Colour ownerColour, typeColour;
    bool compact = false, onLeft = false, selected = false, showType = true;
    bool operator==(const TimelineIdentityContent&) const = default;
};
struct TimelineIdentityTypography
{
    juce::Font title { juce::FontOptions().withHeight(17.0f).withStyle("Bold") };
    juce::Font detail { juce::FontOptions().withHeight(11.0f) };
    juce::Font compact { juce::FontOptions().withHeight(11.5f).withStyle("Bold") };
};
struct TimelineIdentityLayout
{
    juce::Rectangle<int> backdrop, primary, secondary, typeMark;
    bool twoLines = false;
    juce::Font primaryFont {juce::FontOptions{}}, secondaryFont {juce::FontOptions{}};

    static int measuredWidth(const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, 0, 0);
        return juce::roundToInt(std::ceil(glyphs.getBoundingBox(0, -1, true).getWidth())) + 2;
    }
    [[nodiscard]] static TimelineIdentityLayout make(
        juce::Rectangle<int> contentArea, const TimelineIdentityContent& content,
        const TimelineIdentityTypography& fonts)
    {
        TimelineIdentityLayout out;
        if (contentArea.isEmpty()) return out;
        const bool dense = content.compact || contentArea.getHeight() < 48;
        out.primaryFont = dense ? fonts.compact : fonts.title;
        out.secondaryFont = fonts.detail;
        out.twoLines = !dense && content.secondary.isNotEmpty();
        // The overlay floats INSIDE content. No input border, clip X or row size changes.
        const int edge = juce::jmin(12, contentArea.getWidth() / 8);
        const int maxText = juce::jmax(0, juce::jmin(280, contentArea.getWidth() - 2 * edge));
        const int titleWidth = juce::jmin(maxText, measuredWidth(out.primaryFont, content.primary));
        const int titleHeight = juce::jmin(contentArea.getHeight(), dense ? 18 : 25);
        const int detailHeight = out.twoLines ? 17 : 0;
        const int blockHeight = titleHeight + detailHeight;
        const int top = contentArea.getCentreY() - blockHeight / 2;
        const auto align = [&](int width, int y, int height) {
            const int x = content.onLeft ? contentArea.getX() + edge : contentArea.getRight() - edge - width;
            return juce::Rectangle<int>(x, y, width, height).getIntersection(contentArea);
        };
        out.primary = align(titleWidth, top, titleHeight);
        int detailWidth = 0;
        if (out.twoLines)
        {
            const int markSpace = content.showType ? juce::jmin(12,maxText) : 0;
            detailWidth = juce::jmin(juce::jmax(0, maxText - markSpace), measuredWidth(out.secondaryFont, content.secondary));
            const auto line = align(detailWidth + markSpace, top + titleHeight, detailHeight);
            out.secondary = line.withTrimmedLeft(markSpace);
            if (content.showType && line.getWidth() >= 4)
                out.typeMark = {line.getX(), line.getCentreY() - 2, 4, 4};
        }
        const int textWidth = juce::jmax(titleWidth, detailWidth + (content.showType && out.twoLines ? 12 : 0));
        const int feather = dense ? 28 : 52;
        const int backingWidth = juce::jmin(contentArea.getWidth(), textWidth + edge + feather);
        const int backingHeight = juce::jmin(contentArea.getHeight(), blockHeight + (dense ? 4 : 24));
        out.backdrop = {content.onLeft ? contentArea.getX() : contentArea.getRight() - backingWidth,
                        contentArea.getCentreY() - backingHeight / 2, backingWidth, backingHeight};
        return out;
    }
};
} // namespace ayra::timeline_panel
