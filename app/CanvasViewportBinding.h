#pragma once
#include <ayra_timeline/ayra_timeline_presentation.h>
#include <unordered_set>
#include <memory>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace ayra::lab
{
struct ClipSplitView
{
    std::string leftClipId,rightClipId;
    float fraction=0.5f;
};

/** Adapter only: converts the legacy 32-second fixture's geometry using the
    canonical time window. Never scales or widens the JUCE canvas component. */
struct CanvasViewportBinding
{
    bool enabled=false, selected=false;
    ayra::timeline::TimelineTimeWindow time;
    juce::Colour ownerColour;
    std::function<void()> cycleHeight; // presentation-only; installed by the host
    std::function<std::function<void()>(const std::string&)> prepareGlue;
    std::function<void(const std::string&,juce::Component&,juce::Rectangle<int>)> openContainerEditor;
    std::shared_ptr<const std::unordered_set<std::string>> canonicalClipIds;
    std::uint64_t documentProjectionRevision=std::numeric_limits<std::uint64_t>::max();
    std::vector<ClipSplitView> clipSplits;
    bool containsCanonicalClip(const std::string& id)const
    {return canonicalClipIds&&canonicalClipIds->contains(id);}
    double pixelsPerSecond(int width)const noexcept
    {return enabled?time.pixelsPerSecond:std::max(1,width)/32.0;}
    double timeToX(double value,int width)const noexcept
    {return enabled?time.timeToX(value):value*pixelsPerSecond(width);}
    double xToTime(double value,int width)const noexcept
    {return enabled?time.xToTime(value):value/pixelsPerSecond(width);}
    // Geometry may extend beyond the viewport; the component itself never does.
    juce::Rectangle<float> domainBounds(juce::Rectangle<float> legacy)const noexcept
    {
        if(!enabled)return legacy;
        return legacy.withX(static_cast<float>(time.timeToX(0)))
                     .withWidth(static_cast<float>(32.0*time.pixelsPerSecond));
    }
    template<class Look>void paintGrid(juce::Graphics& g,const Look& laf,juce::Rectangle<int> bounds,double pps)const
    {
        laf.drawTrackGrid(g,bounds,pps,enabled?time.timeToX(0):0.0);
        if(enabled&&selected){g.setColour(ownerColour.withAlpha(0.18f));g.fillRect(bounds);}
    }
};
}
