/** View-only floating identity. Material follows fixtures/frosted-lens.html.
    Only the actual track title and navigation targets intercept input.
    Performer/type/status remain passive; expanded names do not reserve space.
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
    bool expanded = false; // transient hover/editor state, never document layout
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
    juce::Rectangle<int> previous,next,navigationGroup,mute,solo,titleHit;
    std::array<juce::Rectangle<int>,trackIndicatorCount> indicators{};
    bool twoLines = false, titleTruncated = false, onLeft = false;
    juce::Font trackFont {juce::FontOptions{}}, performerFont {juce::FontOptions{}}, typeFont {juce::FontOptions{}};
    static constexpr int maxWidth=255,minWidth=144,normalHeight=40;
    static constexpr int navigationWidth=16,navigationHeight=18,indicatorSize=6,indicatorGap=2;
    static int measuredWidth(const juce::Font& font, const juce::String& text)
    {
        if(text.isEmpty())return 0;
        return juce::GlyphArrangement::getStringWidthInt(font,text)+2;
    }
    [[nodiscard]] static TimelineIdentityLayout make(juce::Rectangle<int> area,
        const TimelineIdentityContent& c,const TimelineIdentityTypography& fonts)
    {
        TimelineIdentityLayout out;
        out.twoLines=!c.compact&&area.getHeight()>=normalHeight;
        out.onLeft=c.onLeft;
        out.trackFont=out.twoLines?fonts.track:fonts.type;
        out.performerFont=fonts.performer;out.typeFont=fonts.type;
        if(area.isEmpty())return out;
        const auto active=activeTrackIndicators(c.status);
        const int count=static_cast<int>(std::count(active.begin(),active.end(),true));
        // In a 22px lens the status glyphs use ONE row below a shorter arrow
        // target. At 40px they use at most 4 columns x 2 rows. Never grow a lane
        // to accommodate chrome, never steal the title's row for status icons.
        const int columns=out.twoLines?juce::jmin(4,count):count;
        const int statusWidth=columns?columns*indicatorSize+(columns-1)*indicatorGap:0;
        const int serviceWidth=juce::jmax(c.navigation?2*navigationWidth+2:0,statusWidth);
        const int typeWidth=c.showType&&!c.type.isEmpty()?measuredWidth(fonts.type,c.type):0;
        const int typeGroup=typeWidth?typeWidth+9:0;
        const int perfWidth=measuredWidth(fonts.performer,c.performer.toUpperCase());
        const int titleWidth=measuredWidth(out.trackFont,c.track);
        const int letters=(c.muted?measuredWidth(out.trackFont,"M")+4:0)+(c.soloed?measuredWidth(out.trackFont,"S")+4:0);
        const int natural=out.twoLines?juce::jmax(perfWidth+(perfWidth&&typeGroup?10:0)+typeGroup,titleWidth+letters)
                                     :titleWidth+letters+typeGroup+(typeGroup?8:0)+(perfWidth?perfWidth+8:0)+(c.compact&&c.showType?9:0);
        const int inset=juce::jmin(8,area.getWidth()/8);
        const int available=juce::jmax(0,area.getWidth()-2*inset);
        const int limit=c.expanded?available:juce::jmin(available,maxWidth);
        const int width=juce::jmin(limit,juce::jmax(minWidth,natural+38+(serviceWidth?serviceWidth+4:0)));
        const int height=juce::jmin(area.getHeight(),out.twoLines?normalHeight:22);
        const int top=area.getY()+(area.getHeight()-height)/2;
        out.backdrop={c.onLeft?area.getX()+inset:area.getRight()-inset-width,top,width,height};
        const auto inner=out.backdrop.withTrimmedLeft(juce::jmin(c.onLeft?14:27,width/5))
                                    .withTrimmedRight(juce::jmin(c.onLeft?27:14,width/5));
        if(inner.isEmpty())return out;
        auto textArea=inner;
        // Keep a full-width text sliver even at hostile widths; empty targets
        // never become invisible input interceptors.
        const int service=juce::jmin(serviceWidth,juce::jmax(0,inner.getWidth()-12));
        auto utilities=textArea.removeFromRight(service);
        if(service>0)textArea.removeFromRight(juce::jmin(4,textArea.getWidth()));
        const int navH=(!out.twoLines&&count>0)?12:navigationHeight;
        if(c.navigation&&utilities.getWidth()>=2*navigationWidth+2&&height>=navH) {
            const int navY=(out.twoLines||count>0)?top+1:top+(height-navH)/2;
            out.next={utilities.getRight()-navigationWidth,navY,navigationWidth,navH};
            out.previous=out.next.translated(-navigationWidth-2,0);
            out.navigationGroup=out.previous.getUnion(out.next);
        }
        if(count>0&&utilities.getWidth()>=statusWidth) {
            const int rows=(count+columns-1)/columns;
            const int gridH=rows*indicatorSize+(rows-1)*indicatorGap;
            const int y=out.navigationGroup.isEmpty()?top+(height-gridH)/2
                       :out.twoLines?top+22:top+14;
            int n=0;
            for(std::size_t i=0;i<active.size();++i)if(active[i]) {
                const juce::Rectangle<int> r(utilities.getRight()-statusWidth+(n%columns)*(indicatorSize+indicatorGap),
                    y+(n/columns)*(indicatorSize+indicatorGap),indicatorSize,indicatorSize);
                if(out.backdrop.contains(r))out.indicators[i]=r;
                ++n;
            }
        }
        auto title=out.twoLines?juce::Rectangle<int>(textArea.getX(),top+19,textArea.getWidth(),15)
                               :juce::Rectangle<int>(textArea.getX(),top+(height-15)/2,textArea.getWidth(),15);
        auto meta=out.twoLines?juce::Rectangle<int>(textArea.getX(),top+5,textArea.getWidth(),12):title;
        if(typeGroup>0&&meta.getWidth()>=typeGroup+12) {
            auto group=meta.removeFromRight(typeGroup);
            out.typeMark={group.getX(),group.getCentreY()-2,5,5};out.type=group.withTrimmedLeft(9);
            meta.removeFromRight(juce::jmin(out.twoLines?10:8,meta.getWidth()));
            if(!out.twoLines)title=meta;
        }
        if(out.twoLines)out.performer=meta;
        else if(!c.performer.isEmpty()&&title.getWidth()>48) {
            const int titleBudget=juce::jmin(titleWidth+letters,juce::jmax(32,title.getWidth()/2));
            const int performerWidth=juce::jmin(perfWidth,juce::jmax(0,title.getWidth()-titleBudget-8));
            out.performer=title.removeFromLeft(performerWidth);title.removeFromLeft(juce::jmin(8,title.getWidth()));
        }
        if(c.compact&&c.showType&&c.type.isEmpty()&&title.getWidth()>14) {
            out.typeMark={title.getX(),title.getCentreY()-2,5,5};title.removeFromLeft(9);
        }
        // M/S follow the title, before the separate navigation/status column.
        const auto letter=[&](bool on,const char* value,juce::Rectangle<int>& rect) {
            if(!on)return;
            const int w=measuredWidth(out.trackFont,value);
            if(title.getWidth()>=w+8){rect=title.removeFromRight(w);title.removeFromRight(4);}
        };
        letter(c.soloed,"S",out.solo);letter(c.muted,"M",out.mute);
        out.track=title.getIntersection(out.backdrop);
        out.titleTruncated=titleWidth>out.track.getWidth();
        // Only the actually drawn name (including an ellipsis) is protected.
        // Performer, type, status glyphs and unused padding remain pass-through.
        out.titleHit=out.track.withLeft(out.track.getRight()-juce::jmin(titleWidth,out.track.getWidth()));
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
    bool update(bool inside,bool editable,bool protectedTarget,double now,bool editing=false)noexcept
    {
        const bool before=suppressed;
        if(!inside){reset();return before;}
        if(suppressed)return false;
        if(protectedTarget){due.reset();return false;}
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
