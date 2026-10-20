/** Host-side translation of the approved frosted-lens.html, not another global
    Ayra palette. Other widgets keep their selected Multiverse visual source.
    Material and the feathered rim are cached per resident row; text stays vector.
    Optional real backdrop blur uses the existing Melatonin provider, not a
    fabricated frosted texture. No global cache or continuous animation. */
#pragma once
#include "../modules/ayra_timeline_panel/TimelineIdentity.h"

namespace ayra::lab
{
class TimelineIdentityArtwork final
{
public:
    static timeline_panel::TimelineIdentityTypography typography()
    {
        // Match the HTML system/Inter stack without shipping font files. Resolve
        // once per style update, not during paint. JUCE point height specifies
        // the em-square in logical pixels, matching CSS font-size (withHeight
        // would incorrectly squeeze the glyphs into ascender+descender).
        const auto available = juce::Font::findAllTypefaceNames();
        auto family = ayra::tokens::type::body().getTypefaceName();
        for (const auto& candidate : {"SF Pro Text", "Inter", "Inter Display"})
            if (available.contains(candidate, true)) { family = candidate; break; }
        const auto font = [&](float height, const char* weight, float tracking) {
            return juce::Font(juce::FontOptions().withName(family).withPointHeight(height).withStyle(weight))
                .withExtraKerningFactor(tracking);
        };
        return {font(12.5f, "Medium", -0.01f), font(9.5f, "SemiBold", 0.07f), font(9.5f, "Medium", 0.0f)};
    }
    static juce::Image prepare(const timeline_panel::TimelineIdentityContent& content,
                               const timeline_panel::TimelineIdentityLayout& layout)
    {
        const auto area = layout.backdrop;
        if (area.isEmpty()) return {};
        // 2x cached material remains smooth through 100/125/150/200% scaling.
        juce::Image result(juce::Image::ARGB, area.getWidth() * 2, area.getHeight() * 2, true);
        juce::Graphics g(result); g.addTransform(juce::AffineTransform::scale(2.0f));
        const auto r = area.withPosition(0, 0).toFloat();
        const bool dark = ayra::tokens::detail::dark();
        const auto glass = dark ? juce::Colour(0xff111724) : juce::Colour(0xffeef2fa);
        // CSS linear-gradient(105deg, .72 at 0%, .32 at 75%, 0 at 100%).
        // Mirror the direction when docked right, not the text or its hierarchy.
        const float sign = content.onLeft ? 1.0f : -1.0f;
        const float dx = 0.9659258f * sign, dy = 0.2588190f;
        const float length = std::abs(dx) * r.getWidth() + dy * r.getHeight();
        const auto a = r.getCentre() - juce::Point<float>(dx, dy) * (length * 0.5f);
        const auto b = r.getCentre() + juce::Point<float>(dx, dy) * (length * 0.5f);
        juce::ColourGradient gradient(glass.withAlpha(0.72f), a, glass.withAlpha(0.0f), b, false);
        gradient.addColour(0.75, glass.withAlpha(0.32f));
        g.setGradientFill(gradient); g.fillRoundedRectangle(r, 7.0f);
        // Bake the rim into the same feathered material. An unmasked vector
        // highlight would reveal the very left corners we intend to dissolve.
        {
            const juce::Graphics::ScopedSaveState save(g);
            juce::Path lens;lens.addRoundedRectangle(r,7.0f);g.reduceClipRegion(lens);
            g.setColour(dark?juce::Colours::white.withAlpha(0.12f):juce::Colours::white.withAlpha(0.65f));
            g.fillRect(r.getX(),r.getY(),r.getWidth(),0.5f);
        }
        return mask(result,layout,area,2.0f);
    }
    // The first nine logical pixels are COMPLETELY clear: both 7px corners
    // disappear before the smooth fade begins. Independent of the name side.
    static float leftAlpha(float x) noexcept
    {
        const auto u=juce::jlimit(0.0f,1.0f,(x-9.0f)/16.0f);
        return u*u*(3.0f-2.0f*u);
    }
    static juce::Image mask(const juce::Image& source,const timeline_panel::TimelineIdentityLayout& layout,
                            juce::Rectangle<int> bounds,float scale)
    {
        if(!source.isValid()||bounds.isEmpty())return {};
        juce::Image alpha(juce::Image::SingleChannel,source.getWidth(),source.getHeight(),true);
        {
            juce::Graphics m(alpha);m.addTransform(juce::AffineTransform::scale(scale));
            const auto lens=layout.backdrop.translated(-bounds.getX(),-bounds.getY()).toFloat();
            juce::ColourGradient fade(juce::Colours::transparentWhite,lens.getX()+9,lens.getY(),
                                      juce::Colours::white,lens.getX()+25,lens.getY(),false);
            for(int i=1;i<8;++i)fade.addColour(i/8.0,juce::Colours::white.withAlpha(leftAlpha(9.0f+2.0f*i)));
            m.setGradientFill(fade);m.fillRoundedRectangle(lens,7.0f);
        }
        juce::Image result(juce::Image::ARGB,source.getWidth(),source.getHeight(),true);
        juce::Graphics g(result);g.reduceClipRegion(alpha,juce::AffineTransform());g.drawImageAt(source,0,0);
        return result;
    }
    static juce::Image prepareUnderlay(const juce::Image& source,float scale,
                                       const timeline_panel::TimelineIdentityLayout& layout,juce::Rectangle<int> bounds)
    {return mask(blurUnderlay(source,scale),layout,bounds,scale);}
    static juce::Image blurUnderlay(const juce::Image& source, float scale)
    {
        if (!source.isValid()) return {};
        melatonin::CachedBlur blur(static_cast<std::size_t>(juce::jmax(1, juce::roundToInt(5.0f * scale))));
        return blur.render(source); // returned Image retains its data after blur dies
    }
    static void drawUnderlay(juce::Graphics& g, const timeline_panel::TimelineIdentityLayout& layout,
                             const juce::Image& image, juce::Rectangle<int> sampleBounds)
    {
        if (!image.isValid()) return;
        const juce::Graphics::ScopedSaveState save(g);
        juce::Path lens; lens.addRoundedRectangle(layout.backdrop.toFloat(), 7.0f);
        g.reduceClipRegion(lens);
        g.drawImage(image, sampleBounds.toFloat());
    }
    static void draw(juce::Graphics& g, const timeline_panel::TimelineIdentityContent& content,
                     const timeline_panel::TimelineIdentityLayout& layout, const juce::Image& material)
    {
        if (layout.backdrop.isEmpty()) return;
        const bool dark = ayra::tokens::detail::dark();
        if (material.isValid()) g.drawImage(material, layout.backdrop.toFloat());
        const auto label = [&](const juce::String& text, juce::Rectangle<int> rect,
                               const juce::Font& font, juce::Colour colour, bool right) {
            if (rect.isEmpty()) return;
            g.setColour(colour); g.setFont(font);
            g.drawText(text, rect, right?juce::Justification::centredRight:juce::Justification::centredLeft, true);
        };
        label(content.performer.toUpperCase(), layout.performer, layout.performerFont,
              dark ? juce::Colour(0xffa5b4fc) : juce::Colour(0xff4f56a4),false);
        label(content.track, layout.track, layout.trackFont,
              dark ? juce::Colour(0xfff1f5f9) : juce::Colour(0xff182033),true);
        label(content.type, layout.type, layout.typeFont,
              dark ? juce::Colour(0xff94a3b8) : juce::Colour(0xff58677c),true);
        if (!layout.typeMark.isEmpty())
        {
            const auto dot = layout.typeMark.toFloat();
            // The 5px glow is an inexpensive radial falloff, not a per-frame blur.
            const auto glow = dot.expanded(5.0f);
            const auto centre = dot.getCentre();
            juce::ColourGradient halo(content.typeColour.withAlpha(0.38f), centre,
                                      content.typeColour.withAlpha(0.0f), {glow.getRight(), centre.y}, true);
            halo.addColour(0.35, content.typeColour.withAlpha(0.24f));
            g.setGradientFill(halo); g.fillEllipse(glow);
            g.setColour(content.typeColour); g.fillEllipse(dot);
        }
    }
};
} // namespace ayra::lab
