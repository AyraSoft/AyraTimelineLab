/** Compatibility composition TU. Compile this INSTEAD OF app/LabComponent.cpp.
    The approved canvas classes currently live in its anonymous namespace. Including
    the original TU once avoids copying their painters and gesture algorithms.
    This dependency stays in demo/, never in the portable panel module. */
#include "../app/LabComponent.cpp"
#include "TimelinePanelDemo.h"
#include "PanelFixture.h"
#include "../modules/ayra_timeline_panel/TimelineGrouping.h"
#include "PanelClipWindow.h"
#include "TimelineIdentityArtwork.h"
#include "TimelineLaneControls.h"
#include "../modules/ayra_timeline_panel/TimelinePanel.h"

namespace ayra::lab
{
namespace
{
// Artwork is an adapter to the same public Ayra primitives already used by this
// Lab. No copied meter renderer, recessed recipe, accent painter or icon data.
class PanelArtwork final : public panel::TimelinePanelLookAndFeel
{
public:
    explicit PanelArtwork(LabLookAndFeel& look):laf(look){}
    std::unique_ptr<juce::Button> makeButton(panel::LaneButton kind) override
    {return TimelineLaneControls::makeButton(kind);}
    void drawControlGroup(juce::Graphics& g,juce::Rectangle<float> bounds)const override
    {TimelineLaneControls::drawGroup(g,bounds);}
    juce::Image prepareIdentity(const panel::TimelineIdentityContent& c,const panel::TimelineIdentityLayout& layout)const override
    {return TimelineIdentityArtwork::prepare(c,layout);}
    int identityBackdropPadding()const noexcept override {return 15;}
    juce::Image prepareIdentityUnderlay(const juce::Image& source,float scale,
                                       const panel::TimelineIdentityLayout& layout,juce::Rectangle<int> bounds)const override
    {return TimelineIdentityArtwork::prepareUnderlay(source,scale,layout,bounds);}
    void drawIdentityUnderlay(juce::Graphics& g,const panel::TimelineIdentityLayout& layout,
                              const juce::Image& image,juce::Rectangle<int> bounds)const override
    {TimelineIdentityArtwork::drawUnderlay(g,layout,image,bounds);}
    void drawIdentity(juce::Graphics& g,const panel::TimelineIdentityContent& c,const panel::TimelineIdentityLayout& layout,const juce::Image& image)const override
    {TimelineIdentityArtwork::draw(g,c,layout,image);}
    void drawAccent(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c) const override
    { juce::Path clip;clip.addRoundedRectangle(r,2.0f);ayra::drawAccentSpine(g,r,clip,c,r.getWidth()); }
    void drawMeter(juce::Graphics& g,juce::Rectangle<float> r,float value) const override
    { laf.drawVMeter(g,r,value); }
private: LabLookAndFeel& laf;
};
struct PanelCanvasStateEntry
{
    std::shared_ptr<void> state;
    std::uint64_t version=0;
};
using PanelCanvasStates=std::map<std::pair<panel::Id,panel::VisualKind>,PanelCanvasStateEntry>;

std::vector<ClipSplitView> projectClipSplits(const Session& session,const panel::Id& laneId)
{
    std::vector<ClipSplitView> result;
    if(!session.panelMetadata)return result;
    for(const auto& split:session.panelMetadata->splitLineage)
        if(split.left.lane==laneId&&split.right.lane==laneId)
            result.push_back({split.left.clip,split.right.clip,static_cast<float>(split.fraction)});
    return result;
}

template<class Canvas>
panel::LaneCanvas wrapPanelCanvas(std::unique_ptr<Canvas> component,LabComponent& legacy,
                                  PanelCanvasStates& states,const panel::Lane& sourceLane)
{
    static_assert(requires(const Canvas& c){c.containsPanelEditableAt(juce::Point<float>{});}
               ||requires(const Canvas& c){c.containsPanelClipAt(juce::Point<float>{});},
                  "Every canvas must expose its real editable-content hit geometry");
    auto* raw=component.get();const auto key=std::make_pair(sourceLane.id,sourceLane.visual);
    auto appliedStateVersion=std::make_shared<std::uint64_t>(0);
    raw->panelView.cycleHeight=[&legacy, id=sourceLane.id]{if(legacy.onPanelHeightCycle)legacy.onPanelHeightCycle(id);};
    raw->panelView.prepareGlue=[&legacy,id=sourceLane.id](const std::string& clip){
        return legacy.onPrepareGlue?legacy.onPrepareGlue(id,clip):std::function<void()>{};
    };
    raw->panelView.openContainerEditor=[&legacy,&states,key,raw,appliedStateVersion,id=sourceLane.id](const std::string& clip,
        juce::Component& source,juce::Rectangle<int> anchor)
    {
        auto& entry=states[key];
        if(entry.state&&entry.version!=*appliedStateVersion)
        {raw->restorePanelState(entry.state);*appliedStateVersion=entry.version;}
        entry.state=raw->capturePanelState();++entry.version;
        *appliedStateVersion=entry.version;
        if(legacy.onOpenClipContainer)legacy.onOpenClipContainer(id,clip,source,anchor);
    };
    if(const auto it=states.find(key);it!=states.end()&&it->second.state)
    {raw->restorePanelState(it->second.state);*appliedStateVersion=it->second.version;}
    panel::LaneCanvas result;
    if constexpr(requires {raw->containsPanelEditableAt(juce::Point<float>{});})
        result.containsEditableAt=[raw](juce::Point<float> point){return raw->containsPanelEditableAt(point);};
    if constexpr(requires {raw->containsPanelClipAt(juce::Point<float>{});})
        result.containsClipAt=[raw](juce::Point<float> point){return raw->containsPanelClipAt(point);};
    if constexpr(requires {raw->preferredPanelHeight(0);})
        result.preferredHeight=[raw](int width){return raw->preferredPanelHeight(width);};
    if constexpr(requires {raw->onPanelContentChanged;})
        result.setContentChangeListener=[raw](std::function<void()> listener){raw->onPanelContentChanged=std::move(listener);};
    result.saveState=[raw,&states,key,appliedStateVersion]{auto& entry=states[key];
        if(entry.state&&entry.version!=*appliedStateVersion)return;
        entry.state=raw->capturePanelState();++entry.version;*appliedStateVersion=entry.version;};
    result.hasInteraction=[raw]{
        bool pending=false;
        if constexpr(requires{raw->hasPendingPanelGroupGesture();})pending=raw->hasPendingPanelGroupGesture();
        return pending||raw->isMouseButtonDown(true)||raw->hasKeyboardFocus(true);
    };
    if constexpr(requires {raw->panelEventTimes();})result.eventTimes=[raw]{return raw->panelEventTimes();};
    auto window=std::make_shared<PanelClipWindow>();
    result.bind=[raw,&legacy,&states,key,appliedStateVersion,window](const panel::Lane& lane,const panel::CanvasFrame& frame)
    {
        if(const auto saved=states.find(key);saved!=states.end()&&saved->second.state
            &&saved->second.version!=*appliedStateVersion)
        {raw->restorePanelState(saved->second.state);*appliedStateVersion=saved->second.version;}
        raw->panelView.enabled=true;raw->panelView.time=frame.time;
        raw->panelView.selected=frame.selected;raw->panelView.ownerColour=juce::Colour(frame.ownerColour);
        if(raw->panelView.documentProjectionRevision!=frame.revision) {
            raw->panelView.clipSplits=projectClipSplits(legacy.store().state(),lane.id);
            raw->panelView.documentProjectionRevision=frame.revision;
        }
        if constexpr(requires {raw->setWarpFeatureEnabled(true);})raw->setWarpFeatureEnabled(legacy.isWarpFeatureEnabled());
        if constexpr(requires {raw->setGainFeatureEnabled(true);})raw->setGainFeatureEnabled(legacy.isGainFeatureEnabled());
        if constexpr(requires {raw->setOverlayFeatureEnabled(true);})raw->setOverlayFeatureEnabled(legacy.isGainFeatureEnabled());
        if(const auto* source=findTrack(legacy.store().state(),lane.id))
        {
            if constexpr(requires {raw->panelEventTimes();})raw->setModel(*source,frame.height);
            else {
                window->update(*source,frame.revision);raw->panelView.canonicalClipIds=window->canonicalIds;
                auto wanted=window->visible(frame.time,frame.width,juce::Component::isMouseButtonDownAnywhere());
                bool addedSplitPartner=true;
                while(addedSplitPartner)
                {
                    addedSplitPartner=false;
                    for(const auto& split:raw->panelView.clipSplits)
                        if(wanted.contains(split.leftClipId)||wanted.contains(split.rightClipId))
                        {
                            addedSplitPartner=wanted.insert(split.leftClipId).second||addedSplitPartner;
                            addedSplitPartner=wanted.insert(split.rightClipId).second||addedSplitPartner;
                        }
                }
                if constexpr(requires {raw->includePanelPartners(wanted);})raw->includePanelPartners(wanted);
                auto projection=window->projection(wanted,frame.height);raw->setModel(projection,frame.height);
            }
        }
    };
    result.component=std::move(component);return result;
}

/** Shell app-specifico del sotto-arrangement Glue. Il documento resta nello
    Store; questo oggetto possiede soltanto il canvas temporaneo del CallOutBox. */
class ClipContainerEditor final:public juce::Component
{
public:
    ClipContainerEditor(std::unique_ptr<juce::Component> content,juce::String titleText,
                        int expandedHeight,std::function<void(int)> bindCallback,
                        std::function<void()> dissolveCallback,
                        std::function<void()> closeCallback)
        :canvas(std::move(content)),expanded(std::max(96,expandedHeight)),
         modelHeight(expanded),bind(std::move(bindCallback)),onClose(std::move(closeCallback))
    {
        title.setText(std::move(titleText),juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)).boldened());
        title.setJustificationType(juce::Justification::centredLeft);
        hint.setText("Doppio click sul corpo di un Glue: entra nel livello successivo. Header: altezza.",
                     juce::dontSendNotification);
        hint.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        hint.setJustificationType(juce::Justification::centredRight);
        dissolve.setTooltip("Scioglie questo Glue e ripristina le clip nella lane che lo contiene.");
        dissolve.onClick=std::move(dissolveCallback);
        addAndMakeVisible(title);addAndMakeVisible(hint);addAndMakeVisible(dissolve);addAndMakeVisible(*canvas);
        setSize(780,expanded+38);refresh();
    }
    ~ClipContainerEditor() override {disconnectHost(true);}
    void disconnectHost(const bool persistState)
    {
        canvas->setLookAndFeel(nullptr);bind={};dissolve.onClick={};
        auto close=std::move(onClose);onClose={};
        if(persistState&&close)close();
    }
    void refresh(){if(bind)bind(modelHeight);repaint();}
    void cycleHeight()
    {
        modelHeight=modelHeight<=32?expanded:32;
        setSize(getWidth(),modelHeight+38);resized();refresh();
    }
    void resized()override
    {
        auto area=getLocalBounds();auto header=area.removeFromTop(34).reduced(8,2);
        title.setBounds(header.removeFromLeft(std::min(260,header.getWidth()/2)));
        dissolve.setBounds(header.removeFromRight(112).reduced(2));
        hint.setBounds(header);canvas->setBounds(area.reduced(4));
    }
    void paint(juce::Graphics& g)override
    {
        g.fillAll(juce::Colour(0xff0d1218));
        g.setColour(juce::Colour(0xff27313c));g.drawRect(getLocalBounds(),1);
    }
private:
    std::unique_ptr<juce::Component> canvas;
    juce::Label title,hint;
    juce::TextButton dissolve{"Sciogli Glue"};
    int expanded=280,modelHeight=280;
    std::function<void(int)> bind;
    std::function<void()> onClose;
};

class TimelinePanelDemo final:public juce::Component,private juce::AsyncUpdater
{
    friend struct CanvasContractAccess;
public:
    TimelinePanelDemo()
    {
        setWantsKeyboardFocus(true);
        // Retain the old host API, not its old row model or per-change layout work.
        legacy.store()=Store(makePanelFixture());
        legacy.store().changed=nullptr;
        legacy.onPanelHeightCycle=[this](const std::string& id){if(controller.cycleHeight(id))surface->refresh();};
        bridge=std::make_unique<PanelFixtureHost>(legacy.store());
        bridge->onChanged=[this]{triggerAsyncUpdate();};
        bridge->onOpenOwner=[this](const auto& id){openOwner(id);};
        legacy.onSplitClip=[this](const std::string& lane,const std::string& clip,double time)
        {
            auto split=bridge->splitClip(lane,clip,time);report(split.result);return split.rightClipId;
        };
        controller.onCommand=[this](const panel::Command& c){return bridge->apply(c);};
        const auto initial=controller.adopt(surfaceSnapshot());
        if(!initial)throw std::invalid_argument(initial.message);
        for(const auto& lane:controller.snapshot().lanes)
            if(const auto* track=findTrack(legacy.store().state(),lane.id))previousLegacyHeights[lane.id]=track->height;
        controller.setMode(panel::Mode::freeLanes);
        surface=std::make_unique<panel::TimelinePanel>(controller,[this](const auto& lane){return makeCanvas(lane);},artwork);
        surface->readMeter=[](const panel::Lane& lane,bool input){
            // Explicitly synthetic READOUT for style testing, never labelled live audio.
            panel::MeterReadback r;r.available=true;
            r.count=std::min(8,input?lane.inputMeterCount:lane.outputMeterCount);
            const double seconds=juce::Time::getMillisecondCounterHiRes()*0.001;
            const double seed=static_cast<double>(std::hash<std::string>{}(lane.owner)%100)/10.0;
            for(int i=0;i<r.count;++i)r.values[static_cast<std::size_t>(i)]=static_cast<float>(0.15+0.7*std::abs(std::sin(seconds*1.7+seed+i+(input?0.0:0.4))));
            return r;
        };
        surface->onResult=[this](const auto& r){report(r);};
        surface->onSelectionChanged=[this]{updateStatus();};
        surface->onSeek=[this](double){updateStatus();}; // No transport or audio engine in this showcase.
        addAndMakeVisible(*surface);setLookAndFeel(&legacy.look());
        legacy.onTimelineContextMenuRequested=[this](const clips::TimelinePointerContext& c){clipMenu(c);};
        legacy.onPrepareGlue=[this](const std::string& lane,const std::string& clip){
            const auto snapshot=bridge->snapshot();
            auto selected=selectedEvents;
            return std::function<void()>{[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),
                lane,clip,selected=std::move(selected),session=snapshot.session,revision=snapshot.revision]{
                if(!safe)return;
                const auto current=safe->bridge->snapshot();
                if(current.session!=session || current.revision!=revision) {
                    safe->report(panel::Result::fail("Documento cambiato durante il gesto Glue: operazione annullata"));return;
                }
                std::vector<panel::EventRef> targets;
                auto result=panel::glueContainerTargets(current,lane,clip,selected,targets);
                if(result) {
                    panel::Command c;c.kind=panel::CommandKind::glueClips;
                    c.session=session;c.revision=revision;c.events=targets;
                    result=safe->bridge->apply(c);
                    if(result){safe->selectedEvents={{lane,safe->legacy.store().state().selectedClip}};
                        safe->message="Container Glue creato (Undo disponibile)";}
                }
                safe->report(result);
            }};
        };
        legacy.onOpenClipContainer=[this](const std::string& lane,const std::string& clip,
                                          juce::Component& source,juce::Rectangle<int> anchor)
        {
            openContainerEditor(lane,clip,source.localAreaToGlobal(anchor));
        };
        addButton(structured,[this]{changeMode(panel::Mode::structured);});
        addButton(free,[this]{changeMode(panel::Mode::freeLanes);});
        addButton(groupLanes,[this]{groupSelectedLanes();});
        addButton(ungroupLanes,[this]{ungroupSelectedLanes();});
        addButton(undoView,[this]{controller.undoView();surface->refresh();updateStatus();});
        addButton(redoView,[this]{controller.redoView();surface->refresh();updateStatus();});
        addButton(theme,[this]{dark=!dark;applyStyle();});
        addButton(visibility,[this]{surface->showVisibilityMenu(visibility);});
        addButton(chooseClips,[this]{chooseEvents();});
        addButton(groupClips,[this]{edit(panel::CommandKind::glueClips);});
        addButton(dissolve,[this]{edit(panel::CommandKind::dissolveContainer);});
        addButton(link,[this]{edit(panel::CommandKind::linkClips);});
        addButton(unlink,[this]{edit(panel::CommandKind::unlinkClips);});
        addButton(undoEdit,[this]{legacy.store().undo();});
        addButton(redoEdit,[this]{legacy.store().redo();});
        addButton(colours,[this]{chooseColour();});
        addButton(warp,[this]{warpOn=!warpOn;legacy.setWarpFeatureEnabled(warpOn);updateFeatures();});
        addButton(gain,[this]{gainOn=!gainOn;legacy.setGainFeatureEnabled(gainOn);updateFeatures();});
        zoom.setRange(16,160,1);zoom.setValue(64,juce::dontSendNotification);
        zoom.setSliderStyle(juce::Slider::LinearHorizontal);zoom.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,20);
        zoom.setTooltip("Zoom orizzontale; i controlli restano fissi ai bordi");
        zoom.onValueChange=[this]{surface->setPixelsPerSecond(zoom.getValue());};addAndMakeVisible(zoom);
        search.setTextToShowWhenEmpty("Trova traccia o lane...",juce::Colour(0xff8b939f));
        search.onTextChange=[this]{findLane();};addAndMakeVisible(search);
        status.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));addAndMakeVisible(status);
        groupClips.setTooltip("Crea un container Glue editabile e annidabile nella stessa lane.");
        link.setTooltip("Link esplicito delle clip selezionate cross-lane: timing condiviso, gain/fade indipendenti");
        chooseClips.setTooltip("Selezione cross-lane per identita'; disponibile anche dal menu contestuale della clip");
        applyStyle();updateFeatures();updateStatus();setSize(1480,1000);
    }
    ~TimelinePanelDemo() override
    {
        if(activeContainerEditor)activeContainerEditor->disconnectHost(false);
        if(activeContainerCallout)activeContainerCallout->dismiss();
        cancelPendingUpdate();legacy.onTimelineContextMenuRequested=nullptr;legacy.onPrepareGlue=nullptr;
        legacy.onOpenClipContainer=nullptr;
        legacy.onSplitClip=nullptr;legacy.onPanelHeightCycle={};
        surface.reset();bridge.reset();setLookAndFeel(nullptr);
    }
    void resized()override
    {
        auto area=getLocalBounds().reduced(8);auto first=area.removeFromTop(30);
        for(auto* b:{&structured,&free,&groupLanes,&ungroupLanes,&undoView,&redoView,&theme})
            b->setBounds(first.removeFromLeft(106).reduced(2));
        zoom.setBounds(first.removeFromRight(160).reduced(2));search.setBounds(first.reduced(2));
        auto second=area.removeFromTop(30);
        for(auto* b:{&chooseClips,&groupClips,&dissolve,&link,&unlink,&undoEdit,&redoEdit,&colours,&warp,&gain,&visibility})
            b->setBounds(second.removeFromLeft(104).reduced(2));
        status.setBounds(area.removeFromBottom(30));surface->setBounds(area);
    }
    void paint(juce::Graphics& g)override{g.fillAll(legacy.look().colours().base);}
    bool keyPressed(const juce::KeyPress& k)override
    {
        if(k==juce::KeyPress::escapeKey){selectedEvents.clear();updateStatus();return true;}
        if(k.getModifiers().isCommandDown()||k.getModifiers().isCtrlDown())
        {
            if(k.getKeyCode()=='Z'){if(k.getModifiers().isShiftDown())legacy.store().redo();else legacy.store().undo();return true;}
        }
        return false;
    }
private:
    LabComponent legacy; // owns the approved LAF and the compatibility Store, not a second musical document
    PanelArtwork artwork{legacy.look()};
    PanelCanvasStates canvasStates;
    panel::Controller controller;
    std::unique_ptr<PanelFixtureHost> bridge;
    std::unique_ptr<panel::TimelinePanel> surface;
    juce::Component::SafePointer<ClipContainerEditor> activeContainerEditor;
    juce::Component::SafePointer<juce::CallOutBox> activeContainerCallout;
    panel::Id activeContainerLane,activeContainerId;
    juce::TextButton structured{"Struttura"},free{"Lane libere"},groupLanes{"Gruppo lane"},ungroupLanes{"Sciogli lane"};
    juce::TextButton undoView{"Undo vista"},redoView{"Redo vista"},theme{"Tema"};
    juce::TextButton chooseClips{"Clip..."},groupClips{"Glue clip"},dissolve{"Dissolve"},link{"Link clip"},unlink{"Unlink"};
    juce::TextButton undoEdit{"Undo edit"},redoEdit{"Redo edit"},colours{"Colori tipo"},warp{"Warp"},gain{"Gain"};
    juce::TextButton visibility{"Visibilita"};
    juce::Slider zoom;juce::TextEditor search;juce::Label status;
    juce::TooltipWindow tooltips{this,350};
    std::vector<panel::EventRef> selectedEvents;
    std::map<std::string,int> previousLegacyHeights;
    bool dark=true,warpOn=true,gainOn=true;unsigned groupSequence=0;
    std::string message="Accento: selezione/riordino. Bordo inferiore: resize. Identita e stati non editabili; frecce nella card. Scomparsa dopo 500 ms sopra clip, curve ed eventi. Meter sintetici opzionali.";
    template<class F>void addButton(juce::TextButton& b,F f){b.onClick=std::move(f);addAndMakeVisible(b);}
    static bool localPreview(const panel::Lane& lane)
    {return lane.visual==panel::VisualKind::point || lane.visual==panel::VisualKind::span || lane.visual==panel::VisualKind::continuous;}
    panel::Snapshot surfaceSnapshot()const
    {
        auto result=bridge->snapshot();std::set<panel::Id> local;
        for(const auto& lane:result.lanes)if(localPreview(lane))local.insert(lane.id);
        // These existing canvas adapters own local events; do NOT navigate phantom Store clips.
        std::erase_if(result.events,[&](const auto& event){return local.contains(event.ref.lane);});
        return result;
    }
    panel::LaneCanvas makeCanvas(const panel::Lane& lane)
    {
        using V=panel::VisualKind;
        const auto wrap=[&](auto c){return wrapPanelCanvas(std::move(c),legacy,canvasStates,lane);};
        if(lane.visual==V::audio)return wrap(std::make_unique<AudioClipCanvas>(legacy));
        if(lane.visual==V::midi)return wrap(std::make_unique<MidiClipCanvas>(legacy));
        if(lane.visual==V::image)return wrap(std::make_unique<ImageClipCanvas>(legacy));
        if(lane.visual==V::line)return wrap(std::make_unique<LineClipCanvas>(legacy,
            timeline_visual_style::CurveLineColourVariant::light,TimelineCurveUnderlayStyle::darkBelow));
        panel::LaneCanvas result;
        if(lane.visual==V::continuous) {
            auto c=std::make_unique<ContinuousLineCanvas>(legacy,timeline_visual_style::CurveLineColourVariant::tempoTeal);
            if(lane.id=="tempo")c->setRange(40.0f,220.0f,120.0f,1);
            result=wrap(std::move(c));
        } else {
            auto c=std::make_unique<KeyframeLaneCanvas>(legacy,
                lane.visual==V::point?KeyframeLaneType::points:KeyframeLaneType::contiguousSpans);
            const auto neutral=legacy.look().colours().secondary;
            if(lane.id=="range")c->setPanelEvents({{1,0,32,0,"Song range",neutral,false}});
            if(lane.id=="markers")c->setPanelEvents({{1,0,0,0,"Intro",neutral,false},{2,8,0,0,"Verse",neutral,false},{3,18,0,0,"Chorus",neutral,false}});
            if(lane.id=="timesig")c->setPanelEvents({{1,0,0,0,"4/4",neutral,false},{2,16,0,0,"3/4",neutral,false},{3,24,0,0,"4/4",neutral,false}});
            if(lane.id=="counter")c->setPanelEvents({{1,0,0,0,"4",neutral,false},{2,0.5,0,0,"3",neutral,false},{3,1,0,0,"2",neutral,false},{4,1.5,0,0,"1",neutral,false}});
            result=wrap(std::move(c));
        }
        result.supportsDocumentEvents=false;return result;
    }
    std::function<void()> prepareNestedGlue(const panel::Id& laneId,
                                            const panel::Id& parentContainerId,
                                            const panel::Id& clickedClipId)
    {
        const auto before=bridge->snapshot();
        return [safe=juce::Component::SafePointer<TimelinePanelDemo>(this),laneId,
                parentContainerId,clickedClipId,session=before.session,
                revision=before.revision]
        {
            if(!safe)return;
            const auto current=safe->bridge->snapshot();
            if(current.session!=session||current.revision!=revision)
            {safe->report(panel::Result::fail("Documento cambiato durante il gesto Glue"));return;}
            const auto* track=findTrack(safe->legacy.store().state(),laneId);
            const auto* parent=track?findClip(*track,parentContainerId):nullptr;
            if(parent==nullptr||!parent->isContainer())
            {safe->report(panel::Result::fail("Container Glue non disponibile"));return;}
            std::vector<panel::EventRef> targets;
            const auto clicked=std::find_if(parent->children.begin(),parent->children.end(),
                [&](const auto& child){return child.id==clickedClipId;});
            if(clicked!=parent->children.end())
            {
                targets.push_back({laneId,clicked->id});
                if(std::next(clicked)!=parent->children.end())targets.push_back({laneId,std::next(clicked)->id});
            }
            if(targets.size()<2)
            {safe->report(panel::Result::fail("Glue richiede almeno due clip sorelle"));return;}
            panel::Command command;command.kind=panel::CommandKind::glueClips;
            command.session=session;command.revision=revision;command.events=std::move(targets);
            const auto result=safe->bridge->apply(command);
            if(result)safe->message="Glue annidato creato";
            safe->report(result);
        };
    }
    void dissolveOpenContainer(const panel::Id& laneId,
                               const panel::Id& containerId)
    {
        const auto& state=legacy.store().state();
        const auto* track=findTrack(state,laneId);
        const auto* container=track?findClip(*track,containerId):nullptr;
        if(container==nullptr||!container->isContainer())
        {report(panel::Result::fail("Container Glue non disponibile"));return;}
        const auto snapshot=bridge->snapshot();panel::Command command;
        command.kind=panel::CommandKind::dissolveContainer;command.session=snapshot.session;
        command.revision=snapshot.revision;command.events={{laneId,containerId}};
        const auto result=bridge->apply(command);if(result)message="Glue sciolto";report(result);
    }
    template<class Canvas>
    std::unique_ptr<ClipContainerEditor> makeContainerEditor(
        std::unique_ptr<Canvas> canvas,const panel::Lane& lane,
        const panel::Id& containerId,const int expandedHeight)
    {
        auto* raw=canvas.get();const auto key=std::make_pair(lane.id,lane.visual);
        raw->setLookAndFeel(&legacy.look());
        if(const auto saved=canvasStates.find(key);saved!=canvasStates.end()&&saved->second.state)
            raw->restorePanelState(saved->second.state);
        raw->panelView.enabled=true;
        raw->panelView.prepareGlue=[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),
            laneId=lane.id,containerId](const std::string& clicked)
        {
            return safe?safe->prepareNestedGlue(laneId,containerId,clicked):std::function<void()>{};
        };
        raw->panelView.openContainerEditor=[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),
            laneId=lane.id](const std::string& nested,juce::Component& source,juce::Rectangle<int> anchor)
        {
            if(!safe)return;
            const auto screenAnchor=source.localAreaToGlobal(anchor);
            juce::MessageManager::callAsync([safe,laneId,nested,screenAnchor]
            {if(safe)safe->openContainerEditor(laneId,nested,screenAnchor);});
        };
        auto bind=[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),raw,
                   laneId=lane.id,containerId](const int modelHeight)
        {
            if(!safe)return;
            const auto* source=findTrack(safe->legacy.store().state(),laneId);
            const auto* container=source?findClip(*source,containerId):nullptr;
            if(source==nullptr||container==nullptr||!container->isContainer())return;
            Track projection=*source;projection.clips=container->children;projection.height=modelHeight;
            double duration=0.125;
            auto ids=std::make_shared<std::unordered_set<std::string>>();
            for(const auto& child:projection.clips)
                duration=std::max(duration,child.start+child.length);
            for(const auto& root:source->clips)
            {
                std::vector<std::string> nestedIds;collectClipIds(root,nestedIds);
                ids->insert(nestedIds.begin(),nestedIds.end());
            }
            raw->panelView.time={0.0,std::max(1.0,static_cast<double>(raw->getWidth()-8)/duration),0.0};
            raw->panelView.ownerColour=juce::Colour(source->colour);
            raw->panelView.canonicalClipIds=std::move(ids);
            raw->panelView.documentProjectionRevision=safe->bridge->snapshot().revision;
            raw->panelView.clipSplits=projectClipSplits(safe->legacy.store().state(),laneId);
            if constexpr(requires {raw->setWarpFeatureEnabled(true);})raw->setWarpFeatureEnabled(safe->legacy.isWarpFeatureEnabled());
            if constexpr(requires {raw->setGainFeatureEnabled(true);})raw->setGainFeatureEnabled(safe->legacy.isGainFeatureEnabled());
            if constexpr(requires {raw->setOverlayFeatureEnabled(true);})raw->setOverlayFeatureEnabled(safe->legacy.isGainFeatureEnabled());
            raw->setModel(projection,modelHeight);
        };
        auto save=[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),raw,key]
        {
            raw->setLookAndFeel(nullptr);
            if(!safe)return;
            auto& entry=safe->canvasStates[key];entry.state=raw->capturePanelState();++entry.version;
            if(safe->surface){safe->surface->invalidateCanvasFrames();safe->surface->refresh();}
        };
        auto dissolve=[safe=juce::Component::SafePointer<TimelinePanelDemo>(this),
                       laneId=lane.id,containerId]
        {if(safe)safe->dissolveOpenContainer(laneId,containerId);};
        auto editor=std::make_unique<ClipContainerEditor>(std::move(canvas),
            "Dentro Glue / "+panel::text(containerId),expandedHeight,std::move(bind),
            std::move(dissolve),std::move(save));
        raw->panelView.cycleHeight=[safe=juce::Component::SafePointer<ClipContainerEditor>(editor.get())]
        {if(safe)safe->cycleHeight();};
        return editor;
    }
    void openContainerEditor(const panel::Id& laneId,const panel::Id& containerId,
                             const juce::Rectangle<int> screenAnchor)
    {
        const auto* track=findTrack(legacy.store().state(),laneId);
        const auto* container=track?findClip(*track,containerId):nullptr;
        const auto* lane=controller.lane(laneId);
        if(container==nullptr||!container->isContainer()||lane==nullptr)
        {report(panel::Result::fail("La clip selezionata non e' un container Glue"));return;}
        if(activeContainerCallout)activeContainerCallout->dismiss();
        std::unique_ptr<ClipContainerEditor> editor;
        using V=panel::VisualKind;
        if(lane->visual==V::audio)editor=makeContainerEditor(std::make_unique<AudioClipCanvas>(legacy),*lane,containerId,300);
        else if(lane->visual==V::midi)editor=makeContainerEditor(std::make_unique<MidiClipCanvas>(legacy),*lane,containerId,400);
        else if(lane->visual==V::image)editor=makeContainerEditor(std::make_unique<ImageClipCanvas>(legacy),*lane,containerId,300);
        else if(lane->visual==V::line)editor=makeContainerEditor(std::make_unique<LineClipCanvas>(legacy,
            timeline_visual_style::CurveLineColourVariant::light,TimelineCurveUnderlayStyle::darkBelow),*lane,containerId,300);
        if(!editor){report(panel::Result::fail("Questo tipo di lane non supporta clip Glue"));return;}
        auto* editorPtr=editor.get();
        auto& callout=juce::CallOutBox::launchAsynchronously(std::move(editor),screenAnchor,nullptr);
        activeContainerEditor=editorPtr;activeContainerCallout=&callout;
        activeContainerLane=laneId;activeContainerId=containerId;
    }
    void updateFeatures()
    {
        surface->invalidateCanvasFrames();
        if(activeContainerEditor)activeContainerEditor->refresh();
        warp.setToggleState(warpOn,juce::dontSendNotification);gain.setToggleState(gainOn,juce::dontSendNotification);
    }
    void handleAsyncUpdate()override
    {
        const auto adopted=controller.adopt(surfaceSnapshot());
        if(!adopted){report(adopted);return;}
        // Honour the divider gestures still implemented inside the original canvases.
        // New separator-only resizes use ViewState and never enter the document Store.
        for(const auto& lane:controller.snapshot().lanes)
            if(const auto* t=findTrack(legacy.store().state(),lane.id))
            {
                const auto old=previousLegacyHeights.find(lane.id);
                if(old!=previousLegacyHeights.end() && old->second!=t->height)controller.setHeight(lane.id,t->height);
                previousLegacyHeights[lane.id]=t->height;
            }
        if(!legacy.store().lastError().empty())message=legacy.store().lastError();
        std::erase_if(selectedEvents,[this](const auto& ref){return controller.event(ref)==nullptr;});
        surface->refresh();
        const auto* activeTrack=findTrack(legacy.store().state(),activeContainerLane);
        const auto* active=activeTrack?findClip(*activeTrack,activeContainerId):nullptr;
        if(activeContainerEditor&&active&&active->isContainer())activeContainerEditor->refresh();
        else if(activeContainerCallout)activeContainerCallout->dismiss();
        updateStatus();
    }
    void applyStyle()
    {
        legacy.look().setDark(dark);const auto& p=legacy.look().colours();panel::PanelStyle style;
        style.identityTypography=TimelineIdentityArtwork::typography();
        style.background=p.base;style.seam=p.raised;style.foreground=p.text;style.border=p.border;
        style.streamColour=[this](panel::Stream stream)
        {return stream==panel::Stream::global?legacy.look().colours().secondary:juce::Colour(legacy.store().state().panelMetadata->streamColours.at(static_cast<std::size_t>(stream)));};
        surface->setStyle(std::move(style));status.setColour(juce::Label::textColourId,p.secondary);repaint();
    }
    void changeMode(panel::Mode m)
    {
        if(juce::Component::isMouseButtonDownAnywhere() && controller.resizing())return;
        controller.setMode(m);surface->refresh();updateStatus();
    }
    void report(const panel::Result& r){if(!r)message=r.message;updateStatus();}
    void updateStatus()
    {
        structured.setToggleState(controller.view().mode==panel::Mode::structured,juce::dontSendNotification);
        free.setToggleState(controller.view().mode==panel::Mode::freeLanes,juce::dontSendNotification);
        std::string selection;
        for(const auto& ref:selectedEvents){if(!selection.empty())selection+=" | ";selection+=ref.clip;}
        status.setText(juce::String(legacy.look().sourceLabel())+" | "+panel::text(message)+"   ["+juce::String(static_cast<int>(selectedEvents.size()))+" clip] "+panel::text(selection),juce::dontSendNotification);
    }
    void groupSelectedLanes()
    {
        report(controller.groupLayout("layout-"+std::to_string(++groupSequence)));
        surface->refresh();
    }
    void ungroupSelectedLanes()
    {
        controller.ungroupLayout();surface->refresh();updateStatus();
    }

    void findLane()
    {
        const auto needle=search.getText().trim();if(needle.isEmpty())return;
        for(const auto& lane:controller.snapshot().lanes)
        {
            const auto* owner=controller.owner(lane.owner);
            if(panel::text(lane.name).containsIgnoreCase(needle) || (owner&&panel::text(owner->name).containsIgnoreCase(needle)))
            {
                if(controller.view().mode==panel::Mode::structured)
                {
                    std::vector<panel::Id> unfold;
                    for(auto* p=owner;p;p=controller.owner(p->parent))if(controller.view().collapsedOwners.contains(p->id))unfold.push_back(p->id);
                    for(const auto& id:unfold)controller.toggleCollapse(id);
                }
                surface->revealLane(lane.id);updateStatus();return;
            }
        }
    }
    void toggleEvent(const panel::EventRef& ref,bool exclusive)
    {
        if(exclusive)selectedEvents={ref};
        else if(const auto it=std::find(selectedEvents.begin(),selectedEvents.end(),ref);it!=selectedEvents.end())selectedEvents.erase(it);
        else selectedEvents.push_back(ref);
        updateStatus();
    }
    void edit(panel::CommandKind kind)
    {
        auto targets=selectedEvents;
        if(kind==panel::CommandKind::dissolveContainer&&targets.empty())
            for(const auto& event:controller.snapshot().events)
                if(event.ref.clip==legacy.store().state().selectedClip){targets.push_back(event.ref);break;}
        const auto result=controller.edit(kind,targets);if(result)message="Comando inviato come una transazione fixture";report(result);
    }
    void chooseEvents()
    {
        juce::PopupMenu menu;menu.addItem(1,"Svuota selezione");menu.addSeparator();
        auto choices=controller.snapshot().events;int id=2;
        for(const auto& e:choices)
        {
            const bool selected=std::find(selectedEvents.begin(),selectedEvents.end(),e.ref)!=selectedEvents.end();
            menu.addItem(id++,panel::text(e.ref.lane)+"   "+juce::String(e.start,2)+" - "+juce::String(e.end,2)+" s",true,selected);
        }
        const auto session=controller.snapshot().session;const auto revision=controller.snapshot().revision;
        juce::Component::SafePointer<TimelinePanelDemo> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chooseClips),[safe,session,revision,choices=std::move(choices)](int chosen)
        {
            if(safe==nullptr || chosen==0)return;
            if(safe->controller.snapshot().session!=session || safe->controller.snapshot().revision!=revision)
            {safe->report(panel::Result::fail("Selezione scaduta: riapri il menu"));return;}
            if(chosen==1){safe->selectedEvents.clear();safe->updateStatus();}
            else if(juce::isPositiveAndBelow(chosen-2,static_cast<int>(choices.size())))safe->toggleEvent(choices[static_cast<std::size_t>(chosen-2)].ref,false);
        });
    }
    void clipMenu(const clips::TimelinePointerContext& context)
    {
        const panel::EventRef ref{context.trackId,context.itemId};
        if(!controller.event(ref)){message="Canvas con eventi locali: binding documentale non ancora disponibile";updateStatus();return;}
        juce::PopupMenu menu;menu.addItem(1,"Seleziona solo questa clip");menu.addItem(2,"Aggiungi / rimuovi dalla selezione cross-lane");menu.addItem(3,"Apri editor clip");
        const auto session=controller.snapshot().session;const auto revision=controller.snapshot().revision;
        juce::Component::SafePointer<TimelinePanelDemo> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(surface.get()),[safe,ref,session,revision](int chosen)
        {
            if(safe==nullptr || chosen==0)return;
            if(safe->controller.snapshot().session!=session || safe->controller.snapshot().revision!=revision)
            {safe->report(panel::Result::fail("Contesto clip scaduto"));return;}
            if(chosen==1||chosen==2)safe->toggleEvent(ref,chosen==1);
            if(chosen==3)safe->legacy.openClip(ref.lane,ref.clip);
        });
    }
    void chooseColour()
    {
        // Explicit fixture settings, not a parallel production colour registry.
        juce::PopupMenu menu;for(int i=0;i<7;++i)menu.addItem(i+1,panel::streamName(static_cast<panel::Stream>(i)));
        juce::Component::SafePointer<TimelinePanelDemo> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&colours),[safe](int chosen)
        {
            if(safe==nullptr||chosen<1||chosen>7)return;
            auto* dialog=new juce::AlertWindow("Colore tipo - fixture","Inserisci RGB esadecimale; l'accento della traccia resta invariato.",juce::MessageBoxIconType::NoIcon);
            const auto stream=static_cast<panel::Stream>(chosen-1);
            const auto current=safe->legacy.store().state().panelMetadata->streamColours[static_cast<std::size_t>(stream)];
            dialog->addTextEditor("rgb",juce::String::toHexString(static_cast<int>(current&0xffffffu)).paddedLeft('0',6),"RGB");
            dialog->addButton("Applica",1);dialog->addButton("Annulla",0);
            juce::Component::SafePointer<juce::AlertWindow> safeDialog(dialog);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,safeDialog,stream](int answer)
            {
                if(safe==nullptr||safeDialog==nullptr||answer!=1)return;
                const auto hex=safeDialog->getTextEditorContents("rgb").trim();
                if(hex.length()!=6 || !hex.containsOnly("0123456789abcdefABCDEF")){safe->report(panel::Result::fail("RGB non valido"));return;}
                safe->report(safe->bridge->setStreamColour(stream,static_cast<std::uint32_t>(hex.getHexValue32())));
            }),true);
        });
    }
    void openOwner(const panel::Id& id)
    {
        const auto* owner=controller.owner(id);if(!owner)return;
        juce::String lanes;
        for(const auto& lane:controller.snapshot().lanes)if(lane.owner==id)lanes+="\n"+panel::text(lane.id);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Tensor fixture: "+panel::text(owner->name),
            "Owner: "+panel::text(id)+"\nMonitoring condiviso: "+monitoringName(owner->monitoring)+"\nLane:"+lanes+
            "\n\nQuesto e' il punto di integrazione dell'editor Tensor reale. Nessun grafo o motore audio viene creato.");
    }
};
}
std::unique_ptr<juce::Component> makeTimelinePanelDemo(){return std::make_unique<TimelinePanelDemo>();}
}
