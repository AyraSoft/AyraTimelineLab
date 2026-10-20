/** Compatibility composition TU. Compile this INSTEAD OF app/LabComponent.cpp.
    The approved canvas classes currently live in its anonymous namespace. Including
    the original TU once avoids copying their painters and gesture algorithms.
    This dependency stays in demo/, never in the portable panel module. */
#include "../app/LabComponent.cpp"
#include "TimelinePanelDemo.h"
#include "PanelFixture.h"
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
using PanelCanvasStates=std::map<std::pair<panel::Id,panel::VisualKind>,std::shared_ptr<void>>;
template<class Canvas>
panel::LaneCanvas wrapPanelCanvas(std::unique_ptr<Canvas> component,LabComponent& legacy,
                                  PanelCanvasStates& states,const panel::Lane& sourceLane)
{
    auto* raw=component.get();const auto key=std::make_pair(sourceLane.id,sourceLane.visual);
    if(const auto it=states.find(key);it!=states.end())raw->restorePanelState(it->second);
    panel::LaneCanvas result;
    if constexpr(requires {raw->containsPanelClipAt(juce::Point<float>{});})
        result.containsClipAt=[raw](juce::Point<float> point){return raw->containsPanelClipAt(point);};
    result.saveState=[raw,&states,key]{states[key]=raw->capturePanelState();};
    result.hasInteraction=[raw]{return raw->isMouseButtonDown(true)||raw->hasKeyboardFocus(true);};
    if constexpr(requires {raw->panelEventTimes();})result.eventTimes=[raw]{return raw->panelEventTimes();};
    auto window=std::make_shared<PanelClipWindow>();
    result.bind=[raw,&legacy,window](const panel::Lane& lane,const panel::CanvasFrame& frame)
    {
        raw->panelView.enabled=true;raw->panelView.time=frame.time;
        raw->panelView.selected=frame.selected;raw->panelView.ownerColour=juce::Colour(frame.ownerColour);
        if constexpr(requires {raw->setWarpFeatureEnabled(true);})raw->setWarpFeatureEnabled(legacy.isWarpFeatureEnabled());
        if constexpr(requires {raw->setGainFeatureEnabled(true);})raw->setGainFeatureEnabled(legacy.isGainFeatureEnabled());
        if constexpr(requires {raw->setOverlayFeatureEnabled(true);})raw->setOverlayFeatureEnabled(legacy.isGainFeatureEnabled());
        if(const auto* source=findTrack(legacy.store().state(),lane.id))
        {
            if constexpr(requires {raw->panelEventTimes();})raw->setModel(*source,frame.height);
            else {
                window->update(*source,frame.revision);raw->panelView.canonicalClipIds=window->canonicalIds;
                auto wanted=window->visible(frame.time,frame.width,juce::Component::isMouseButtonDownAnywhere());
                if constexpr(requires {raw->includePanelPartners(wanted);})raw->includePanelPartners(wanted);
                auto projection=window->projection(wanted,frame.height);raw->setModel(projection,frame.height);
            }
        }
    };
    result.component=std::move(component);return result;
}
class TimelinePanelDemo final:public juce::Component,private juce::AsyncUpdater
{
public:
    TimelinePanelDemo()
    {
        setWantsKeyboardFocus(true);
        // Retain the old host API, not its old row model or per-change layout work.
        legacy.store()=Store(makePanelFixture());
        legacy.store().changed=nullptr;
        bridge=std::make_unique<PanelFixtureHost>(legacy.store());
        bridge->onChanged=[this]{triggerAsyncUpdate();};
        bridge->onOpenOwner=[this](const auto& id){openOwner(id);};
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
        addButton(structured,[this]{changeMode(panel::Mode::structured);});
        addButton(free,[this]{changeMode(panel::Mode::freeLanes);});
        addButton(groupLanes,[this]{groupSelectedLanes();});
        addButton(ungroupLanes,[this]{ungroupSelectedLanes();});
        addButton(undoView,[this]{controller.undoView();surface->refresh();updateStatus();});
        addButton(redoView,[this]{controller.redoView();surface->refresh();updateStatus();});
        addButton(theme,[this]{dark=!dark;applyStyle();});
        addButton(visibility,[this]{surface->showVisibilityMenu(visibility);});
        addButton(chooseClips,[this]{chooseEvents();});
        addButton(groupClips,[this]{edit(panel::CommandKind::temporalGroup);});
        addButton(dissolve,[this]{edit(panel::CommandKind::dissolveGroup);});
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
        groupClips.setTooltip("Gruppo temporale reversibile nella stessa lane. NON esegue un Glue PCM.");
        link.setTooltip("Link esplicito delle clip selezionate cross-lane: timing condiviso, gain/fade indipendenti");
        chooseClips.setTooltip("Selezione cross-lane per identita'; disponibile anche dal menu contestuale della clip");
        applyStyle();updateFeatures();updateStatus();setSize(1480,1000);
    }
    ~TimelinePanelDemo() override
    {
        cancelPendingUpdate();legacy.onTimelineContextMenuRequested=nullptr;
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
    juce::TextButton structured{"Struttura"},free{"Lane libere"},groupLanes{"Gruppo lane"},ungroupLanes{"Sciogli lane"};
    juce::TextButton undoView{"Undo vista"},redoView{"Redo vista"},theme{"Tema"};
    juce::TextButton chooseClips{"Clip..."},groupClips{"Group tempo"},dissolve{"Dissolve"},link{"Link clip"},unlink{"Unlink"};
    juce::TextButton undoEdit{"Undo edit"},redoEdit{"Redo edit"},colours{"Colori tipo"},warp{"Warp"},gain{"Gain"};
    juce::TextButton visibility{"Visibilita"};
    juce::Slider zoom;juce::TextEditor search;juce::Label status;
    juce::TooltipWindow tooltips{this,350};
    std::vector<panel::EventRef> selectedEvents;
    std::map<std::string,int> previousLegacyHeights;
    bool dark=true,warpOn=true,gainOn=true;unsigned groupSequence=0;
    std::string message="Accento: selezione/riordino. Bordo inferiore: resize. Identita non editabile: si nasconde sopra le clip al passaggio del mouse. Meter sintetici opzionali.";
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
    void updateFeatures()
    { surface->invalidateCanvasFrames();warp.setToggleState(warpOn,juce::dontSendNotification);gain.setToggleState(gainOn,juce::dontSendNotification); }
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
        std::erase_if(selectedEvents,[this](const auto& ref){return controller.event(ref)==nullptr;});
        surface->refresh();updateStatus();
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
        const auto result=controller.edit(kind,selectedEvents);if(result)message="Comando inviato come una transazione fixture";report(result);
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
