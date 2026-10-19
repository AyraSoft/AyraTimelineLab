/** Compatibility composition TU. Compile this INSTEAD OF app/LabComponent.cpp.
    The approved canvas classes currently live in its anonymous namespace. Including
    the original TU once avoids copying their painters and gesture algorithms.
    This dependency stays in demo/, never in the portable panel module. */
#include "../app/LabComponent.cpp"
#include "TimelinePanelDemo.h"
#include "PanelFixture.h"
#include "../modules/ayra_timeline_panel/TimelinePanel.h"

namespace ayra::lab
{
namespace
{
template<class Canvas>
panel::LaneCanvas wrapPanelCanvas(std::unique_ptr<Canvas> component,LabComponent& legacy)
{
    auto* raw=component.get();
    panel::LaneCanvas result;
    result.bind=[raw,&legacy](const panel::Lane& lane,const panel::CanvasFrame& frame)
    {
        if(const auto* source=findTrack(legacy.store().state(),lane.id))
        {
            auto projection=*source;projection.height=frame.height;
            raw->setModel(projection,frame.height);
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
        surface=std::make_unique<panel::TimelinePanel>(controller,[this](const auto& lane){return makeCanvas(lane);});
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
        for(auto* b:{&chooseClips,&groupClips,&dissolve,&link,&unlink,&undoEdit,&redoEdit,&colours,&warp,&gain})
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
    panel::Controller controller;
    std::unique_ptr<PanelFixtureHost> bridge;
    std::unique_ptr<panel::TimelinePanel> surface;
    juce::TextButton structured{"Struttura"},free{"Lane libere"},groupLanes{"Gruppo lane"},ungroupLanes{"Sciogli lane"};
    juce::TextButton undoView{"Undo vista"},redoView{"Redo vista"},theme{"Tema"};
    juce::TextButton chooseClips{"Clip..."},groupClips{"Group tempo"},dissolve{"Dissolve"},link{"Link clip"},unlink{"Unlink"};
    juce::TextButton undoEdit{"Undo edit"},redoEdit{"Redo edit"},colours{"Colori tipo"},warp{"Warp"},gain{"Gain"};
    juce::Slider zoom;juce::TextEditor search;juce::Label status;
    juce::TooltipWindow tooltips{this,350};
    std::vector<panel::EventRef> selectedEvents;
    std::vector<std::function<void()>> featureUpdates;
    std::map<std::string,int> previousLegacyHeights;
    bool dark=true,warpOn=true,gainOn=true;unsigned groupSequence=0;
    std::string message="Seleziona l'accento; trascina dal bordo sinistro; ridimensiona il separatore. Nessun meter.";
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
        if(lane.visual==V::audio)
        {
            auto c=std::make_unique<AudioClipCanvas>(legacy);auto* ptr=c.get();
            featureUpdates.push_back([this,ptr]{ptr->setWarpFeatureEnabled(warpOn);ptr->setGainFeatureEnabled(gainOn);});
            featureUpdates.back()();return wrapPanelCanvas(std::move(c),legacy);
        }
        if(lane.visual==V::midi)
        {
            auto c=std::make_unique<MidiClipCanvas>(legacy);auto* ptr=c.get();
            featureUpdates.push_back([this,ptr]{ptr->setWarpFeatureEnabled(warpOn);ptr->setOverlayFeatureEnabled(gainOn);});
            featureUpdates.back()();return wrapPanelCanvas(std::move(c),legacy);
        }
        if(lane.visual==V::image)
        {
            auto c=std::make_unique<ImageClipCanvas>(legacy);auto* ptr=c.get();
            featureUpdates.push_back([this,ptr]{ptr->setWarpFeatureEnabled(warpOn);ptr->setGainFeatureEnabled(gainOn);});
            featureUpdates.back()();return wrapPanelCanvas(std::move(c),legacy);
        }
        if(lane.visual==V::line)
            return wrapPanelCanvas(std::make_unique<LineClipCanvas>(legacy,timeline_visual_style::CurveLineColourVariant::light,
                TimelineCurveUnderlayStyle::darkBelow),legacy);
        panel::LaneCanvas result;
        if(lane.visual==V::continuous)
        {
            auto c=std::make_unique<ContinuousLineCanvas>(legacy,timeline_visual_style::CurveLineColourVariant::tempoTeal);
            if(lane.id=="tempo")c->setRange(40.0f,220.0f,120.0f,1);
            result=wrapPanelCanvas(std::move(c),legacy);
        }
        else result=wrapPanelCanvas(std::make_unique<KeyframeLaneCanvas>(legacy,
            lane.visual==V::point?KeyframeLaneType::points:KeyframeLaneType::contiguousSpans),legacy);
        result.supportsDocumentEvents=false;return result;
    }
    void updateFeatures(){for(auto& update:featureUpdates)update();warp.setToggleState(warpOn,juce::dontSendNotification);gain.setToggleState(gainOn,juce::dontSendNotification);}
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
        applyStyle();surface->refresh();updateStatus();
    }
    void applyStyle()
    {
        legacy.look().setDark(dark);const auto& p=legacy.look().colours();panel::PanelStyle style;
        style.background=p.base;style.seam=p.raised;style.foreground=p.text;style.border=p.border;
        style.streamColour=[this](panel::Stream stream)
        {return juce::Colour(legacy.store().state().panelMetadata->streamColours.at(static_cast<std::size_t>(stream)));};
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
        status.setText(panel::text(message)+"   ["+juce::String(static_cast<int>(selectedEvents.size()))+" clip] "+panel::text(selection),juce::dontSendNotification);
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
                controller.select(lane.id,panel::Controller::Selection::replace);surface->refresh();surface->revealLane(lane.id);updateStatus();return;
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
