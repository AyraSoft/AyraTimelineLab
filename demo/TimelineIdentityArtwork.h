/** Host-side Ayra identity recipe. Uses the selected Ayra tokens; it is not a
    copy of ApplicationLookAndFeel. The panel supplies data and delegates paint.
    The feathered backdrop is prepared only on identity/layout/theme changes,
    owned by the mounted row and released with it. No per-frame blur or cache singleton.
*/
#pragma once
#include "../modules/ayra_timeline_panel/TimelineIdentity.h"

namespace ayra::lab
{
class TimelineIdentityArtwork final
{
public:
    static timeline_panel::TimelineIdentityTypography typography()
    {
        namespace type = ayra::tokens::type;
        auto title = type::title().withHeight(20.0f);
        // Avenir's "Demi Bold" face is not installed on every host. JUCE may
        // otherwise fall back to a regular face despite the requested weight.
        // Resolve the *family* once per style update, never from paint().
        const auto available = juce::Font::findAllTypefaceNames();
        if (!available.contains(title.getTypefaceName(), true))
            title = juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold"));
        return {title, type::caption().withHeight(11.5f), type::body().withHeight(11.5f).boldened()};
    }
    static juce::Image prepare(const timeline_panel::TimelineIdentityContent& content,
                               const timeline_panel::TimelineIdentityLayout& layout)
    {
        const auto area = layout.backdrop;
        if (area.isEmpty()) return {};
        namespace tk = ayra::tokens;
        juce::Image result(juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
        juce::Image::BitmapData pixels(result, juce::Image::BitmapData::writeOnly);
        const auto textArea = layout.twoLines ? layout.primary.getUnion(layout.secondary) : layout.primary;
        // A neutral plateau supports the whole name. Owner colour only modulates
        // the finish subtly: saturated Audio/MIDI content cannot muddy the lettering.
        const auto topColour = tk::surfaceRaised().interpolatedWith(content.ownerColour, 0.035f);
        const auto bottomColour = tk::surfaceBase().interpolatedWith(content.ownerColour, 0.025f);
        const auto smooth = [](float value) {
            const float t = juce::jlimit(0.0f, 1.0f, value);
            return t * t * (3.0f - 2.0f * t);
        };
        const int textNear = content.onLeft ? textArea.getRight() - area.getX() : area.getRight() - textArea.getX();
        const float plateau = static_cast<float>(textNear + 9);
        const float feather = juce::jmax(1.0f, area.getWidth() - plateau);
        const float yFeather = layout.twoLines ? 12.0f : 3.0f;
        for (int y = 0; y < area.getHeight(); ++y)
        {
            const float vertical = smooth((y + 0.5f) / yFeather)
                                 * smooth((area.getHeight() - y - 0.5f) / yFeather);
            const auto colour = topColour.interpolatedWith(bottomColour, (y + 0.5f) / area.getHeight());
            for (int x = 0; x < area.getWidth(); ++x)
            {
                const float distance = content.onLeft ? x + 0.5f : area.getWidth() - x - 0.5f;
                const float horizontal = 1.0f - smooth((distance - plateau) / feather);
                pixels.setPixelColour(x, y, colour.withAlpha(0.96f * vertical * horizontal));
            }
        }
        return result;
    }
    static void draw(juce::Graphics& g, const timeline_panel::TimelineIdentityContent& content,
                     const timeline_panel::TimelineIdentityLayout& layout, const juce::Image& backdrop)
    {
        namespace tk = ayra::tokens;
        if (backdrop.isValid()) g.drawImageAt(backdrop, layout.backdrop.getX(), layout.backdrop.getY());
        const auto alignment = content.onLeft ? juce::Justification::centredLeft : juce::Justification::centredRight;
        g.setFont(layout.primaryFont);
        g.setColour(tk::textPrimary());
        g.drawText(content.primary, layout.primary, alignment, true);
        if (layout.twoLines)
        {
            g.setFont(layout.secondaryFont);
            g.setColour(tk::textSecondary());
            g.drawText(content.secondary, layout.secondary, alignment, true);
            if (!layout.typeMark.isEmpty())
            {
                g.setColour(content.typeColour);
                g.fillEllipse(layout.typeMark.toFloat());
            }
        }
        // Fine identity glint, not another rail or a box around the name.
        if (!content.compact && layout.twoLines)
        {
            const auto area = layout.primary;
            const float outer = static_cast<float>(content.onLeft ? area.getX() : area.getRight());
            const float inner = outer + (content.onLeft ? 1.0f : -1.0f) * juce::jmin(72.0f, static_cast<float>(area.getWidth()));
            juce::ColourGradient rim(content.ownerColour.withAlpha(content.selected ? 0.80f : 0.36f), outer, 0,
                                     content.ownerColour.withAlpha(0.0f), inner, 0, false);
            g.setGradientFill(rim);
            g.fillRect(juce::jmin(inner, outer), static_cast<float>(layout.secondary.getBottom() + 3),
                       std::abs(outer - inner), 1.0f);
        }
    }
};
} // namespace ayra::lab
