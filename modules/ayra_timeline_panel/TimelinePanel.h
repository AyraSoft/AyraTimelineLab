#pragma once
#include "TimelineModel.h"
#include "TimelineIdentity.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace ayra::timeline_panel
{
namespace presentation = ayra::timeline;
using LaneButton = presentation::TimelineLaneButton;
using ChromeGeometry = presentation::TimelineLaneChromeGeometry;
inline juce::Rectangle<int> rectangle(presentation::TimelineChromeRect r)
{return {r.x,r.y,r.width,r.height};}
inline const char* streamName(Stream s) noexcept
{
    switch(s){case Stream::audio:return "Audio";case Stream::midi:return "MIDI";
        case Stream::dmx:return "DMX";case Stream::osc:return "OSC";case Stream::image:return "Image";
        case Stream::parameter:return "Parameter";case Stream::text:return "Text";case Stream::global:return "Global";}
    return "Unknown";
}
inline juce::String text(const std::string& s){return juce::String::fromUTF8(s.c_str());}
struct PanelStyle
{
    juce::Colour background{0xff181c23},seam{0xff222730},foreground{0xffd8dde5},border{0xff353c47};
    TimelineIdentityTypography identityTypography;
    std::function<juce::Colour(Stream)> streamColour;
};
/** Native artwork is supplied by the host's existing Ayra design system.
    No Tensor, Lab or Multiverse types belong to this interface. */
class TimelinePanelLookAndFeel
{
public:
    virtual ~TimelinePanelLookAndFeel()=default;
    virtual std::unique_ptr<juce::Button> makeButton(LaneButton)=0;
    virtual void drawAccent(juce::Graphics&,juce::Rectangle<float>,juce::Colour)const=0;
    virtual void drawMeter(juce::Graphics&,juce::Rectangle<float>,float)const=0;
    virtual juce::Image prepareIdentity(const TimelineIdentityContent&, const TimelineIdentityLayout&) const { return {}; }
    virtual void drawIdentity(juce::Graphics& g, const TimelineIdentityContent& c,
                              const TimelineIdentityLayout& layout, const juce::Image&) const
    {
        g.setColour(juce::Colours::white); g.setFont(layout.primaryFont);
        g.drawText(c.primary,layout.primary,juce::Justification::centredRight,true);
    }
};
struct CanvasFrame
{
    int width=0,height=0;
    double durationSeconds=32.0;
    std::uint64_t revision=0;
    presentation::TimelineTimeWindow time;
    bool selected=false;
    std::uint32_t ownerColour=0;
    bool operator==(const CanvasFrame&)const=default;
};
struct LaneCanvas
{
    std::unique_ptr<juce::Component> component;
    std::function<void(const Lane&,const CanvasFrame&)> bind;
    std::function<void()> saveState;
    std::function<bool()> hasInteraction;
    std::function<std::vector<double>()> eventTimes;
    // Pure content/width measurement, never a function of the current height.
    std::function<int(int width)> preferredHeight;
    bool supportsDocumentEvents=true;
};
using CanvasFactory=std::function<LaneCanvas(const Lane&)>;
struct MeterReadback
{
    bool available=false;
    std::array<float,ChromeGeometry::maxMeterColumns> values{};
    int count=0;
};

/** Viewport-sized composition. A row consumes exactly its content height: there
    is no header/separator row. Time axis, indices and residency use ayra_timeline.
    The factory must externalise editor state before eviction; saveState runs
    while the canvas is still alive. Borrowed factory/LAF services outlive this. */
class TimelinePanel final:public juce::Component,private juce::ScrollBar::Listener,
                          private juce::Timer,private juce::AsyncUpdater
{
    class LaneRow;
public:
    static constexpr int rulerHeight=28;
    TimelinePanel(Controller& source,CanvasFactory factoryToUse,TimelinePanelLookAndFeel& artwork)
        : controller(source),factory(std::move(factoryToUse)),painter(artwork),horizontal(false),vertical(true),cache(6)
    {
        setWantsKeyboardFocus(true);body.setInterceptsMouseClicks(false,true);
        addAndMakeVisible(body);addAndMakeVisible(horizontal);addAndMakeVisible(vertical);
        horizontal.addListener(this);vertical.addListener(this);
        time.pixelsPerSecond=64.0;
    }
    ~TimelinePanel()override
    {
        stopTimer();cancelPendingUpdate();controller.endResize(false);cache.clear();
        horizontal.removeListener(this);vertical.removeListener(this);
    }
    std::function<void(double)> onSeek;
    std::function<void(const Result&)> onResult;
    std::function<void()> onSelectionChanged;
    // The provider owns source identity/units/banking. No implicit audio-only filter.
    std::function<MeterReadback(const Lane&,bool input)> readMeter;
    void setStyle(PanelStyle s){style=std::move(s);++frameEpoch;refresh();}
    void invalidateCanvasFrames(){++frameEpoch;refresh();}
    void setPixelsPerSecond(double pps)
    {if(std::isfinite(pps)&&pps>0){time.pixelsPerSecond=std::clamp(pps,1.0,100000.0);refresh();}}
    void setTimeExtent(double seconds)
    {if(std::isfinite(seconds)&&seconds>0){durationSeconds=seconds;refresh();}}
    void setPlayhead(double seconds)
    {if(std::isfinite(seconds)){playhead=std::max(0.0,seconds);repaint();}}
    double getPlayhead()const noexcept{return playhead;}
    double getViewStart()const noexcept{return time.startSeconds;}
    void setViewStart(double seconds)
    {if(std::isfinite(seconds)){time.startSeconds=std::max(0.0,seconds);refresh();}}
    void setVerticalPosition(std::int64_t pixels){scrollY=std::max<std::int64_t>(0,pixels);refresh();}
    std::size_t getResidentRowCount()const noexcept{return cache.size();}
    std::size_t getCreatedRowCount()const noexcept{return cache.createdCount();}
    std::size_t getVisibleRowCount()const noexcept{return visibleRows.size();}
    void revealLane(const Id& id)
    {controller.setLaneVisible(id,true);scrollY=static_cast<std::int64_t>(controller.laneTop(id));refresh();}
    void setChrome(presentation::TimelineChromeVisibility settings)
    {controller.setChrome(settings);refresh();}
    void refresh()
    {
        if(updating)return;
        const juce::ScopedValueSetter<bool> guard(updating,true);
        const auto& snapshot=controller.snapshot();
        if(session!=snapshot.session){cache.clear();session=snapshot.session;scrollY=0;originChrome.reset();}
        const auto total=static_cast<std::int64_t>(controller.totalHeight());
        scrollY=std::clamp<std::int64_t>(scrollY,0,std::max<std::int64_t>(0,total-body.getHeight()));
        vertical.setRangeLimits(0.0,static_cast<double>(std::max<std::int64_t>(body.getHeight(),total)),juce::dontSendNotification);
        vertical.setCurrentRange(static_cast<double>(scrollY),body.getHeight(),juce::dontSendNotification);
        visibleRows.clear();wanted.clear();
        controller.visitRows(scrollY,body.getHeight(),80,[&](const Row& r){visibleRows.push_back(r);wanted.push_back(r.key);});
        cache.eraseIf([&](const Id&,LaneRow& r){
            if(r.row.lane.empty())return false;
            const auto* lane=controller.lane(r.row.lane);return !lane||!r.matches(*lane);
        });
        cache.sync(std::span<const Id>(wanted),[&](const Id& key){
            auto r=std::make_unique<LaneRow>(*this);
            const auto row=std::find_if(visibleRows.begin(),visibleRows.end(),[&](const Row& v){return v.key==key;});
            r->row=*row;r->initialise();body.addChildComponent(*r);return r;
        },[](const Id&,LaneRow& r){return r.interacting();},
          [](const Id&,LaneRow& r,bool visible){r.setVisible(visible);});
        // One origin for ALL rows. Reserve the maximum configured left chrome
        // once per document/preferences change, not once per visible row. Thus
        // time zero remains editable, and vertical scrolling never shifts X.
        if(originRevision!=snapshot.revision||!originChrome||*originChrome!=controller.view().chrome){
            int inset=ChromeGeometry::accentWidth;
            for(const auto& lane:snapshot.lanes){
                const auto* owner=controller.owner(lane.owner);if(!owner)continue;
                presentation::TimelineChromeCapabilities caps;
                caps.navigation=lane.navigation;caps.mute=owner->capabilities.mute;caps.solo=owner->capabilities.solo;
                caps.open=owner->capabilities.open;caps.monitoring=owner->capabilities.monitoring;
                caps.inputMeterCount=lane.inputMeterCount;caps.outputMeterCount=lane.outputMeterCount;
                inset=std::max(inset,presentation::layoutTimelineLaneChrome(4096,controller.height(lane.id),caps,controller.view().chrome).leftInset);
            }
            time.originX=inset;originRevision=snapshot.revision;originChrome=controller.view().chrome;
        }
        for(const auto& r:visibleRows)
            if(auto* component=cache.find(r.key)) {
                component->row=r;component->setBounds(0,static_cast<int>(r.top-scrollY),body.getWidth(),static_cast<int>(r.height));
                component->bindRow();
            }
        cache.forEach([&](const Id& id,LaneRow& r,bool visible){
            if(visible&&std::find(wanted.begin(),wanted.end(),id)==wanted.end()) {
                const auto top=controller.laneTop(r.row.lane)-static_cast<double>(scrollY);
                r.setTopLeftPosition(0,static_cast<int>(std::clamp(top,-100000.0,100000.0)));
            }
        });
        const double span=std::max(0.001,time.deltaPixelsToSeconds(body.getWidth()));
        horizontal.setRangeLimits(0,std::max(durationSeconds,time.startSeconds+span),juce::dontSendNotification);
        horizontal.setCurrentRange(time.startSeconds,span,juce::dontSendNotification);
        const auto& show=controller.view().chrome;
        if(isShowing()&&readMeter&&(show.inputMeters||show.outputMeters)) {
            if(!isTimerRunning())startTimerHz(30);
        } else stopTimer();
        repaint();
    }
    void resized()override
    {
        auto area=getLocalBounds();area.removeFromTop(rulerHeight);
        horizontal.setBounds(area.removeFromBottom(14));vertical.setBounds(area.removeFromRight(12));body.setBounds(area);
        refresh();
    }
    void paint(juce::Graphics& g)override
    {
        g.fillAll(style.background);g.setColour(style.seam);g.fillRect(0,0,getWidth(),rulerHeight);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11)));
        const double step=std::pow(2.0,std::ceil(std::log2(std::max(0.001,60.0/time.pixelsPerSecond))));
        const double first=std::floor(time.startSeconds/step)*step;
        for(int i=0;i<body.getWidth()/30+4;++i) {
            const double t=first+i*step;const auto x=juce::roundToInt(time.timeToX(t));
            if(x>body.getWidth())break;if(x<0)continue;
            g.setColour(style.border);g.drawVerticalLine(x,19.0f,static_cast<float>(rulerHeight));
            g.setColour(style.foreground);g.drawText(juce::String(t,step<1?2:0)+" s",x+3,1,64,18,juce::Justification::left);
        }
    }
    void paintOverChildren(juce::Graphics& g)override
    {
        const auto x=time.timeToX(playhead);
        if(x>=0&&x<body.getWidth()) {
            g.setColour(style.foreground.withAlpha(0.6f));g.drawVerticalLine(juce::roundToInt(x),0.0f,static_cast<float>(body.getBottom()));
        }
        if(!dropLane.empty()) {
            const auto y=controller.laneTop(dropLane)-static_cast<double>(scrollY)+body.getY();
            g.setColour(style.foreground);g.fillRect(0.0f,static_cast<float>(y),static_cast<float>(body.getWidth()),2.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e)override
    {if(e.y<rulerHeight&&e.mods.isLeftButtonDown())seek(time.xToTime(e.position.x));}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override
    {
        if(e.mods.isCommandDown()||e.mods.isCtrlDown())time.zoomAt(e.position.x,std::exp(w.deltaY*1.5));
        else if(e.mods.isShiftDown()||std::abs(w.deltaX)>std::abs(w.deltaY))
            time.startSeconds=std::max(0.0,time.startSeconds-time.deltaPixelsToSeconds((std::abs(w.deltaX)>0.001?w.deltaX:w.deltaY)*360.0));
        else scrollY=std::max<std::int64_t>(0,scrollY-static_cast<std::int64_t>(w.deltaY*360));
        refresh();
    }
    bool keyPressed(const juce::KeyPress& key)override
    {
        if(key==juce::KeyPress::escapeKey&&controller.resizing()){controller.endResize(false);refresh();return true;}
        return false;
    }
    void visibilityChanged()override{if(!isShowing())stopTimer();else refresh();}
    void showVisibilityMenu(juce::Component& anchor)
    {
        juce::PopupMenu menu;const auto& s=controller.view().chrome;
        menu.addSectionHeader("Controlli lane");
        const std::array<const char*,9> names{"Eventi precedenti / successivi","Mute","Solo","Monitoring","Apri Tensor","Meter ingresso","Meter uscita","Colore tipo","Indicatore resize"};
        const std::array<bool,9> values{s.navigation,s.mute,s.solo,s.monitoring,s.open,s.inputMeters,s.outputMeters,s.typeColour,s.resizeIndicator};
        for(std::size_t i=0;i<names.size();++i)menu.addItem(static_cast<int>(i)+1,names[i],true,values[i]);
        menu.addSeparator();menu.addSectionHeader("Nome sempre visibile");
        menu.addItem(20,"Nome a sinistra",true,s.nameSide==presentation::TimelineNameSide::left);
        menu.addItem(21,"Nome a destra",true,s.nameSide==presentation::TimelineNameSide::right);
        menu.addSectionHeader("Accento sempre visibile");
        menu.addItem(30,"Accento a sinistra",true,s.accentSide==presentation::TimelineAccentSide::left);
        menu.addItem(31,"Accento a destra",true,s.accentSide==presentation::TimelineAccentSide::right);
        menu.addItem(32,"Accento su entrambi i lati",true,s.accentSide==presentation::TimelineAccentSide::both);
        menu.addSeparator();menu.addItem(40,"Nascondi lane selezionate");menu.addItem(41,"Mostra tutte le lane");
        const auto currentSession=session;juce::Component::SafePointer<TimelinePanel> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&anchor),[safe,currentSession](int id){
            if(!safe||!id||safe->session!=currentSession)return;
            auto c=safe->controller.view().chrome;
            std::array<bool*,9> flags{&c.navigation,&c.mute,&c.solo,&c.monitoring,&c.open,&c.inputMeters,&c.outputMeters,&c.typeColour,&c.resizeIndicator};
            if(id>=1&&id<=9)*flags[static_cast<std::size_t>(id-1)]=!*flags[static_cast<std::size_t>(id-1)];
            if(id==20||id==21)c.nameSide=id==20?presentation::TimelineNameSide::left:presentation::TimelineNameSide::right;
            if(id>=30&&id<=32)c.accentSide=static_cast<presentation::TimelineAccentSide>(id-30);
            if(id==40){const auto selected=safe->controller.view().selectedLanes;for(const auto& lane:selected)safe->controller.setLaneVisible(lane,false);}
            if(id==41){const auto hidden=safe->controller.view().hiddenLanes;for(const auto& lane:hidden)safe->controller.setLaneVisible(lane,true);}
            safe->controller.setChrome(c);safe->refresh();
        });
    }
private:
    class LaneRow final:public juce::Component
    {
        /** JUCE hitTest has no click count. This small name-only proxy forwards
            the first click and every drag/wheel to the underlying live target,
            but consumes the SECOND down/up and double-click for rename. The
            gradient itself never intercepts input. */
        class NameInput final:public juce::Component
        {
        public:
            explicit NameInput(LaneRow& row, bool isPrimary=true):owner(row),primary(isPrimary){}
            void mouseDown(const juce::MouseEvent& e)override {
                if(e.mods.isLeftButtonDown()&&e.getNumberOfClicks()==2){renameClick=true;target=nullptr;owner.beginRename(primary);return;}
                renameClick=false;target=findTarget(e);if(target)target->mouseDown(e.getEventRelativeTo(target));
            }
            void mouseDrag(const juce::MouseEvent& e)override {if(target&&!renameClick)target->mouseDrag(e.getEventRelativeTo(target));}
            void mouseUp(const juce::MouseEvent& e)override {if(target&&!renameClick)target->mouseUp(e.getEventRelativeTo(target));target=nullptr;renameClick=false;}
            void mouseDoubleClick(const juce::MouseEvent&)override {}
            void mouseMove(const juce::MouseEvent& e)override {if(auto* p=findTarget(e)){p->mouseMove(e.getEventRelativeTo(p));setMouseCursor(p->getMouseCursor());}}
            void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override
            {if(auto* p=findTarget(e))p->mouseWheelMove(e.getEventRelativeTo(p),w);}
        private:
            juce::Component* findTarget(const juce::MouseEvent& e)const {
                auto* root=owner.canvas.component.get();if(!root)return nullptr;
                return root->getComponentAt(root->getLocalPoint(this,e.getPosition()));
            }
            LaneRow& owner; bool primary=true; juce::Component::SafePointer<juce::Component> target;bool renameClick=false;
        };
        class ResizeInput final:public juce::Component
        {
        public:
            explicit ResizeInput(LaneRow& r):owner(r){setMouseCursor(juce::MouseCursor::UpDownResizeCursor);}
            void mouseDown(const juce::MouseEvent& e)override{owner.mouseDown(e.getEventRelativeTo(&owner));}
            void mouseDrag(const juce::MouseEvent& e)override{owner.mouseDrag(e.getEventRelativeTo(&owner));}
            void mouseUp(const juce::MouseEvent& e)override{owner.mouseUp(e.getEventRelativeTo(&owner));}
            void mouseDoubleClick(const juce::MouseEvent& e)override{owner.mouseDoubleClick(e.getEventRelativeTo(&owner));}
        private:LaneRow& owner;
        };
    public:
        explicit LaneRow(TimelinePanel& p):host(p),nameInput(*this),laneNameInput(*this,false),resizeInput(*this)
        {setWantsKeyboardFocus(true);addAndMakeVisible(nameInput);addAndMakeVisible(laneNameInput);addChildComponent(nameEditor);addAndMakeVisible(resizeInput);}
        ~LaneRow()override {nameEditor.onFocusLost=nullptr;nameEditor.onReturnKey=nullptr;if(canvas.saveState)canvas.saveState();}
        Row row;
        void initialise()
        {
            setComponentID("lane:"+text(row.lane));nameInput.setComponentID("name:"+text(row.lane));
            laneNameInput.setComponentID("lane-name:"+text(row.lane));
            nameEditor.setComponentID("rename:"+text(row.lane));resizeInput.setComponentID("resize:"+text(row.lane));
            if(const auto* lane=host.controller.lane(row.lane)){canvas=host.factory(*lane);createdVisual=lane->visual;}
            if(canvas.component){canvas.component->setComponentID("canvas:"+text(row.lane));addAndMakeVisible(*canvas.component);canvas.component->toBack();}
            for(std::size_t i=0;i<buttons.size();++i){
                buttons[i]=host.painter.makeButton(static_cast<LaneButton>(i));
                if(!buttons[i])throw std::runtime_error("Missing lane control artwork");
                buttons[i]->setComponentID("button:"+juce::String(static_cast<int>(i))+":"+text(row.lane));
                addAndMakeVisible(*buttons[i]);buttons[i]->onClick=[this,i]{action(static_cast<LaneButton>(i));};
            }
            const std::array<const char*,6> tips{"Evento precedente nella lane","Evento successivo nella lane","Mute della traccia (tutte le sue lane)","Solo della traccia (tutte le sue lane)","Apri il Tensor della traccia","Monitoring condiviso della traccia"};
            for(std::size_t i=0;i<buttons.size();++i)buttons[i]->setTooltip(tips[i]);
            nameEditor.onEscapeKey=[this]{nameEditor.setVisible(false);nameInput.setVisible(true);laneNameInput.setVisible(identityLayout.twoLines);repaint();};
            nameEditor.onReturnKey=[this]{finishRename();};
            nameEditor.onFocusLost=[this]{if(nameEditor.isVisible())finishRename();};
        }
        bool matches(const Lane& lane)const{return createdVisual&&*createdVisual==lane.visual;}
        bool interacting()const
        {return dragging||accentDown||resizing||nameEditor.isVisible()||isMouseButtonDown(true)
            ||(canvas.hasInteraction&&canvas.hasInteraction());}
        void bindRow()
        {
            const auto* lane=host.controller.lane(row.lane);const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            const auto& c=owner->capabilities;
            presentation::TimelineChromeCapabilities caps;
            caps.navigation=lane&&lane->navigation;caps.mute=c.mute;caps.solo=c.solo;caps.open=c.open;caps.monitoring=c.monitoring;
            caps.resizable=lane&&lane->heightPolicy==HeightPolicy::manual;
            caps.inputMeterCount=lane?lane->inputMeterCount:0;caps.outputMeterCount=lane?lane->outputMeterCount:0;
            geometry=presentation::layoutTimelineLaneChrome(getWidth(),getHeight(),caps,host.controller.view().chrome);
            for(std::size_t i=0;i<buttons.size();++i){buttons[i]->setBounds(rectangle(geometry.buttons[i]));buttons[i]->setVisible(!geometry.buttons[i].empty());}
            buttons[2]->setToggleState(owner->muted,juce::dontSendNotification);
            buttons[3]->setToggleState(owner->soloed,juce::dontSendNotification);
            buttons[5]->setToggleState(owner->monitoring!=Monitoring::automatic,juce::dontSendNotification);
            TimelineIdentityContent nextIdentity;
            const bool isGlobal=owner->role==OwnerRole::global;
            nextIdentity.primary=lane&&isGlobal?text(lane->name):text(owner->name);
            nextIdentity.secondary=lane&&!isGlobal?text(lane->name):juce::String{};
            nextIdentity.ownerColour=juce::Colour(owner->colour);
            nextIdentity.typeColour=lane&&host.style.streamColour?host.style.streamColour(lane->stream):host.style.foreground;
            nextIdentity.compact=isGlobal||getHeight()<48;
            nextIdentity.onLeft=host.controller.view().chrome.nameSide==presentation::TimelineNameSide::left;
            nextIdentity.selected=host.controller.view().selectedLanes.contains(row.lane);
            nextIdentity.showType=host.controller.view().chrome.typeColour;
            const auto contentBounds=rectangle(geometry.content);
            if(!identityReady||nextIdentity!=identityContent||identityArea!=contentBounds||identityEpoch!=host.frameEpoch){
                identityContent=nextIdentity;identityArea=contentBounds;identityEpoch=host.frameEpoch;identityReady=true;
                identityLayout=TimelineIdentityLayout::make(contentBounds,identityContent,host.style.identityTypography);
                identityBackdrop=host.painter.prepareIdentity(identityContent,identityLayout);
            }
            nameBounds=identityLayout.primary;
            nameInput.setBounds(nameBounds);laneNameInput.setBounds(identityLayout.secondary);
            if(!nameEditor.isVisible())laneNameInput.setVisible(identityLayout.twoLines);
            resizeInput.toFront(false);
            resizeInput.setBounds(rectangle(geometry.resizeHit));resizeInput.setVisible(!geometry.resizeHit.empty());resizeInput.toFront(false);
            if(canvas.component&&lane){
                canvas.component->setBounds(rectangle(geometry.content));
                CanvasFrame f;f.width=canvas.component->getWidth();f.height=getHeight();f.durationSeconds=host.durationSeconds;
                f.revision=host.controller.snapshot().revision;f.time=host.time;
                f.time.originX-=geometry.content.x;f.selected=host.controller.view().selectedLanes.contains(lane->id);f.ownerColour=owner->colour;
                if(!lastFrame||*lastFrame!=f||lastEpoch!=host.frameEpoch){if(canvas.bind)canvas.bind(*lane,f);lastFrame=f;lastEpoch=host.frameEpoch;}
            }
            if(lane&&lane->heightPolicy==HeightPolicy::automatic&&canvas.preferredHeight)
                if(host.controller.setAutoHeight(lane->id,canvas.preferredHeight(geometry.content.width)))host.triggerAsyncUpdate();
            repaint();
        }
        void paint(juce::Graphics& g)override
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            const auto colour=juce::Colour(owner->colour);const bool selected=host.controller.view().selectedLanes.contains(row.lane);
            g.fillAll(selected?host.style.background.interpolatedWith(colour,0.18f):host.style.background);
            for(auto rail:{geometry.leftAccent,geometry.rightAccent})if(!rail.empty())
                host.painter.drawAccent(g,rectangle(rail).toFloat(),selected?colour:colour.withMultipliedAlpha(0.80f));
            if(const auto* lane=host.controller.lane(row.lane);lane&&host.controller.view().chrome.typeColour){
                const auto type=host.style.streamColour?host.style.streamColour(lane->stream):host.style.foreground;
                g.setColour(type);const auto rail=geometry.leftAccent.empty()?geometry.rightAccent:geometry.leftAccent;
                g.fillRect(rail.x+rail.width-2,rail.y,2,rail.height);
            }
        }
        void paintOverChildren(juce::Graphics& g)override
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            if(!nameEditor.isVisible())
                host.painter.drawIdentity(g,identityContent,identityLayout,identityBackdrop);
            if(!geometry.resizeGlyph.empty()){
                const auto r=rectangle(geometry.resizeGlyph);g.setColour(host.style.foreground.withAlpha(0.75f));
                g.drawHorizontalLine(r.getY(),static_cast<float>(r.getX()),static_cast<float>(r.getRight()));
                g.drawHorizontalLine(r.getY()+3,static_cast<float>(r.getX()+4),static_cast<float>(r.getRight()-4));
            }
            g.setColour(host.style.border);g.drawHorizontalLine(getHeight()-1,0.0f,static_cast<float>(getWidth()));
            drawMeters(g,input,geometry.inputMeters);drawMeters(g,output,geometry.outputMeters);
        }
        void updateMeters()
        {
            const auto* lane=host.controller.lane(row.lane);if(!lane||!host.readMeter)return;
            if(!geometry.inputMeters.empty()){input=host.readMeter(*lane,true);repaint(rectangle(geometry.inputMeters));}
            if(!geometry.outputMeters.empty()){output=host.readMeter(*lane,false);repaint(rectangle(geometry.outputMeters));}
        }
        void mouseDown(const juce::MouseEvent& e)override
        {
            if(!e.mods.isLeftButtonDown())return;
            if(geometry.isAccent(e.x,e.y)&&!row.lane.empty()){
                const auto selection=e.mods.isShiftDown()?Controller::Selection::range:
                    (e.mods.isCommandDown()||e.mods.isCtrlDown())?Controller::Selection::toggle:Controller::Selection::replace;
                deferredSelection=selection==Controller::Selection::replace&&host.controller.view().selectedLanes.contains(row.lane);
                if(!deferredSelection)host.controller.select(row.lane,selection);
                accentDown=true;dragging=false;host.refresh();if(host.onSelectionChanged)host.onSelectionChanged();return;
            }
            if(geometry.resizeHit.contains(e.x,e.y)&&host.controller.beginResize(row.lane)){
                resizeStart=e.getScreenY();resizing=true;grabKeyboardFocus();
            }
        }
        void mouseMove(const juce::MouseEvent& e)override
        {setMouseCursor(geometry.resizeHit.contains(e.x,e.y)?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);}
        void mouseDrag(const juce::MouseEvent& e)override
        {
            if(resizing){host.controller.updateResize(e.getScreenY()-resizeStart);host.refresh();return;}
            if(accentDown&&e.getDistanceFromDragStart()>4){dragging=true;host.dragTo(e.getEventRelativeTo(&host.body).position);}
        }
        void mouseUp(const juce::MouseEvent&)override
        {
            if(resizing){resizing=false;host.controller.endResize(true);host.originChrome.reset();host.refresh();}
            if(accentDown){
                accentDown=false;
                if(dragging&&!host.dropLane.empty()){
                    std::vector<Id> ids(host.controller.view().selectedLanes.begin(),host.controller.view().selectedLanes.end());
                    host.report(host.controller.moveBefore(ids,host.dropLane));
                }else if(deferredSelection)host.controller.select(row.lane,Controller::Selection::replace);
                dragging=false;host.dropLane.clear();host.refresh();if(host.onSelectionChanged)host.onSelectionChanged();
            }
        }
        void mouseDoubleClick(const juce::MouseEvent& e)override
        {if(geometry.resizeHit.contains(e.x,e.y)){if(const auto* lane=host.controller.lane(row.lane))host.controller.setHeight(row.lane,lane->initialHeight);host.refresh();}}
        bool keyPressed(const juce::KeyPress& key)override
        {if(key==juce::KeyPress::escapeKey&&resizing){resizing=false;host.controller.endResize(false);host.refresh();return true;}return false;}
    private:
        TimelinePanel& host;LaneCanvas canvas;ChromeGeometry geometry;std::optional<VisualKind> createdVisual;
        std::uint64_t renameRevision=0;
        std::array<std::unique_ptr<juce::Button>,6> buttons;
        NameInput nameInput,laneNameInput;ResizeInput resizeInput;juce::TextEditor nameEditor;juce::Rectangle<int> nameBounds;
        TimelineIdentityContent identityContent; TimelineIdentityLayout identityLayout;
        juce::Image identityBackdrop; juce::Rectangle<int> identityArea;
        std::uint64_t identityEpoch=0; bool identityReady=false,renameOwnerTarget=false;
        std::optional<CanvasFrame> lastFrame;std::uint64_t lastEpoch=0;
        MeterReadback input,output;bool accentDown=false,dragging=false,deferredSelection=false,resizing=false;int resizeStart=0;
        void drawMeters(juce::Graphics& g,const MeterReadback& data,presentation::TimelineChromeRect bounds)
        {
            if(bounds.empty())return;
            const int count=std::min(bounds.width/ChromeGeometry::meterWidth,ChromeGeometry::maxMeterColumns);
            for(int i=0;i<count;++i){
                auto r=juce::Rectangle<float>(static_cast<float>(bounds.x+i*ChromeGeometry::meterWidth),static_cast<float>(bounds.y),4.0f,static_cast<float>(bounds.height));
                const float v=data.available&&i<data.count&&std::isfinite(data.values[static_cast<std::size_t>(i)])?std::clamp(data.values[static_cast<std::size_t>(i)],0.0f,1.0f):0.0f;
                host.painter.drawMeter(g,r,v);
            }
        }
        void beginRename(bool primary)
        {
            const auto* lane=host.controller.lane(row.lane);const auto* owner=host.controller.owner(row.owner);
            if(!lane||!owner)return;
            renameOwnerTarget=primary&&owner->role!=OwnerRole::global;
            renameRevision=host.controller.snapshot().revision;
            nameInput.setVisible(false);laneNameInput.setVisible(false);
            nameEditor.setBounds((primary?identityLayout.primary:identityLayout.secondary).expanded(4,2));
            nameEditor.setFont(primary?identityLayout.primaryFont:identityLayout.secondaryFont);
            nameEditor.setText(text(renameOwnerTarget?owner->name:lane->name),false);
            nameEditor.setVisible(true);nameEditor.toFront(true);nameEditor.selectAll();nameEditor.grabKeyboardFocus();repaint();
        }
        void finishRename()
        {
            const auto label=nameEditor.getText().trim().toStdString();nameEditor.setVisible(false);nameInput.setVisible(true);laneNameInput.setVisible(identityLayout.twoLines);
            const auto id=renameOwnerTarget?row.owner:row.lane; const bool ownerTarget=renameOwnerTarget;const auto session=host.session;const auto revision=renameRevision;juce::Component::SafePointer<TimelinePanel> safe(&host);
            juce::MessageManager::callAsync([safe,id,session,label,revision,ownerTarget]{if(safe&&safe->session==session){safe->report(safe->controller.snapshot().revision==revision?(ownerTarget?safe->controller.renameOwner(id,label):safe->controller.rename(id,label)):Result::fail("Rename context expired"));safe->refresh();}});
        }
        void action(LaneButton action)
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            if(action==LaneButton::previous||action==LaneButton::next){
                const bool forward=action==LaneButton::next;
                auto target=host.controller.adjacentEvent(row.lane,host.playhead,forward);
                if(canvas.eventTimes){auto times=canvas.eventTimes();std::sort(times.begin(),times.end());target.reset();
                    if(forward){for(double t:times)if(t>host.playhead+1e-7){target=t;break;}}
                    else{for(auto i=times.rbegin();i!=times.rend();++i)if(*i<host.playhead-1e-7){target=*i;break;}}
                }
                if(target)host.seek(*target);return;
            }
            if(action==LaneButton::mute)host.report(host.controller.control(row.lane,CommandKind::mute,!owner->muted));
            if(action==LaneButton::solo)host.report(host.controller.control(row.lane,CommandKind::solo,!owner->soloed));
            if(action==LaneButton::open)host.report(host.controller.control(row.lane,CommandKind::openOwner));
            if(action==LaneButton::monitoring){
                juce::PopupMenu menu;const std::array<const char*,4> labels{"Auto","Live","Playback","Merge"};
                for(int i=0;i<4;++i)menu.addItem(i+1,labels[static_cast<std::size_t>(i)],true,static_cast<int>(owner->monitoring)==i);
                const auto id=row.lane;const auto session=host.session;const auto revision=host.controller.snapshot().revision;juce::Component::SafePointer<TimelinePanel> safe(&host);
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(buttons[5].get()),[safe,id,session,revision](int chosen){
                    if(safe&&chosen>0&&chosen<=4&&safe->session==session&&safe->controller.snapshot().revision==revision)safe->report(safe->controller.control(id,CommandKind::monitoring,false,static_cast<Monitoring>(chosen-1)));
                });
            }
            host.refresh();
        }
    };
    Controller& controller;CanvasFactory factory;TimelinePanelLookAndFeel& painter;
    PanelStyle style;juce::Component body;juce::ScrollBar horizontal,vertical;
    presentation::TimelineRowResidency<Id,LaneRow> cache;
    presentation::TimelineTimeWindow time;std::int64_t scrollY=0;
    std::vector<Row> visibleRows;std::vector<Id> wanted;Id session,dropLane;
    std::optional<presentation::TimelineChromeVisibility> originChrome;std::uint64_t originRevision=0;
    double durationSeconds=32,playhead=0;bool updating=false;std::uint64_t frameEpoch=1;
    void report(const Result& result){if(onResult)onResult(result);}
    void seek(double seconds)
    {setPlayhead(seconds);if(time.timeToX(playhead)<0||time.timeToX(playhead)>body.getWidth())time.startSeconds=std::max(0.0,playhead-1);refresh();if(onSeek)onSeek(playhead);}
    void scrollBarMoved(juce::ScrollBar* bar,double start)override
    {if(bar==&horizontal)time.startSeconds=std::max(0.0,start);else scrollY=static_cast<std::int64_t>(start);refresh();}
    void handleAsyncUpdate()override{refresh();}
    void timerCallback()override{cache.forEach([](const Id&,LaneRow& row,bool visible){if(visible)row.updateMeters();});}
    void dragTo(juce::Point<float> p)
    {
        if(p.y<20)scrollY=std::max<std::int64_t>(0,scrollY-20);else if(p.y>body.getHeight()-20)scrollY+=20;
        dropLane.clear();controller.visitRows(scrollY+static_cast<std::int64_t>(std::max(0.0f,p.y)),1,0,[&](const Row& r){if(!r.lane.empty())dropLane=r.lane;});
        refresh();
    }
};
}
