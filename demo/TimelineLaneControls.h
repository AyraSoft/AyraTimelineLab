/** Glyph-only lane controls. The row paints ONE canonical Ayra toolbar well
    per bank; individual controls never paint a permanent recessed surface. */
#pragma once
#include "../modules/ayra_timeline_panel/TimelinePanel.h"

namespace ayra::lab
{
class TimelineLaneControls final
{
public:
    // Same single-chevron geometry/stroke as TimelineTrackHeaderWidget::paint.
    // Event navigation is NOT transport rewind/fast-forward (no vertical bar).
    static juce::Path navigationPath(juce::Rectangle<float> box, bool right)
    {
        const auto r=box.reduced(box.getWidth()*0.32f,box.getHeight()*0.26f);
        juce::Path path;
        path.startNewSubPath(right?r.getX():r.getRight(),r.getY());
        path.lineTo(right?r.getRight():r.getX(),r.getCentreY());
        path.lineTo(right?r.getX():r.getRight(),r.getBottom());
        return path;
    }
    static void drawGroup(juce::Graphics& g,juce::Rectangle<float> bounds)
    {ayra::drawFlagToolbarWell(g,bounds);}
    static std::unique_ptr<juce::Button> makeButton(timeline_panel::LaneButton kind)
    {
        using B=timeline_panel::LaneButton;
        if(kind==B::previous||kind==B::next)return std::make_unique<Chevron>(kind==B::next);
        if(kind==B::mute||kind==B::solo)return std::make_unique<Letter>(kind==B::mute);
        auto button=std::make_unique<ayra::ShapeIconButton>();
        button->setDrawBackground(false);button->setIconPadding(0.22f);
        button->setGlyph(kind==B::open?ayra::glyphs::window:ayra::glyphs::audio);
        return button;
    }
private:
    static void feedback(juce::Graphics& g,juce::Button& b,bool over,bool down)
    {
        const auto r=b.getLocalBounds().toFloat().reduced(1);
        if(b.isEnabled()&&(over||down)) {
            g.setColour(ayra::tokens::textPrimary().withAlpha(down?0.12f:0.06f));
            g.fillRoundedRectangle(r,3);
        }
        if(b.hasKeyboardFocus(false)) {
            g.setColour(ayra::tokens::focusRing());g.drawRoundedRectangle(r,3,1.0f);
        }
    }
    class Chevron final:public juce::Button
    {
    public:
        explicit Chevron(bool next):Button(next?">":"<"),right(next){}
        void paintButton(juce::Graphics& g,bool over,bool down)override
        {
            feedback(g,*this,over,down);
            g.setColour(ayra::tokens::textPrimary().withAlpha(!isEnabled()?0.30f:(over||down?1.0f:0.75f)));
            g.strokePath(navigationPath(getLocalBounds().toFloat(),right),
                         juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
    private:bool right;
    };
    class Letter final:public juce::Button
    {
    public:
        explicit Letter(bool mute):Button(mute?"M":"S"),isMute(mute){}
        void paintButton(juce::Graphics& g,bool over,bool down)override
        {
            feedback(g,*this,over,down);
            const auto colour=getToggleState()?(isMute?ayra::tokens::muteIndicator():ayra::tokens::soloIndicator()):ayra::tokens::textPrimary();
            g.setColour(colour.withMultipliedAlpha(isEnabled()?1.0f:0.30f));
            g.setFont(ayra::tokens::type::body().withHeight(12.0f).boldened());
            g.drawText(getName(),getLocalBounds(),juce::Justification::centred);
        }
    private:bool isMute;
    };
};
} // namespace ayra::lab
