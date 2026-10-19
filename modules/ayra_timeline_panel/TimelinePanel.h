#pragma once
#include "TimelineModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace ayra::timeline_panel
{
inline const char* streamName(Stream stream) noexcept
{
    switch(stream) {
        case Stream::audio:return "Audio"; case Stream::midi:return "MIDI";
        case Stream::dmx:return "DMX"; case Stream::osc:return "OSC";
        case Stream::image:return "Image"; case Stream::parameter:return "Parameter";
        case Stream::text:return "Text"; case Stream::global:return "Global";
    }
    return "Unknown";
}
inline juce::String text(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
struct PanelStyle
{
    juce::Colour background{0xff181c23}, seam{0xff222730}, foreground{0xffd8dde5}, border{0xff353c47};
    // Mandatory in the product: inject the SAME settings-backed stream resolver as Tensor.
    std::function<juce::Colour(Stream)> streamColour;
};
class TimelinePanel;
/** Stateless painter, independent from LookAndFeel_V4 and from document/command ownership. */
class TimelinePanelLookAndFeel
{
public:
    virtual ~TimelinePanelLookAndFeel()=default;
    virtual void drawLaneChrome(juce::Graphics&,const TimelinePanel&,juce::Rectangle<int>,
                                const Row&,const Owner&,const Lane*,bool selected,
                                const PanelStyle&) const=0;
};
class DefaultTimelinePanelLookAndFeel final : public TimelinePanelLookAndFeel
{
public:
    void drawLaneChrome(juce::Graphics& g,const TimelinePanel&,juce::Rectangle<int> bounds,
                        const Row& row,const Owner& owner,const Lane* lane,bool selected,
                        const PanelStyle& style) const override
    {
        g.fillAll(style.background);
        const auto seam=bounds.removeFromBottom(static_cast<int>(Controller::seamHeight));
        g.setColour(style.seam);g.fillRect(seam);
        g.setColour(juce::Colour(owner.colour));g.fillRect(0,0,selected?6:3,bounds.getHeight()+seam.getHeight());
        // Nesting uses only the fixed rail; musical X never changes with depth.
        for(int d=0;d<std::min(row.depth,4);++d)
        {g.setColour(juce::Colour(owner.colour).withAlpha(0.20f));g.fillRect(6+d*2,0,1,bounds.getHeight());}
        if(lane)
        {
            g.setColour(style.streamColour?style.streamColour(lane->stream):style.foreground);
            g.fillRect(13,0,3,bounds.getHeight());
        }
        g.setColour(style.border);g.drawHorizontalLine(seam.getBottom()-1,0.0f,static_cast<float>(seam.getRight()));
        if(selected){g.setColour(juce::Colour(owner.colour).withAlpha(0.75f));g.drawRect(seam.reduced(0,1));}
    }
};
struct CanvasFrame
{
    int width=0,height=0;
    double durationSeconds=32.0;
    std::uint64_t revision=0;
};
struct LaneCanvas
{
    std::unique_ptr<juce::Component> component;
    std::function<void(const Lane&,const CanvasFrame&)> bind;
    // False for a legacy canvas whose local events have no document projection yet.
    bool supportsDocumentEvents=true;
};
using CanvasFactory=std::function<LaneCanvas(const Lane&)>;

/** View-only JUCE surface. Factories retain the approved clip renderer/gesture ownership.
    Rows are created lazily and retained by ID: the legacy Lab keeps some curve/editor state
    inside its canvases, so evicting visited rows would lose state. This is NOT full eviction
    virtualization. Only visible rows are bound and positioned on scroll.
*/
class TimelinePanel final : public juce::Component,private juce::ScrollBar::Listener
{
public:
    static constexpr int railWidth=16,rulerHeight=28;
    explicit TimelinePanel(Controller& source,CanvasFactory makeCanvas)
        : controller(source),factory(std::move(makeCanvas)),viewport(*this),horizontal(false)
    {
        setWantsKeyboardFocus(true);
        viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);
        horizontal.addListener(this);addAndMakeVisible(viewport);addAndMakeVisible(horizontal);
    }
    ~TimelinePanel() override
    {
        controller.endResize(false);horizontal.removeListener(this);viewport.setViewedComponent(nullptr,false);
    }
    std::function<void(double)> onSeek;
    std::function<void(const Result&)> onResult;
    std::function<void()> onSelectionChanged;
    void setStyle(PanelStyle value){style=std::move(value);refresh();}
    void setPanelLookAndFeel(const TimelinePanelLookAndFeel* value){painter=value?value:&fallback;refresh();}
    void setPixelsPerSecond(double value)
    {if(std::isfinite(value)){pixelsPerSecond=std::clamp(value,12.0,500.0);resized();}}
    void setTimeExtent(double seconds)
    {if(std::isfinite(seconds)&&seconds>0.0&&seconds<=86400.0){durationSeconds=seconds;resized();}}
    void setPlayhead(double seconds)
    {if(std::isfinite(seconds)){playhead=std::max(0.0,seconds);repaint();}}
    double getPlayhead()const noexcept{return playhead;}
    void revealLane(const Id& id)
    {
        for(const auto& row:controller.rows())if(row.lane==id)
        {viewport.setViewPosition(0,static_cast<int>(row.top));break;}
    }
    void refresh()
    {
        if(updating)return;
        const juce::ScopedValueSetter<bool> guard(updating,true);
        const int width=std::max(1,viewport.getMaximumVisibleWidth());
        content.setSize(width,std::max(1,static_cast<int>(std::ceil(controller.totalHeight()))));
        const auto range=controller.visible(viewport.getViewPositionY(),viewport.getMaximumVisibleHeight(),64);
        std::set<Id> visible;
        for(std::size_t i=range.first;i<range.second;++i)
        {
            const auto& row=controller.rows()[i];
            const auto cacheKey=std::make_pair(controller.snapshot().session,row.key);visible.insert(row.key);
            auto& component=cache[cacheKey];
            if(!component){component=std::make_unique<LaneRow>(*this);content.addAndMakeVisible(*component);}
            component->setVisible(true);component->setBounds(0,static_cast<int>(row.top),width,static_cast<int>(row.height));
            component->bindRow(row);
        }
        for(auto& [key,component]:cache)
            if((key.first!=controller.snapshot().session || !visible.contains(key.second))
               && !component->isMouseButtonDown(true)) component->setVisible(false);
        repaint();
    }
    void resized() override
    {
        auto area=getLocalBounds();area.removeFromTop(rulerHeight);
        horizontal.setBounds(area.removeFromBottom(16));viewport.setBounds(area);
        canvasWidth=std::max(1,static_cast<int>(std::round(durationSeconds*pixelsPerSecond)));
        const int visible=std::max(1,viewport.getMaximumVisibleWidth()-railWidth);
        canvasWidth=std::max(canvasWidth,visible);
        horizontal.setRangeLimits(0.0,static_cast<double>(canvasWidth));
        horizontal.setCurrentRange(std::clamp(offset,0.0,static_cast<double>(std::max(0,canvasWidth-visible))),visible,juce::dontSendNotification);
        offset=horizontal.getCurrentRangeStart();refresh();
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(key==juce::KeyPress::escapeKey && controller.resizing())
        {controller.endResize(false);refresh();return true;}
        return false;
    }
    void visibilityChanged() override
    {if(!isShowing() && controller.resizing()){controller.endResize(false);refresh();}}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(style.background);g.setColour(style.seam);g.fillRect(0,0,getWidth(),rulerHeight);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        const double scale=canvasWidth/durationSeconds;
        for(int second=0;second<=static_cast<int>(durationSeconds);++second)
        {
            const int x=railWidth+static_cast<int>(second*scale-offset);if(x<railWidth || x>getWidth())continue;
            g.setColour(style.border);g.drawVerticalLine(x,18.0f,static_cast<float>(rulerHeight));
            if(second%2==0){g.setColour(style.foreground);g.drawText(juce::String(second)+" s",x+3,2,42,17,juce::Justification::left);}
        }
    }
    void paintOverChildren(juce::Graphics& g) override
    {
        const float x=static_cast<float>(railWidth+playhead*(canvasWidth/durationSeconds)-offset);
        if(x>=railWidth && x<viewport.getRight())
        {g.setColour(style.foreground.withAlpha(0.70f));g.drawVerticalLine(juce::roundToInt(x),0.0f,static_cast<float>(viewport.getBottom()));}
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(e.y<rulerHeight && e.x>=railWidth)
            seek((e.position.x-railWidth+offset)/(canvasWidth/durationSeconds));
    }
private:
    class Viewport final:public juce::Viewport
    {
    public:explicit Viewport(TimelinePanel& p):panel(p){}
        void visibleAreaChanged(const juce::Rectangle<int>&)override{panel.refresh();}
    private:TimelinePanel& panel;
    };
    class LaneRow final:public juce::Component
    {
    public:
        explicit LaneRow(TimelinePanel& panel):host(panel)
        {
            setWantsKeyboardFocus(true);addAndMakeVisible(slot);
            for(auto* button:{&previous,&next,&mute,&solo,&monitor,&open,&fold})addAndMakeVisible(*button);
            previous.onClick=[this]{navigate(false);};next.onClick=[this]{navigate(true);};
            mute.onClick=[this]{const auto* o=host.controller.owner(row.owner);if(o)control(CommandKind::mute,!o->muted);};
            solo.onClick=[this]{const auto* o=host.controller.owner(row.owner);if(o)control(CommandKind::solo,!o->soloed);};
            open.onClick=[this]{control(CommandKind::openOwner);};
            fold.onClick=[this]{host.controller.toggleCollapse(row.owner);host.refresh();};
            monitor.onClick=[this]{showMonitoring();};
            previous.setTooltip("Evento precedente in questa lane (owner e discendenti se ripiegato)");
            next.setTooltip("Evento successivo in questa lane (owner e discendenti se ripiegato)");
            mute.setTooltip("Mute della TRACCIA: condiviso da tutte le sue lane");
            solo.setTooltip("Solo della TRACCIA: condiviso da tutte le sue lane");
            monitor.setTooltip("Monitoring della TRACCIA: una sola policy condivisa");
            open.setTooltip("Apri il Tensor/traccia proprietario, non l'editor della clip");
            fold.setTooltip("Ripiega/espandi la struttura, senza spegnere il MIX");
        }
        void bindRow(const Row& value)
        {
            row=value;boundSession=host.controller.snapshot().session;
            const auto* lane=host.controller.lane(row.lane);
            if(lane && !canvas.component && host.factory)
            {
                canvas=host.factory(*lane);
                if(canvas.component)slot.addAndMakeVisible(*canvas.component);
            }
            const CanvasFrame frame{host.canvasWidth,lane?host.controller.height(lane->id):0,
                                    host.durationSeconds,host.controller.snapshot().revision};
            if(lane && canvas.bind && (lastFrame.width!=frame.width || lastFrame.height!=frame.height
                || lastFrame.revision!=frame.revision || lastFrame.durationSeconds!=frame.durationSeconds))
            {canvas.bind(*lane,frame);lastFrame=frame;}
            const bool navigationAvailable=row.summary || canvas.supportsDocumentEvents;
            previous.setEnabled(navigationAvailable);next.setEnabled(navigationAvailable);
            if(!navigationAvailable){previous.setTooltip("Eventi locali legacy: navigazione documentale non ancora collegata");next.setTooltip(previous.getTooltip());}
            resized();updateControls();repaint();
        }
        void resized()override
        {
            auto bounds=getLocalBounds();auto seam=bounds.removeFromBottom(static_cast<int>(Controller::seamHeight));
            slot.setBounds(bounds.withTrimmedLeft(railWidth));
            if(canvas.component)canvas.component->setBounds(-static_cast<int>(host.offset),0,host.canvasWidth,slot.getHeight());
            int x=railWidth+4;
            fold.setBounds(x,seam.getY()+2,18,18);x+=20;
            // Bounded label region: long names never claim a horizontal header column.
            nameBounds={x,seam.getY()+1,150,18};x+=154;
            for(auto* button:{&previous,&next,&mute,&solo}){button->setBounds(x,seam.getY()+2,22,18);x+=24;}
            monitor.setBounds(x,seam.getY()+2,48,18);x+=50;open.setBounds(x,seam.getY()+2,38,18);
        }
        void paint(juce::Graphics& g)override
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            const auto* lane=host.controller.lane(row.lane);
            const bool selected=host.controller.view().selectedLanes.contains(row.lane);
            host.painter->drawLaneChrome(g,host,getLocalBounds(),row,*owner,lane,selected,host.style);
            g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
            g.setColour(host.style.foreground);
            juce::String caption=text(owner->name);
            if(lane)
            {
                if(!row.firstForOwner && host.controller.view().mode==Mode::structured)caption.clear();
                if(caption.isNotEmpty())caption+=" / ";
                caption+=text(lane->name);
            }
            g.drawFittedText(caption,nameBounds,juce::Justification::centredLeft,1,0.8f);
            // Relation provenance is drawn in the separator, never on clip handles.
            int badgesX=open.getRight()+8;
            for(const auto& relation:host.controller.snapshot().relations)
                if(std::any_of(relation.members.begin(),relation.members.end(),[&](const auto& ref){return ref.lane==row.lane;}))
                {
                    g.setColour(host.style.foreground.withAlpha(0.75f));
                    g.drawText(relation.kind==RelationKind::temporalGroup?"GROUP":"LINK",badgesX,getHeight()-23,44,18,juce::Justification::centred);
                    badgesX+=46;
                }
            if(host.dropAnchor==row.lane && !row.lane.empty())
            {g.setColour(juce::Colour(owner->colour));g.drawRect(getLocalBounds().removeFromBottom(24),2);}
        }
        void mouseEnter(const juce::MouseEvent&)override{updateControls();}
        void mouseExit(const juce::MouseEvent&)override{updateControls();}
        void mouseMove(const juce::MouseEvent& e)override
        {
            updateControls();setMouseCursor(e.y>=getHeight()-4?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);
        }
        void mouseDown(const juce::MouseEvent& e)override
        {
            if(!e.mods.isLeftButtonDown())return;
            grabKeyboardFocus();startScreenY=e.getScreenY();dragging=false;ownsResize=false;
            if(!row.lane.empty() && e.y>=getHeight()-4)
            {ownsResize=host.controller.beginResize(row.lane);return;}
            if(row.lane.empty())return;
            const auto how=e.mods.isShiftDown()?Controller::Selection::range:
                (e.mods.isCommandDown()||e.mods.isCtrlDown())?Controller::Selection::toggle:Controller::Selection::replace;
            if(how!=Controller::Selection::replace || !host.controller.view().selectedLanes.contains(row.lane))
                host.controller.select(row.lane,how);
            dragEnabled=e.x<railWidth;
            host.refresh();if(host.onSelectionChanged)host.onSelectionChanged();
        }
        void mouseDrag(const juce::MouseEvent& e)override
        {
            if(ownsResize){host.controller.updateResize(e.getScreenY()-startScreenY);host.refresh();return;}
            if(!dragEnabled || row.lane.empty() || std::abs(e.getScreenY()-startScreenY)<6)return;
            dragging=true;host.findDrop(e.getScreenPosition());
        }
        void mouseUp(const juce::MouseEvent&)override
        {
            if(ownsResize){host.controller.endResize(true);ownsResize=false;host.refresh();return;}
            if(dragging && !host.dropAnchor.empty())
            {
                const auto& selected=host.controller.view().selectedLanes;
                host.report(host.controller.moveBefore({selected.begin(),selected.end()},host.dropAnchor));
            }
            dragging=false;dragEnabled=false;host.dropAnchor.clear();host.refresh();
        }
        void mouseDoubleClick(const juce::MouseEvent&)override
        {
            if(row.lane.empty())host.controller.toggleCollapse(row.owner);
            else {const auto h=host.controller.height(row.lane);host.controller.setHeight(row.lane,h<256?256:h<400?400:96);}
            host.refresh();
        }
        bool keyPressed(const juce::KeyPress& key)override{return host.keyPressed(key);}
    private:
        TimelinePanel& host;Row row;LaneCanvas canvas;juce::Component slot;Id boundSession;
        CanvasFrame lastFrame{-1,-1,0,0};
        juce::TextButton previous{"<"},next{">"},mute{"M"},solo{"S"},monitor{"Auto"},open{"Open"},fold{"v"};
        juce::Rectangle<int> nameBounds;bool ownsResize=false,dragging=false,dragEnabled=false;int startScreenY=0;
        void updateControls()
        {
            const auto* o=host.controller.owner(row.owner);if(!o)return;
            const bool active=host.controller.view().activeLane==row.lane || isMouseOverOrDragging(true) || row.summary;
            mute.setVisible(o->capabilities.mute&&(active||o->muted));mute.setToggleState(o->muted,juce::dontSendNotification);
            solo.setVisible(o->capabilities.solo&&(active||o->soloed));solo.setToggleState(o->soloed,juce::dontSendNotification);
            monitor.setVisible(o->capabilities.monitoring&&(active||o->monitoring!=Monitoring::automatic));
            const char* name=o->monitoring==Monitoring::live?"Live":o->monitoring==Monitoring::playback?"Play":o->monitoring==Monitoring::merge?"Merge":"Auto";
            monitor.setButtonText(name);open.setVisible(o->capabilities.open&&active);
            fold.setVisible(host.controller.view().mode==Mode::structured&&(row.firstForOwner||row.summary));
            fold.setButtonText(host.controller.view().collapsedOwners.contains(row.owner)?">":"v");
        }
        void navigate(bool forward)
        {
            const auto target=row.lane.empty()?host.controller.adjacentOwnerEvent(row.owner,host.playhead,forward):
                host.controller.adjacentEvent(row.lane,host.playhead,forward);
            if(target)host.seek(*target);
        }
        void control(CommandKind kind,bool value=false,Monitoring policy=Monitoring::automatic)
        {
            if(boundSession!=host.controller.snapshot().session){host.report(Result::fail("Stale row session"));return;}
            host.report(host.controller.controlOwner(row.owner,kind,value,policy,row.lane));
        }
        void showMonitoring()
        {
            juce::PopupMenu menu;menu.addItem(1,"Auto");menu.addItem(2,"Live");menu.addItem(3,"Playback");menu.addItem(4,"Merge");
            juce::Component::SafePointer<LaneRow> safe(this);
            const auto session=host.controller.snapshot().session;const auto revision=host.controller.snapshot().revision;
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&monitor),[safe,session,revision](int chosen)
            {if(safe!=nullptr && chosen>=1 && chosen<=4)
                {if(safe->host.controller.snapshot().session!=session || safe->host.controller.snapshot().revision!=revision)
                    {safe->host.report(Result::fail("Monitoring menu expired; reopen it"));return;}
                 safe->control(CommandKind::monitoring,false,static_cast<Monitoring>(chosen-1));}});
        }
    };
    Controller& controller;CanvasFactory factory;PanelStyle style;
    DefaultTimelinePanelLookAndFeel fallback;const TimelinePanelLookAndFeel* painter=&fallback;
    Viewport viewport;juce::Component content;juce::ScrollBar horizontal;
    std::map<std::pair<Id,Id>,std::unique_ptr<LaneRow>> cache;
    double pixelsPerSecond=64,durationSeconds=32,offset=0,playhead=0;int canvasWidth=2048;
    bool updating=false;Id dropAnchor;
    void scrollBarMoved(juce::ScrollBar*,double start)override{offset=start;refresh();}
    void report(const Result& result){if(onResult)onResult(result);}
    void seek(double seconds)
    {
        setPlayhead(seconds);const double x=seconds*(canvasWidth/durationSeconds);
        const double visible=std::max(1,viewport.getMaximumVisibleWidth()-railWidth);
        if(x<offset || x>offset+visible)horizontal.setCurrentRangeStart(std::clamp(x-visible*0.4,0.0,std::max(0.0,canvasWidth-visible)));
        if(onSeek)onSeek(seconds);
    }
    void findDrop(juce::Point<int> screen)
    {
        const auto local=viewport.getLocalPoint(nullptr,screen);
        if(local.y<24)viewport.setViewPosition(0,std::max(0,viewport.getViewPositionY()-12));
        else if(local.y>viewport.getHeight()-24)viewport.setViewPosition(0,viewport.getViewPositionY()+12);
        const double y=local.y+viewport.getViewPositionY();dropAnchor.clear();
        const auto found=controller.visible(y,1);
        if(found.first<found.second)dropAnchor=controller.rows()[found.first].lane;
        for(auto& [id,row]:cache){(void)id;if(row->isVisible())row->repaint();}
    }
};
}
