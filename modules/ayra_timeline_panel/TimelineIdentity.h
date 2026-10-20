/** View-only floating identity. Material follows fixtures/frosted-lens.html.
    Names/status are passive. Only the two navigation targets intercept input.
    Geometry is viewport-local and never reserves row height or musical time. */
#pragma once
#include "TimelineTrackStatus.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <optional>

namespace ayra::timeline_panel
{
struct TimelineIdentityContent
{
    juce::String performer, track, type;
    juce::Colour ownerColour, typeColour;
    bool compact = false, onLeft = false, selected = false, showType = true;
    bool navigation = false, muted = false, soloed = false;
    TrackStatus status{};
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
    juce::Rectangle<int> backdrop, performer, track, type, typeMark;
    juce::Rectangle<int> previous,next,navigationGroup,mute,solo;
    std::array<juce::Rectangle<int>,trackIndicatorCount> indicators{};
    bool twoLines = false;
    juce::Font trackFont {juce::FontOptions{}}, performerFont {juce::FontOptions{}}, typeFont {juce::FontOptions{}};
    static constexpr int maxWidth=229,minWidth=144,normalHeight=40;
    static constexpr int navigationWidth=16,navigationHeight=18,indicatorSize=6,indicatorGap=3;
    static int measuredWidth(const juce::Font& font, const juce::String& text)
    {
        if(text.isEmpty())return 0;
        return juce::GlyphArrangement::getStringWidthInt(font,text)+2;
    }
    [[nodiscard]] static TimelineIdentityLayout make(juce::Rectangle<int> area,
        const TimelineIdentityContent& c,const TimelineIdentityTypography& fonts)
    {
        TimelineIdentityLayout out;
        out.trackFont=c.compact?fonts.type:fonts.track;
        out.performerFont=fonts.performer;out.typeFont=fonts.type;
        if(area.isEmpty())return out;
        out.twoLines=!c.compact&&area.getHeight()>=normalHeight;
        const int inset=juce::jmin(8,area.getWidth()/8);
        const int available=juce::jmax(0,area.getWidth()-2*inset);
        const int typeWidth=c.showType&&!c.compact?measuredWidth(fonts.type,c.type):0;
        const int typeGroup=typeWidth>0?typeWidth+9:0;
        const int navWidth=c.navigation?2*navigationWidth+2:0;
        const int meta=measuredWidth(fonts.performer,c.performer.toUpperCase())+10+typeGroup+navWidth+4;
        // Indicators share the existing lower line. The fixed maximum remains
        // 229px even with all states active; long titles elide, never shrink.
        const auto active=activeTrackIndicators(c.status);
        const int statusWidth=static_cast<int>(std::count(active.begin(),active.end(),true))*(indicatorSize+indicatorGap);
        const int letters=(c.muted?measuredWidth(out.trackFont,"M")+4:0)+(c.soloed?measuredWidth(out.trackFont,"S")+4:0);
        const int natural=out.twoLines?juce::jmax(meta,measuredWidth(out.trackFont,c.track)+statusWidth+letters+navWidth+10)
                                      :measuredWidth(out.trackFont,c.track)+typeGroup+navWidth+14;
        const int width=juce::jmin(available,juce::jlimit(minWidth,maxWidth,natural+38));
        const int height=juce::jmin(area.getHeight(),out.twoLines?normalHeight:22);
        const int top=area.getY()+(area.getHeight()-height)/2;
        out.backdrop={c.onLeft?area.getX()+inset:area.getRight()-inset-width,top,width,height};
        const auto inner=out.backdrop.withTrimmedLeft(juce::jmin(24,width/5)).withTrimmedRight(juce::jmin(14,width/5));
        if(inner.isEmpty())return out;
        auto metaRow=juce::Rectangle<int>(inner.getX(),out.twoLines?top+2:top+(height-navigationHeight)/2,
                                        inner.getWidth(),juce::jmin(height,navigationHeight)).getIntersection(out.backdrop);
        if(c.navigation&&metaRow.getWidth()>=2*navigationWidth+24&&height>=navigationHeight) {
            out.next=metaRow.removeFromRight(navigationWidth);
            metaRow.removeFromRight(2);
            out.previous=metaRow.removeFromRight(navigationWidth);
            out.navigationGroup=out.previous.getUnion(out.next);metaRow.removeFromRight(4);
        }
        auto title=out.twoLines?juce::Rectangle<int>(inner.getX(),top+19,inner.getWidth(),15):metaRow;
        if(out.twoLines&&typeGroup>0&&metaRow.getWidth()>=14) {
            auto group=metaRow.removeFromRight(juce::jmin(typeGroup,metaRow.getWidth()));
            out.typeMark={group.getX(),group.getCentreY()-2,5,5};out.type=group.withTrimmedLeft(9).withSizeKeepingCentre(juce::jmax(0,group.getWidth()-9),12);
            metaRow.removeFromRight(10);
        }
        if(out.twoLines&&!c.performer.isEmpty())out.performer=metaRow.withSizeKeepingCentre(metaRow.getWidth(),12);
        if(out.twoLines&&!out.type.isEmpty())title.setRight(out.type.getRight());
        if(c.compact&&c.showType&&title.getWidth()>=14) {
            out.typeMark={title.getX(),title.getCentreY()-2,5,5};title.removeFromLeft(9);
        } else if(!out.twoLines&&typeGroup>0&&title.getWidth()>typeGroup+60) {
            auto group=title.removeFromRight(typeGroup);
            out.typeMark={group.getX(),group.getCentreY()-2,5,5};out.type=group.withTrimmedLeft(9);
            title.removeFromRight(10);
        }
        // Explicit flags only, independent of side-button visibility or policy.
        // Both M and S can be present. They have exactly the title's typography.
        const auto letter=[&](bool active,const char* value,juce::Rectangle<int>& rect) {
            if(!active)return;
            const int w=measuredWidth(out.trackFont,value);
            if(title.getWidth()>=w+8){rect=title.removeFromRight(w);title.removeFromRight(4);}
        };
        letter(c.soloed,"S",out.solo);letter(c.muted,"M",out.mute);
        auto flags=activeTrackIndicators(c.status);
        int count=static_cast<int>(std::count(flags.begin(),flags.end(),true));
        // Seven 6px glyphs take 60px at most. No rectangles for inactive flags.
        if(count>0&&title.getWidth()>=count*(indicatorSize+indicatorGap)+12) {
            for(std::size_t i=0;i<flags.size();++i)if(flags[i]) {
                out.indicators[i]={title.getX(),title.getCentreY()-indicatorSize/2,indicatorSize,indicatorSize};
                title.removeFromLeft(indicatorSize+indicatorGap);
            }
            title.removeFromLeft(3);
        }
        out.track=title.getIntersection(out.backdrop);
        return out;
    }
};

/** Pure hover state; the PANEL owns one wake-up timer shared with optional
    meters. A deadline is not restarted by each mouse move. Once hidden, the
    same footprint stays suppressed until exit (no invisible navigation hits). */
class TimelineIdentityHoverState
{
public:
    static constexpr double delayMilliseconds=500.0;
    [[nodiscard]] bool hidden()const noexcept{return suppressed;}
    [[nodiscard]] std::optional<double> deadline()const noexcept{return due;}
    bool update(bool inside,bool editable,bool navigation,double now,bool editing=false)noexcept
    {
        const bool before=suppressed;
        if(!inside){reset();return before;}
        if(suppressed)return false;
        if(navigation){due.reset();return false;}
        if(!editable){due.reset();return false;}
        if(editing){suppressed=true;due.reset();return true;}
        if(!due)due=now+delayMilliseconds;
        if(now>=*due){suppressed=true;due.reset();}
        return before!=suppressed;
    }
    void reset()noexcept{suppressed=false;due.reset();}
private:
    bool suppressed=false;
    std::optional<double> due;
};
} // namespace ayra::timeline_panel
