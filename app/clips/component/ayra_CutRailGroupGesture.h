/** Arbitraggio Shift sulla Cut rail. Il Glue viene consegnato al rilascio
    dello stesso clic; un drag oltre soglia annulla il gesto. Il secondo clic
    di una sequenza doppia viene solo consumato, senza una seconda mutazione. */
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>
namespace ayra::lab::clips
{
class CutRailGroupGesture final
{
public:
    using Action=std::function<void()>;
    using Prepare=std::function<Action(const std::string& clip)>;
    bool mouseDown(const juce::MouseEvent& e, bool rail, const std::string& clip, const Prepare& prepare)
    {
        if(!rail || !e.mods.isLeftButtonDown() || !e.mods.isShiftDown()) {cancel();return false;}
        cancel();down=true;origin=e.position;
        // Il primo clic ha gia' eseguito Glue: il secondo non deve annidare
        // accidentalmente il container appena creato con la clip seguente.
        if(e.getNumberOfClicks()>=2) {cancelled=true;return true;}
        action=prepare?prepare(clip):Action{};
        return true;
    }
    bool mouseDrag(const juce::MouseEvent& e)
    {
        if(!down)return false;
        if(e.position.getDistanceFrom(origin)>4.0f){cancelled=true;action={};}
        return true;
    }
    bool mouseUp(const juce::MouseEvent&)
    {
        if(!down)return false;
        down=false;
        if(cancelled || !action){cancel();return true;}
        deliver();
        return true;
    }
    void cancel() noexcept {action={};down=false;cancelled=false;}
    bool pending()const noexcept{return down || bool(action);}
private:
    void deliver()
    {
        auto callback=std::move(action);cancel();
        if(callback)callback(); // Puo' distruggere la row: non accedere a this dopo.
    }
    Action action;juce::Point<float> origin;
    bool down=false,cancelled=false;
};
} // namespace ayra::lab::clips
