/** Shift-cut-rail arbitration. Normal cutting stays immediate; a Shift
    single click waits for the system double-click interval. No timer callback
    retains the canvas. Host callbacks capture/revalidate document identity. */
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>
namespace ayra::lab::clips
{
class CutRailGroupGesture final : private juce::Timer
{
public:
    using Action=std::function<void(bool wholeLane)>;
    using Prepare=std::function<Action(const std::string& clip)>;
    ~CutRailGroupGesture() override {cancel();}
    bool mouseDown(const juce::MouseEvent& e, bool rail, const std::string& clip, const Prepare& prepare)
    {
        if(!rail || !e.mods.isLeftButtonDown() || !e.mods.isShiftDown()) {cancel();return false;}
        // Always consume Shift+rail, even when the host has no grouping provider.
        const bool sameSequence=action && target==clip && e.getNumberOfClicks()>=2;
        stopTimer();down=true;cancelled=false;all=sameSequence;origin=e.position;
        if(!sameSequence) {action=prepare?prepare(clip):Action{};target=clip;}
        return true;
    }
    bool mouseDrag(const juce::MouseEvent& e)
    {
        if(!down)return false;
        if(e.position.getDistanceFrom(origin)>4.0f){cancelled=true;action={};stopTimer();}
        return true;
    }
    bool mouseUp(const juce::MouseEvent&)
    {
        if(!down)return false;
        down=false;
        if(cancelled || !action){cancel();return true;}
        if(all)deliver(true);
        else startTimer(juce::MouseEvent::getDoubleClickTimeout());
        return true;
    }
    void cancel() noexcept {stopTimer();action={};target.clear();down=false;cancelled=false;all=false;}
    bool pending()const noexcept{return down || bool(action);}
private:
    void timerCallback()override {deliver(false);}
    void deliver(bool wholeLane)
    {
        auto callback=std::move(action);cancel();
        if(callback)callback(wholeLane); // may destroy the row: do not access this afterwards
    }
    Action action;std::string target;juce::Point<float> origin;
    bool down=false,cancelled=false,all=false;
};
} // namespace ayra::lab::clips
