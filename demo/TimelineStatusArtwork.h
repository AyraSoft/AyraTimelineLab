/** Passive state readouts. Node glyphs/colours are the SAME canonical selectors
    used by MixerChannelStrip, not copied paths, buttons or an independent palette.
    Monitoring retains the Lab's existing audio glyph and the shared active-green
    token. The source profile resolves all of these from Multiverse in place. */
#pragma once
#include "../modules/ayra_timeline_panel/TimelineTrackStatus.h"

namespace ayra::lab
{
class TimelineStatusArtwork final
{
public:
    static ayra::NodeButton nodeButton(timeline_panel::TrackIndicator state)noexcept
    {
        using I=timeline_panel::TrackIndicator;
        switch(state){
            case I::shutdown:return ayra::NodeButton::shutdown;
            case I::bypass:return ayra::NodeButton::bypass;
            case I::canvas:return ayra::NodeButton::window;
            case I::bufferize:return ayra::NodeButton::buffer;
            case I::oversampling:return ayra::NodeButton::oversample;
            case I::latencyCompensation:return ayra::NodeButton::latency;
            default:return ayra::NodeButton::none;
        }
    }
    static void draw(juce::Graphics& g,timeline_panel::TrackIndicator state,juce::Rectangle<int> bounds)
    {
        if(bounds.isEmpty())return;
        const bool monitor=state==timeline_panel::TrackIndicator::monitoring;
        const auto kind=nodeButton(state);
        ayra::glyphs::drawGlyph(g,monitor?ayra::glyphs::audio:ayra::nodeButtonGlyph(kind),
            bounds.toFloat(),monitor?ayra::tokens::powerOnIndicator():ayra::nodeButtonActiveColour(kind));
    }
};
}
