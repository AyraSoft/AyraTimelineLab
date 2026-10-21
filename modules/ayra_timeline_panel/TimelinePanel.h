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
    virtual void drawControlGroup(juce::Graphics&,juce::Rectangle<float>)const {}
    virtual void drawMeter(juce::Graphics&,juce::Rectangle<float>,float)const=0;
    virtual juce::Image prepareIdentity(const TimelineIdentityContent&, const TimelineIdentityLayout&) const { return {}; }
    // Optional real frosted underlay. The panel captures only a padded lens-sized
    // region of the canvas, never its own overlay, and only after invalidation.
    virtual int identityBackdropPadding() const noexcept { return 0; }
    virtual juce::Image prepareIdentityUnderlay(const juce::Image&, float) const { return {}; }
    virtual juce::Image prepareIdentityUnderlay(const juce::Image& image, float scale,
                                                const TimelineIdentityLayout&, juce::Rectangle<int>) const
    { return prepareIdentityUnderlay(image,scale); }
    virtual void drawIdentityUnderlay(juce::Graphics&, const TimelineIdentityLayout&,
                                      const juce::Image&, juce::Rectangle<int>) const {}
    virtual void drawIdentity(juce::Graphics& g, const TimelineIdentityContent& c,
                              const TimelineIdentityLayout& layout, const juce::Image&) const
    {
        g.setColour(juce::Colours::white); g.setFont(layout.trackFont);
        g.drawText(c.track,layout.track,juce::Justification::centredRight,true);
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
    // Optional subscription; the canvas clears it on destruction. The host
    // unregisters before eviction. Allows event edits to remeasure auto-height.
    std::function<void(std::function<void()>)> setContentChangeListener;
    bool supportsDocumentEvents=true;
    // Canvas-local, side-effect-free query using the SAME clip/editor geometry
    // as input. A component's rectangular hitTest or a lane time interval is
    // not proof of a clip here. Missing callback = no hover suppression.
    std::function<bool(juce::Point<float>)> containsClipAt; // compatibility fallback
    // All real content targets: clips, points, spans, curves and handles. This
    // must use the canvas input geometry, not its full rectangular hitTest.
    std::function<bool(juce::Point<float>)> containsEditableAt;
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
        shuttingDown=true;stopTimer();cancelPendingUpdate();controller.endResize(false);cache.clear();
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
    {if(std::isfinite(seconds)){playhead=std::max(0.0,seconds);
        cache.forEach([](const Id&,LaneRow& r,bool visible){if(visible)r.updateNavigationEnabled();});repaint();}}
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
    void setIdentityNavigation(bool visible)
    {controller.setIdentityNavigation(visible);refresh();}
    // Read-only inspection ports, also useful for native accessibility tests.
    juce::Rectangle<int> getIdentityBounds(const Id& lane) const
    {
        if (auto* r=cache.find("lane:"+lane)) return getLocalArea(r,r->identityBounds());
        return {};
    }
    bool isIdentitySuppressed(const Id& lane) const
    { if (auto* r=cache.find("lane:"+lane)) return r->identitySuppressed(); return false; }
    std::optional<TimelineIdentityContent> getIdentityReadout(const Id& lane)const
    {if(auto* r=cache.find("lane:"+lane))return r->identityReadout();return {};}
    bool hasPendingIdentityTransition()const noexcept{return identityDue.has_value();}
    // Lane-local rectangles; getIdentityBounds above is panel-local.
    std::optional<TimelineIdentityLayout> getIdentityLayout(const Id& lane)const
    {if(auto* r=cache.find("lane:"+lane))return r->identityGeometry();return {};}
    bool isIdentityEditing(const Id& lane)const
    {if(auto* r=cache.find("lane:"+lane))return r->isEditingTitle();return false;}
    // Async waveform/image providers call this when pixels change independently
    // of a CanvasFrame revision. Presentation invalidation, not a document edit.
    void notifyCanvasContentChanged(const Id& lane)
    { if (auto* r=cache.find("lane:"+lane)) r->contentPixelsChanged(); }
    void refresh()
    {
        if(updating)return;
        const juce::ScopedValueSetter<bool> guard(updating,true);
        const auto& snapshot=controller.snapshot();
        if(session!=snapshot.session){pendingIdentity.clear();identityDue.reset();cache.clear();session=snapshot.session;scrollY=0;originChrome.reset();}
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
                const auto caps=controller.chromeCapabilities(lane);
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
        updateTimerSchedule();
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
    void visibilityChanged()override
    {
        if(!isShowing()){
            stopTimer();pendingIdentity.clear();identityDue.reset();
            cache.forEach([](const Id&,LaneRow& row,bool){row.resetIdentityInteraction();});
        }
        else refresh();
    }
    void showVisibilityMenu(juce::Component& anchor)
    {
        juce::PopupMenu menu;const auto& s=controller.view().chrome;
        menu.addSectionHeader("Navigazione");
        menu.addItem(10,"Frecce nella card",true,controller.view().identityNavigation);
        menu.addSectionHeader("Controlli laterali opzionali (API conservate)");
        const std::array<const char*,9> names{"Navigazione laterale (legacy)","Mute","Solo","Monitoring","Apri Tensor","Meter ingresso","Meter uscita","Pallino del tipo nella card","Indicatore resize"};
        const std::array<bool,9> values{s.navigation,s.mute,s.solo,s.monitoring,s.open,s.inputMeters,s.outputMeters,s.typeColour,s.resizeIndicator};
        for(std::size_t i=0;i<names.size();++i)menu.addItem(static_cast<int>(i)+1,names[i],true,values[i]);
        menu.addSeparator();menu.addSectionHeader("Identita (500 ms sopra contenuti editabili)");
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
            if(id==10){safe->controller.setIdentityNavigation(!safe->controller.view().identityNavigation);safe->refresh();return;}
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
        class IdentitySurface final:public juce::Component
        {
        public:
            explicit IdentitySurface(LaneRow& row):owner(row){setInterceptsMouseClicks(false,true);}
            void paint(juce::Graphics& g)override
            {
                g.addTransform(juce::AffineTransform::translation(-static_cast<float>(getX()),-static_cast<float>(getY())));
                owner.paintIdentity(g);
            }
        private:LaneRow& owner;
        };
        /** Observation only: the canvas keeps JUCE's real mouse target,
            capture, double-click sequence and cursor. No forwarding proxy. */
        class PointerObserver final : public juce::MouseListener
        {
        public:
            explicit PointerObserver(LaneRow& row) : owner(row) {}
            void mouseEnter(const juce::MouseEvent& e) override { observe(e); }
            void mouseMove(const juce::MouseEvent& e) override { observe(e); }
            void mouseExit(const juce::MouseEvent& e) override { observe(e); }
            void mouseDown(const juce::MouseEvent& e) override { changed(e,true); }
            void mouseDrag(const juce::MouseEvent& e) override { changed(e,true); }
            void mouseUp(const juce::MouseEvent& e) override { changed(e);owner.updateNavigationEnabled();owner.measureAutoHeight(); }
            void mouseDoubleClick(const juce::MouseEvent& e) override { changed(e);owner.updateNavigationEnabled();owner.measureAutoHeight(); }
            void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails&) override { changed(e); }
        private:
            LaneRow& owner;
            void observe(const juce::MouseEvent& e) { owner.updateIdentityPointer(e.getEventRelativeTo(&owner).position); }
            void changed(const juce::MouseEvent& e,bool editing=false) {
                owner.underlayDirty=true;
                owner.updateIdentityPointer(e.getEventRelativeTo(&owner).position,editing);
            }
        };
        // This is deliberately only the rendered TRACK name, not the lens.
        // Other labels and passive state glyphs still target the real canvas.
        class TitleInput final:public juce::Component,public juce::TooltipClient
        {
        public:
            explicit TitleInput(LaneRow& row):owner(row){setMouseCursor(juce::MouseCursor::IBeamCursor);setWantsKeyboardFocus(true);}
            bool hitTest(int x,int y)override
            {
                const auto p=owner.getLocalPoint(this,juce::Point<int>{x,y});
                return owner.identityLayout.titleHit.contains(p)
                    ||(owner.titleExpanded&&owner.titleHoverOrigin.contains(p));
            }
            void mouseDoubleClick(const juce::MouseEvent& e)override
            {if(e.mods.isLeftButtonDown())owner.beginTitleEdit();}
            juce::String getTooltip()override{return owner.identityContent.track;}
        private:LaneRow& owner;
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
        explicit LaneRow(TimelinePanel& p):host(p),pointerObserver(*this),resizeInput(*this),identitySurface(*this),titleInput(*this)
        {setWantsKeyboardFocus(true);addMouseListener(&pointerObserver,true);addAndMakeVisible(resizeInput);addAndMakeVisible(identitySurface);identitySurface.addAndMakeVisible(titleInput);}
        ~LaneRow()override {if(canvas.setContentChangeListener)canvas.setContentChangeListener({});host.scheduleIdentity(row.key,{});removeMouseListener(&pointerObserver);if(canvas.saveState)canvas.saveState();}
        juce::Rectangle<int> identityBounds() const { return identityLayout.backdrop; }
        bool identitySuppressed() const { return identityHover.hidden(); }
        TimelineIdentityContent identityReadout() const { return identityContent; }
        TimelineIdentityLayout identityGeometry()const{return identityLayout;}
        bool isEditingTitle()const noexcept{return editingTitle;}
        void contentPixelsChanged() { underlayDirty=true;measureAutoHeight();updateNavigationEnabled();sampleIdentityPointer();repaint(identityLayout.backdrop); }
        void pollIdentityPointer(){sampleIdentityPointer();}
        void resetIdentityInteraction(){
            finishTitleEdit(false);titleExpanded=false;titleHoverOrigin={};identityHover.reset();identitySurface.setVisible(true);
            host.scheduleIdentity(row.key,{});underlayDirty=true;
        }
        void updateNavigationEnabled(){
            for(std::size_t i=0;i<navigation.size();++i)if(navigation[i])navigation[i]->setEnabled(adjacentTime(i==1).has_value());
        }
        Row row;
        void initialise()
        {
            setComponentID("lane:"+text(row.lane));resizeInput.setComponentID("resize:"+text(row.lane));
            if(const auto* lane=host.controller.lane(row.lane)){canvas=host.factory(*lane);createdVisual=lane->visual;}
            titleInput.setComponentID("identity-title:"+text(row.lane));
            if(canvas.setContentChangeListener) {
                juce::Component::SafePointer<LaneRow> safe(this);
                canvas.setContentChangeListener([safe]{if(safe)safe->contentPixelsChanged();});
            }
            if(canvas.component){canvas.component->setComponentID("canvas:"+text(row.lane));addAndMakeVisible(*canvas.component);canvas.component->toBack();}
            for(std::size_t i=0;i<buttons.size();++i){
                buttons[i]=host.painter.makeButton(static_cast<LaneButton>(i));
                if(!buttons[i])throw std::runtime_error("Missing lane control artwork");
                buttons[i]->setComponentID("button:"+juce::String(static_cast<int>(i))+":"+text(row.lane));
                addAndMakeVisible(*buttons[i]);buttons[i]->onClick=[this,i]{action(static_cast<LaneButton>(i));};
            }
            for(std::size_t i=0;i<navigation.size();++i){
                navigation[i]=host.painter.makeButton(i==0?LaneButton::previous:LaneButton::next);
                if(!navigation[i])throw std::runtime_error("Missing identity navigation artwork");
                navigation[i]->setComponentID("card-nav:"+juce::String(i==0?"previous:":"next:")+text(row.lane));
                navigation[i]->setTooltip(i==0?"Evento precedente nella lane":"Evento successivo nella lane");
                identitySurface.addAndMakeVisible(*navigation[i]);
                navigation[i]->onClick=[this,i]{action(i==0?LaneButton::previous:LaneButton::next);};
            }
            const std::array<const char*,6> tips{"Evento precedente nella lane","Evento successivo nella lane","Mute della traccia (tutte le sue lane)","Solo della traccia (tutte le sue lane)","Apri il Tensor della traccia","Monitoring condiviso della traccia"};
            for(std::size_t i=0;i<buttons.size();++i)buttons[i]->setTooltip(tips[i]);
        }
        bool matches(const Lane& lane)const{return createdVisual&&*createdVisual==lane.visual;}
        bool interacting()const
        {return editingTitle||dragging||accentDown||resizing||isMouseButtonDown(true)
            ||(canvas.hasInteraction&&canvas.hasInteraction());}
        void bindRow()
        {
            const auto* lane=host.controller.lane(row.lane);const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            auto caps=lane?host.controller.chromeCapabilities(*lane):presentation::TimelineChromeCapabilities{};
            if(!lane){ // Structural owner rows have no lane navigation or resize.
                caps.navigation=false;caps.resizable=false;
                caps.mute=owner->capabilities.mute;caps.solo=owner->capabilities.solo;
                caps.open=owner->capabilities.open;caps.monitoring=owner->capabilities.monitoring;
            }
            geometry=presentation::layoutTimelineLaneChrome(getWidth(),getHeight(),caps,host.controller.view().chrome);
            for(std::size_t i=0;i<buttons.size();++i){buttons[i]->setBounds(rectangle(geometry.buttons[i]));buttons[i]->setVisible(!geometry.buttons[i].empty());}
            buttons[2]->setToggleState(owner->muted,juce::dontSendNotification);
            buttons[3]->setToggleState(owner->soloed,juce::dontSendNotification);
            buttons[5]->setToggleState(owner->status.monitoringActive,juce::dontSendNotification);
            TimelineIdentityContent nextIdentity;
            const bool isGlobal=owner->role==OwnerRole::global;
            nextIdentity.track=lane&&isGlobal?text(lane->name):text(owner->name);
            nextIdentity.type=lane&&!isGlobal?streamName(lane->stream):juce::String{};
            // Resolve actual ancestry; never parse display names or substitute
            // the Song as a performer for song-owned tracks.
            auto* ancestor=owner;
            for(std::size_t depth=0;ancestor&&depth<host.controller.snapshot().owners.size();++depth) {
                if(ancestor->role==OwnerRole::performer){nextIdentity.performer=text(ancestor->name);break;}
                ancestor=host.controller.owner(ancestor->parent);
            }
            nextIdentity.ownerColour=juce::Colour(owner->colour);
            nextIdentity.typeColour=isGlobal?juce::Colour(owner->colour):(lane&&host.style.streamColour?host.style.streamColour(lane->stream):host.style.foreground);
            nextIdentity.compact=isGlobal;
            nextIdentity.expanded=titleExpanded||editingTitle;
            nextIdentity.navigation=lane&&lane->navigation&&host.controller.view().identityNavigation;
            nextIdentity.muted=owner->muted;nextIdentity.soloed=owner->soloed;
            nextIdentity.status=owner->status;
            nextIdentity.onLeft=host.controller.view().chrome.nameSide==presentation::TimelineNameSide::left;
            nextIdentity.selected=host.controller.view().selectedLanes.contains(row.lane);
            nextIdentity.showType=host.controller.view().chrome.typeColour;
            const auto contentBounds=rectangle(geometry.content);
            if(!identityReady||nextIdentity!=identityContent||identityArea!=contentBounds||identityEpoch!=host.frameEpoch){
                identityContent=nextIdentity;identityArea=contentBounds;identityEpoch=host.frameEpoch;identityReady=true;
                rebuildIdentityGeometry();
            }
            // A disappearing owner or a concurrent rename invalidates the edit;
            // unrelated readback revisions do not silently overwrite user input.
            if(editingTitle&&(!canRenameTitle()||host.session!=editSession||owner->id!=editOwner||text(owner->name)!=editOriginal))
                finishTitleEdit(false);
            resizeInput.toFront(false);
            resizeInput.setBounds(rectangle(geometry.resizeHit));resizeInput.setVisible(!geometry.resizeHit.empty());resizeInput.toFront(false);
            if(canvas.component&&lane){
                canvas.component->setBounds(rectangle(geometry.content));
                CanvasFrame f;f.width=canvas.component->getWidth();f.height=getHeight();f.durationSeconds=host.durationSeconds;
                f.revision=host.controller.snapshot().revision;f.time=host.time;
                f.time.originX-=geometry.content.x;f.selected=host.controller.view().selectedLanes.contains(lane->id);f.ownerColour=owner->colour;
                if(!lastFrame||*lastFrame!=f||lastEpoch!=host.frameEpoch){if(canvas.bind)canvas.bind(*lane,f);lastFrame=f;lastEpoch=host.frameEpoch;underlayDirty=true;}
            }
            measureAutoHeight();
            updateNavigationEnabled();
            sampleIdentityPointer(); // includes stationary-pointer scroll / zoom
            repaint();
        }
        void visibilityChanged() override
        {
            if(!isVisible()){finishTitleEdit(false);titleExpanded=false;titleHoverOrigin={};identityHover.reset();host.scheduleIdentity(row.key,{});}
            underlayDirty=true;
        }
        void paint(juce::Graphics& g)override
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            const auto colour=juce::Colour(owner->colour);const bool selected=host.controller.view().selectedLanes.contains(row.lane);
            g.fillAll(selected?host.style.background.interpolatedWith(colour,0.18f):host.style.background);
            for(auto rail:{geometry.leftAccent,geometry.rightAccent})if(!rail.empty())
                host.painter.drawAccent(g,rectangle(rail).toFloat(),selected?colour:colour.withMultipliedAlpha(0.80f));
            // One well for the complete bank; child buttons draw glyphs/state only.
            for(const auto group:{geometry.leftGroup,geometry.rightGroup})if(!group.empty())
                host.painter.drawControlGroup(g,rectangle(group).toFloat());
        }
        void paintOverChildren(juce::Graphics& g)override
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;

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
        {
            if(geometry.isAccent(e.x,e.y))setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            else setMouseCursor(geometry.resizeHit.contains(e.x,e.y)
                ? juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);
        }
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
        {
            if(!e.mods.isLeftButtonDown())return;
            // Accent owns vertical preview cycling, including continuous lines.
            // Other double clicks remain with the real canvas/editor target.
            if(geometry.isAccent(e.x,e.y)){
                accentDown=false;dragging=false;deferredSelection=false;host.dropLane.clear();
                if(host.controller.cycleHeight(row.lane))host.refresh();
                return;
            }
            if(geometry.resizeHit.contains(e.x,e.y)){
                if(host.controller.cycleHeight(row.lane))host.refresh();
            }
        }
        bool keyPressed(const juce::KeyPress& key)override
        {if(key==juce::KeyPress::escapeKey&&resizing){resizing=false;host.controller.endResize(false);host.refresh();return true;}return false;}
    private:
        TimelinePanel& host;LaneCanvas canvas;ChromeGeometry geometry;std::optional<VisualKind> createdVisual;
        std::array<std::unique_ptr<juce::Button>,6> buttons;
        PointerObserver pointerObserver;ResizeInput resizeInput;IdentitySurface identitySurface;TitleInput titleInput;
        std::unique_ptr<juce::TextEditor> titleEditor;
        bool titleExpanded=false,editingTitle=false;
        juce::Rectangle<int> titleHoverOrigin;
        Id editSession,editOwner;juce::String editOriginal;
        std::uint64_t editRevision=0;
        std::array<std::unique_ptr<juce::Button>,2> navigation;
        TimelineIdentityContent identityContent; TimelineIdentityLayout identityLayout;
        juce::Image identityBackdrop; juce::Rectangle<int> identityArea;
        std::uint64_t identityEpoch=0; bool identityReady=false;
        TimelineIdentityHoverState identityHover;
        juce::Image identityUnderlay;juce::Rectangle<int> underlayBounds;
        bool underlayDirty=true;float underlayScale=0.0f;
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
        bool canRenameTitle()const
        {
            const auto* owner=host.controller.owner(row.owner);
            return owner&&(owner->role==OwnerRole::track||owner->role==OwnerRole::convergence||owner->role==OwnerRole::songMain);
        }
        void rebuildIdentityGeometry()
        {
            const auto old=identityLayout.backdrop;
            identityContent.expanded=titleExpanded||editingTitle;
            identityLayout=TimelineIdentityLayout::make(identityArea,identityContent,host.style.identityTypography);
            identityBackdrop=host.painter.prepareIdentity(identityContent,identityLayout);underlayDirty=true;
            identitySurface.setBounds(identityLayout.backdrop);identitySurface.toFront(false);
            const auto offset=identityLayout.backdrop.getPosition();
            const std::array<juce::Rectangle<int>,2> rects{identityLayout.previous,identityLayout.next};
            for(std::size_t i=0;i<rects.size();++i)if(navigation[i]){
                navigation[i]->setBounds(rects[i].translated(-offset.x,-offset.y));navigation[i]->setVisible(!rects[i].isEmpty());
            }
            const auto titleTarget=titleExpanded?identityLayout.titleHit.getUnion(titleHoverOrigin):identityLayout.titleHit;
            titleInput.setBounds(titleTarget.translated(-offset.x,-offset.y));
            titleInput.setName(identityContent.track);
            titleInput.setVisible(canRenameTitle()&&!editingTitle&&!identityLayout.titleHit.isEmpty());
            if(titleEditor&&editingTitle){
                titleEditor->setBounds(identityLayout.track.translated(-offset.x,-offset.y));
                titleEditor->setFont(identityLayout.trackFont);
                titleEditor->toFront(false);
            }
            identitySurface.setVisible(!identityHover.hidden());repaint(old.getUnion(identityLayout.backdrop));
        }
        void beginTitleEdit()
        {
            if(editingTitle||!canRenameTitle()||identityHover.hidden())return;
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            editSession=host.session;editOwner=owner->id;editOriginal=text(owner->name);
            editRevision=host.controller.snapshot().revision;
            editingTitle=true;titleExpanded=true;identityHover.reset();host.scheduleIdentity(row.key,{});
            if(!titleEditor){
                titleEditor=std::make_unique<juce::TextEditor>();
                titleEditor->setComponentID("identity-editor:"+text(row.lane));
                titleEditor->setMultiLine(false);titleEditor->setReturnKeyStartsNewLine(false);
                titleEditor->setInputRestrictions(256);
                titleEditor->onReturnKey=[this]{finishTitleEdit(true);};
                titleEditor->onEscapeKey=[this]{finishTitleEdit(false);};
                titleEditor->onFocusLost=[this]{finishTitleEdit(true);};
                identitySurface.addChildComponent(*titleEditor);
            }
            titleEditor->setColour(juce::TextEditor::backgroundColourId,host.style.background.withAlpha(0.96f));
            titleEditor->setColour(juce::TextEditor::textColourId,host.style.foreground);
            titleEditor->setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
            titleEditor->setColour(juce::TextEditor::focusedOutlineColourId,identityContent.ownerColour.withAlpha(0.65f));
            titleEditor->setIndents(0,0);titleEditor->setBorder({0,0,0,0});
            titleEditor->setText(editOriginal,false);rebuildIdentityGeometry();
            titleEditor->setVisible(true);titleEditor->grabKeyboardFocus();titleEditor->selectAll();
        }
        void finishTitleEdit(bool commit)
        {
            if(!editingTitle)return;
            const auto label=titleEditor?titleEditor->getText().trim():juce::String{};
            editingTitle=false; // before focusLost, which can be synchronous
            if(titleEditor)titleEditor->setVisible(false);
            titleInput.setVisible(canRenameTitle());
            if(!commit||label==editOriginal){repaint();return;}
            if(label.isEmpty()){host.report(Result::fail("Il nome della traccia non puo essere vuoto"));return;}
            const auto session=editSession,owner=editOwner;const auto original=editOriginal;
            const auto revision=editRevision;const auto lane=row.lane;
            juce::Component::SafePointer<TimelinePanel> safe(&host);
            // Never destroy/rebind the row inside a TextEditor callback. One
            // host command, same owner for every sibling, with optimistic guards.
            juce::MessageManager::callAsync([safe,session,owner,original,revision,lane,label]{
                if(!safe||safe->session!=session)return;
                const auto* current=safe->controller.owner(owner);const auto* target=safe->controller.lane(lane);
                if(!current||!target||target->owner!=owner||text(current->name)!=original
                    ||safe->controller.snapshot().revision<revision){
                    safe->report(Result::fail("Rinomina annullata: la traccia e cambiata"));return;
                }
                safe->report(safe->controller.renameOwner(owner,label.toStdString()));safe->refresh();
            });
        }
        void measureAutoHeight()
        {
            const auto* lane=host.controller.lane(row.lane);
            if(!lane||lane->heightPolicy!=HeightPolicy::automatic||!canvas.preferredHeight)return;
            if(canvas.component&&canvas.component->isMouseButtonDown(true))return;
            if(host.applyAutoHeight(lane->id,canvas.preferredHeight(geometry.content.width)))host.triggerAsyncUpdate();
        }
        void updateIdentityPointer(juce::Point<float> p,bool editing=false)
        {
            if(editingTitle){host.scheduleIdentity(row.key,{});identitySurface.setVisible(true);return;}
            const auto inBody=host.body.getLocalPoint(this,p);
            const bool withinView=isShowing()&&host.body.getLocalBounds().toFloat().contains(inBody);
            bool inside=withinView&&identityLayout.backdrop.toFloat().contains(p);
            const bool onNavigation=inside&&!identityHover.hidden()
                &&(identityLayout.navigationGroup.toFloat().contains(p)
                   ||(navigation[0]&&navigation[0]->isDown())||(navigation[1]&&navigation[1]->isDown()));
            const bool onTitle=inside&&!identityHover.hidden()&&canRenameTitle()
                &&(identityLayout.titleHit.toFloat().contains(p)||(titleExpanded&&titleHoverOrigin.toFloat().contains(p)));
            const bool expand=onTitle||(titleExpanded&&onNavigation);
            if(expand!=titleExpanded){
                if(expand)titleHoverOrigin=identityLayout.titleHit;else titleHoverOrigin={};
                titleExpanded=expand;rebuildIdentityGeometry();
                inside=withinView&&identityLayout.backdrop.toFloat().contains(p);
            }
            const bool protectedTarget=onNavigation||onTitle;
            bool editable=false;
            if(inside&&!protectedTarget&&!identityHover.hidden()&&canvas.component) {
                const auto local=canvas.component->getLocalPoint(this,p);
                auto* target=getComponentAt(p.roundToInt());
                if(target&&(target==canvas.component.get()||canvas.component->isParentOf(target))) {
                    if(canvas.containsEditableAt)editable=canvas.containsEditableAt(local);
                    else if(canvas.containsClipAt)editable=canvas.containsClipAt(local);
                }
            }
            if(identityHover.update(inside,editable,protectedTarget,
                                    juce::Time::getMillisecondCounterHiRes(),
                                    editing || (inside && editable && !canRenameTitle()))) {
                underlayDirty=true;repaint(identityLayout.backdrop);
            }
            identitySurface.setVisible(!identityHover.hidden());
            host.scheduleIdentity(row.key,identityHover.deadline());
        }

        void paintIdentity(juce::Graphics& g)
        {
            if(identityHover.hidden())return;
            prepareUnderlayIfNeeded(g);
            host.painter.drawIdentityUnderlay(g,identityLayout,identityUnderlay,underlayBounds);
            auto readout=identityContent;if(editingTitle)readout.track.clear();
            host.painter.drawIdentity(g,readout,identityLayout,identityBackdrop);
        }

        void sampleIdentityPointer()
        {
            const auto screen=juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
            updateIdentityPointer(getLocalPoint(nullptr,screen));
        }
        void prepareUnderlayIfNeeded(juce::Graphics& g)
        {
            const int padding=host.painter.identityBackdropPadding();
            if(!canvas.component||padding<=0||identityLayout.backdrop.isEmpty())return;
            const float scale=juce::jlimit(1.0f,4.0f,g.getInternalContext().getPhysicalPixelScaleFactor());
            if(!underlayDirty&&scale==underlayScale)return;
            underlayBounds=identityLayout.backdrop.expanded(padding).getIntersection(canvas.component->getBounds());
            const auto local=underlayBounds.translated(-canvas.component->getX(),-canvas.component->getY());
            // Snapshot only the underlying CANVAS subtree, never LaneRow. This
            // cannot capture/recurse into the lens and has bounded pixel area.
            auto pixels=canvas.component->createComponentSnapshot(local,true,scale);
            identityUnderlay=host.painter.prepareIdentityUnderlay(pixels,scale,identityLayout,underlayBounds);
            underlayDirty=false;underlayScale=scale;
        }
        std::optional<double> adjacentTime(bool forward)const
        {
            if(canvas.eventTimes){
                auto times=canvas.eventTimes();
                std::erase_if(times,[](double t){return !std::isfinite(t)||t<0;});
                std::sort(times.begin(),times.end());
                if(forward){for(double t:times)if(t>host.playhead+1e-7)return t;}
                else{for(auto i=times.rbegin();i!=times.rend();++i)if(*i<host.playhead-1e-7)return *i;}
                return {};
            }
            if(!canvas.supportsDocumentEvents)return {};
            return host.controller.adjacentEvent(row.lane,host.playhead,forward);
        }
        void action(LaneButton action)
        {
            const auto* owner=host.controller.owner(row.owner);if(!owner)return;
            if(action==LaneButton::previous||action==LaneButton::next){
                const bool forward=action==LaneButton::next;
                const auto target=adjacentTime(forward);
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
    // Preserve the visible row's offset when automatic rows above it resize.
    // Measurement never depends on lens height, so chrome cannot create loops.
    bool applyAutoHeight(const Id& id,int height)
    {
        Id anchor;double top=0;
        controller.visitRows(scrollY,1,0,[&](const Row& r){if(anchor.empty()){anchor=r.lane;top=r.top;}});
        const auto offset=static_cast<double>(scrollY)-top;
        if(!controller.setAutoHeight(id,height))return false;
        if(!anchor.empty())scrollY=static_cast<std::int64_t>(controller.laneTop(anchor)
            +std::clamp(offset,0.0,static_cast<double>(std::max(0,controller.height(anchor)-1))));
        return true;
    }
    void handleAsyncUpdate()override{refresh();}
    // One panel timer serves BOTH a single pointer candidate and optional meters.
    // At rest with meters hidden it is stopped. No O(number-of-lanes) polling.
    Id pendingIdentity;
    std::optional<double> identityDue;
    double timerWakeAt=0,nextMeterAt=0;
    bool shuttingDown=false;
    bool metersWanted()const {
        const auto& c=controller.view().chrome;
        return isShowing()&&readMeter&&(c.inputMeters||c.outputMeters);
    }
    void scheduleIdentity(const Id& id,std::optional<double> due) {
        if(shuttingDown)return;
        if(due){pendingIdentity=id;identityDue=due;}
        else if(pendingIdentity==id){pendingIdentity.clear();identityDue.reset();}
        updateTimerSchedule();
    }
    void updateTimerSchedule() {
        if(shuttingDown||!isShowing()){stopTimer();timerWakeAt=0;return;}
        const double now=juce::Time::getMillisecondCounterHiRes();
        std::optional<double> wake=identityDue;
        if(metersWanted()) {
            if(nextMeterAt==0)nextMeterAt=now+1000.0/30.0;
            if(!wake||nextMeterAt<*wake)wake=nextMeterAt;
        }else nextMeterAt=0;
        if(!wake){stopTimer();timerWakeAt=0;return;}
        if(!isTimerRunning()||std::abs(timerWakeAt-*wake)>0.5) {
            timerWakeAt=*wake;
            startTimer(std::max(1,static_cast<int>(std::ceil(*wake-now))));
        }
    }
    void timerCallback()override {
        stopTimer();timerWakeAt=0;
        const double now=juce::Time::getMillisecondCounterHiRes();
        const bool meterTick=metersWanted()&&now>=nextMeterAt;
        if(identityDue&&now>=*identityDue) {
            const auto key=pendingIdentity;pendingIdentity.clear();identityDue.reset();
            if(auto* row=cache.find(key))row->pollIdentityPointer();
        }
        if(meterTick) {
            cache.forEach([](const Id&,LaneRow& row,bool visible){if(visible)row.updateMeters();});
            nextMeterAt=now+1000.0/30.0;
        }
        updateTimerSchedule();
    }
    void dragTo(juce::Point<float> p)
    {
        if(p.y<20)scrollY=std::max<std::int64_t>(0,scrollY-20);else if(p.y>body.getHeight()-20)scrollY+=20;
        dropLane.clear();controller.visitRows(scrollY+static_cast<std::int64_t>(std::max(0.0f,p.y)),1,0,[&](const Row& r){if(!r.lane.empty())dropLane=r.lane;});
        refresh();
    }
};
}
