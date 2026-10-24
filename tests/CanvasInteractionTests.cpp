/** Actual native canvases, document bridge, timers and paint. No input proxy,
    provider stub or duplicate grouping algorithm. Runs under a real JUCE peer. */
#include "../demo/TimelinePanelDemo.cpp"
#include <iostream>
#include <type_traits>
#include <stdexcept>
namespace ayra::lab { namespace {
struct CanvasContractAccess
{
    template<class C>static auto hit(C& c,juce::Point<float> p){return c.hitTestZones(p);}
    static auto lineState(LineClipCanvas& c,const std::string& id){return c.uiStates.at(id);}
    static auto audioGainPoints(AudioClipCanvas& c,const std::string& id){return c.uiStates.at(id).gainPoints;}
    static void forceMidiScrollbar(MidiClipCanvas& c,const std::string& id)
    {c.uiStates.at(id).pitchScrollbarVisible=true;}
    static bool midiScrollbarVisible(const MidiClipCanvas& c,const std::string& id)
    {return c.uiStates.at(id).pitchScrollbarVisible;}
    template<class C>static auto containerGainPoints(const C& c,const std::string& id)
    {return c.uiStates.at(id).containerGainPoints;}
    template<class C>static std::size_t stateCount(const C& c){return c.uiStates.size();}
    template<class C>static double offset(C& c,const std::string& id){return c.uiStates.at(id).sourceOffsetSec;}
    template<class C>static auto visual(C& c,const std::string& id,juce::Rectangle<float> body){
        if constexpr(std::is_same_v<C,AudioClipCanvas>)return c.makeAudioClipVisual(id,body,{});
        else if constexpr(std::is_same_v<C,MidiClipCanvas>)return c.makeMidiClipVisual(id,body,{});
        else if constexpr(std::is_same_v<C,LineClipCanvas>)return c.makeLineClipVisual(id,body,{});
        else return c.makeImageClipVisual(id,body,{});
    }
    static panel::Controller& controller(TimelinePanelDemo& c){return c.controller;}
    static panel::TimelinePanel& surface(TimelinePanelDemo& c){return *c.surface;}
    static Store& store(TimelinePanelDemo& c){return c.legacy.store();}
    static auto& selection(TimelinePanelDemo& c){return c.selectedEvents;}
    static bool hasContainerEditor(TimelinePanelDemo& c){return c.activeContainerEditor!=nullptr;}
};
}}
namespace {
using namespace ayra::lab;
using namespace ayra::lab::clips;
int checks=0;
void check(bool ok,const char* what,int line){++checks;if(!ok)throw std::runtime_error("line "+std::to_string(line)+": "+what);}
#define CHECK(...) check(bool((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
juce::File output;
juce::MouseEvent event(juce::Component& target,juce::Point<float> p,int clicks=1,
    int mods=juce::ModifierKeys::leftButtonModifier,juce::Point<float> start={-1,-1},bool drag=false)
{
    if(start.x<0)start=p;
    return {juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),
        1,0,0,0,0,&target,&target,juce::Time::getCurrentTime(),start,juce::Time::getCurrentTime(),clicks,drag};
}
void pump(int milliseconds=15){juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);}
template<class Predicate>bool waitFor(Predicate ready,int timeoutMilliseconds=1500)
{
    const auto limit=juce::Time::getMillisecondCounterHiRes()+timeoutMilliseconds;
    while(!ready()&&juce::Time::getMillisecondCounterHiRes()<limit)pump(10);
    return ready();
}
void save(juce::Component& component,const juce::String& name)
{
    auto image=component.createComponentSnapshot(component.getLocalBounds(),true,2.0f);
    const auto file=output.getChildFile(name+".png");file.deleteFile();juce::FileOutputStream stream(file);
    CHECK(stream.openedOk());CHECK(juce::PNGImageFormat{}.writeImageToStream(image,stream));
}
template<class Visual>double containedPreviewLevel(const Visual& visual)
{
    double level=0.0;
    for(int i=0;i<visual.numContainedClipPreviews;++i) {
        const auto& child=visual.containedClipPreviews[i];
        if(child.container.isContainer())continue;
        if constexpr(std::is_same_v<Visual,AudioClipVisual>) {
            for(int channel=0;channel<child.numWaveformPaths;++channel)
                level+=child.waveformPaths[channel].getBounds().getHeight();
        } else if constexpr(std::is_same_v<Visual,MidiClipVisual>) {
            for(int note=0;note<child.numNotes;++note)level+=child.notes[note].velocity;
        } else if constexpr(std::is_same_v<Visual,LineClipVisual>) {
            for(int point=0;point<child.numPoints;++point)level+=child.points[point].normValue;
        } else {
            for(int frame=0;frame<child.numFrames;++frame)
                level+=child.frames[frame].inheritedOpacityScale;
        }
    }
    return level;
}
juce::Component* find(juce::Component& c,const juce::String& id)
{
    if(c.getComponentID()==id)return &c;
    for(auto* child:c.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
template<class C>class Harness
{
public:
    LabComponent host;
    std::unique_ptr<PanelFixtureHost> bridge;
    panel::Controller controller;
    juce::Component root;
    C canvas;
    std::string lane,id;
    int cycles=0,containerOpens=0;
    std::vector<panel::EventRef> selection;
    explicit Harness(std::string laneId):canvas(host),lane(std::move(laneId)),id(lane+".one")
    {
        host.store().changed=nullptr;host.store()=Store(makePanelFixture());
        bridge=std::make_unique<PanelFixtureHost>(host.store());CHECK(controller.adopt(bridge->snapshot()));
        host.onSplitClip=[this](const std::string& track,const std::string& clip,double time){
            auto split=bridge->splitClip(track,clip,time);CHECK(split.result);return split.rightClipId;
        };
        canvas.panelView.enabled=true;canvas.panelView.time={0,50,0};canvas.setLookAndFeel(&host.look());
        canvas.panelView.cycleHeight=[this]{++cycles;controller.cycleHeight(lane);bind(controller.height(lane));};
        canvas.panelView.prepareGlue=[this](const std::string& clip){
            const auto s=bridge->snapshot();const auto selected=selection;
            return [this,clip,session=s.session,revision=s.revision,selected]{
                const auto current=bridge->snapshot();if(current.session!=session||current.revision!=revision)return;
                std::vector<panel::EventRef> refs;
                auto result=panel::glueContainerTargets(current,lane,clip,selected,refs);CHECK(result);
                panel::Command cmd;cmd.session=session;cmd.revision=revision;
                cmd.kind=panel::CommandKind::glueClips;cmd.events=refs;
                const auto applied=bridge->apply(cmd);
                if(!applied)throw std::runtime_error(applied.message);
            };
        };
        canvas.panelView.openContainerEditor=[this](const std::string&,juce::Component&,juce::Rectangle<int>)
        {++containerOpens;};
        root.addAndMakeVisible(canvas);root.setSize(1100,450);
        root.addToDesktop(juce::ComponentPeer::windowIsTemporary);root.setVisible(true);
        setHeight(32);pump();
    }
    ~Harness(){canvas.groupGesture.cancel();host.onSplitClip=nullptr;canvas.setLookAndFeel(nullptr);root.removeFromDesktop();}
    void bind(int h){
        canvas.setSize(1100,h);
        canvas.panelView.clipSplits=projectClipSplits(host.store().state(),lane);
        const auto* track=findTrack(host.store().state(),lane);
        auto ids=std::make_shared<std::unordered_set<std::string>>();
        for(const auto& clip:track->clips){std::vector<std::string> nested;collectClipIds(clip,nested);ids->insert(nested.begin(),nested.end());}
        canvas.panelView.canonicalClipIds=std::move(ids);canvas.setModel(*track,h);
    }
    void setHeight(int h){controller.setHeight(lane,h);bind(controller.height(lane));}
    juce::Rectangle<float> body(){return canvas.clipBounds(*fixtureClip(host.store().state(),{lane,id}),50);}
    auto visual(){return CanvasContractAccess::visual(canvas,id,body());}
    juce::Component& target(){return canvas;}
    void click(juce::Point<float> p,int count=1,int mods=juce::ModifierKeys::leftButtonModifier){
        target().mouseDown(event(target(),p,count,mods));
        if(count>=2)target().mouseDoubleClick(event(target(),p,count,mods));
        target().mouseUp(event(target(),p,count,mods&~juce::ModifierKeys::leftButtonModifier));
    }
    void doubleHeader(){const auto b=body();const juce::Point<float> p{b.getCentreX(),b.getY()+4};
        CHECK(CanvasContractAccess::hit(canvas,p).zone==TimelineClipZone::clipTitleMove);
        click(p);click(p,2);
    }
};
template<class C>void canvasContracts(const std::string& lane,const std::string& label)
{
    std::cout << "Canvas " << label << std::endl;
    Harness<C> h(lane);
    const auto original=*findTrack(h.host.store().state(),lane);
    CHECK(h.body().getHeight()==18);CHECK(h.body().getY()==3);
    CHECK(h.body().getBottom()+7<=h.canvas.getHeight()-4);
    const auto rail=juce::Point<float>{h.body().getCentreX(),h.body().getBottom()+4};
    CHECK(CanvasContractAccess::hit(h.canvas,rail).zone==TimelineClipZone::bottomCutRail);
    for(int height:{32,48,63,64,72,95,96,256,400}) {
        h.setHeight(height);const auto b=h.body();auto v=h.visual();
        CHECK(v.features.showWarpRail);CHECK(v.features.showWarpControls==(height>=64));
        CHECK(v.features.showCutRail);
        if constexpr(!std::is_same_v<C,MidiClipCanvas>) {
            CHECK(v.features.showGainEnvelope!=v.features.showEnvelopePreview);
            if(height<64)CHECK(v.features.showEnvelopePreview);
        }
        // No invisible top warp marker at compact height.
        if(height<64)for(int x=int(b.getX())+4;x<int(b.getRight())-4;x+=13){
            const auto z=CanvasContractAccess::hit(h.canvas,{float(x),1.0f}).zone;
            CHECK(z!=TimelineClipZone::warpMarker&&z!=TimelineClipZone::topWarpRail);
        }
        if(height<96)for(float fraction:{0.10f,0.35f,0.60f,0.90f}){
            CHECK(CanvasContractAccess::hit(h.canvas,{b.getX()+1,b.getY()+fraction*b.getHeight()}).zone==TimelineClipZone::edgeLeftSlip);
            CHECK(CanvasContractAccess::hit(h.canvas,{b.getRight()-1,b.getY()+fraction*b.getHeight()}).zone==TimelineClipZone::edgeRightSlip);
        }
        if(height<64)for(int x=int(b.getX())+10;x<int(b.getRight())-10;x+=11)
            for(float y=b.getY()+2;y<b.getBottom();y+=3) {
                const auto z=CanvasContractAccess::hit(h.canvas,{float(x),y}).zone;
                CHECK(z!=TimelineClipZone::gainEnvelopePoint&&z!=TimelineClipZone::gainEnvelopeLine
                    &&z!=TimelineClipZone::gainSegmentCurveHandle&&z!=TimelineClipZone::fadeInCurveHandle
                    &&z!=TimelineClipZone::fadeOutCurveHandle);
            }
        if(height==32||height==64||height==400)save(h.canvas,juce::String(label)+"-"+juce::String(height));
    }
    h.setHeight(32);h.doubleHeader();
    if constexpr(std::is_same_v<C,MidiClipCanvas>){
        CHECK(h.canvas.getHeight()==256);CHECK(h.visual().zoomLod==clip_midi_law::ZoomLod::noteEdit);
        CHECK(computeMidiClipLayout(h.body(),h.visual().zoomLod).velocityLaneBounds.isEmpty());
        h.doubleHeader();CHECK(h.canvas.getHeight()==400);
        CHECK(h.visual().zoomLod==clip_midi_law::ZoomLod::noteAndVelocityEdit);
        CHECK(!computeMidiClipLayout(h.body(),h.visual().zoomLod).velocityLaneBounds.isEmpty());
        h.doubleHeader();CHECK(h.canvas.getHeight()==32);CHECK(h.cycles==3);
    } else {
        CHECK(h.canvas.getHeight()==400);h.doubleHeader();CHECK(h.canvas.getHeight()==32);CHECK(h.cycles==2);
    }
    CHECK(findTrack(h.host.store().state(),lane)->clips==original.clips);
    // Stationary Shift modifier changes the cursor; releasing it restores cut.
    const auto cut=juce::Point<float>{h.body().getCentreX(),h.body().getBottom()+4};
    h.target().mouseMove(event(h.target(),cut,1,0));
    h.target().modifierKeysChanged(juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
    CHECK(h.target().getMouseCursor()==timelineMouseCursor(TimelineCursorRole::glue));
    h.target().modifierKeysChanged(juce::ModifierKeys{});
    CHECK(h.target().getMouseCursor()==timelineMouseCursor(TimelineCursorRole::cut));
    // Compact right-edge Slip: opposite SOURCE end remains anchored.
    const auto oldClip=*fixtureClip(h.host.store().state(),{lane,h.id});
    const auto offset=CanvasContractAccess::offset(h.canvas,h.id);
    std::optional<LineClipUIState> oldLine;
    if constexpr(std::is_same_v<C,LineClipCanvas>)oldLine=CanvasContractAccess::lineState(h.canvas,h.id);
    auto p=juce::Point<float>{h.body().getRight()-1,h.body().getCentreY()};
    h.target().mouseDown(event(h.target(),p));
    h.target().mouseDrag(event(h.target(),p.translated(5,0),1,juce::ModifierKeys::leftButtonModifier,p,true));
    h.target().mouseUp(event(h.target(),p.translated(5,0),1,0,p,true));
    const auto* edited=fixtureClip(h.host.store().state(),{lane,h.id});
    CHECK(std::abs(edited->start-oldClip.start)<1e-8);CHECK(std::abs(edited->length-oldClip.length-0.1)<1e-8);
    if constexpr(std::is_same_v<C,LineClipCanvas>) {
        // Line clips retain their source window in normalized points rather
        // than the media source-offset field used by audio/MIDI/image.
        const auto now=CanvasContractAccess::lineState(h.canvas,h.id);
        for(float x:{0.2f,0.4f,0.6f,0.8f}) {
            const auto expected=evaluateAutomationCurveAt(oldLine->points.data(),int(oldLine->points.size()),
                oldLine->defaultNormValue,oldLine->boundary,float((x*edited->length-0.1)/oldClip.length));
            const auto actual=evaluateAutomationCurveAt(now.points.data(),int(now.points.size()),
                now.defaultNormValue,now.boundary,x);
            CHECK(std::abs(expected-actual)<0.001f);
        }
    } else CHECK(std::abs(CanvasContractAccess::offset(h.canvas,h.id)+edited->length-offset-oldClip.length)<1e-8);
    CHECK(h.host.store().undo());
}
void gestureArbitration()
{
    Harness<AudioClipCanvas> h("guitar.audio");
    constexpr int shift=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier;
    const auto p=juce::Point<float>{h.body().getCentreX(),h.body().getBottom()+4};
    const auto before=h.host.store().state();
    auto countMembers=[&]{const auto* track=findTrack(h.host.store().state(),h.lane);
        const auto it=std::find_if(track->clips.begin(),track->clips.end(),[](const auto& clip){return clip.isContainer();});
        return it==track->clips.end()?0:static_cast<int>(it->children.size());};
    h.click(p,1,shift);CHECK(!h.canvas.groupGesture.pending());CHECK(countMembers()==2);
    h.click(p,2,shift);CHECK(countMembers()==2);pump(80);CHECK(countMembers()==2);
    CHECK(h.host.store().undo());CHECK(h.host.store().state()==before);CHECK(!h.host.store().canUndo());
    h.selection={{h.lane,h.lane+".one"},{h.lane,h.lane+".three"}};
    h.click(p,1,shift);CHECK(countMembers()==2);
    const auto* grouped=findClip(*findTrack(h.host.store().state(),h.lane),h.host.store().state().selectedClip);
    CHECK(grouped&&std::any_of(grouped->children.begin(),grouped->children.end(),[&](const auto& child){return child.id==h.lane+".three";}));
    CHECK(h.host.store().undo());h.selection.clear();
    // Drag cancels rather than performing a hidden cut or group.
    h.target().mouseDown(event(h.target(),p,1,shift));
    h.target().mouseDrag(event(h.target(),p.translated(15,0),1,shift,p,true));
    h.target().mouseUp(event(h.target(),p.translated(15,0),1,0,p,true));pump(80);CHECK(countMembers()==0);
    // Escape and document revision changes cancel safely before mouse-up.
    h.target().mouseDown(event(h.target(),p,1,shift));
    CHECK(h.target().keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
    h.target().mouseUp(event(h.target(),p,1,juce::ModifierKeys::shiftModifier));CHECK(countMembers()==0);
    h.target().mouseDown(event(h.target(),p,1,shift));
    h.host.store().selectClip(h.lane+".two");
    h.target().mouseUp(event(h.target(),p,1,juce::ModifierKeys::shiftModifier));CHECK(countMembers()==0);
    CHECK(findTrack(h.host.store().state(),h.lane)->clips==findTrack(before,h.lane)->clips);
}
void linkedSplitIsAtomic()
{
    Harness<AudioClipCanvas> h("voice.audio");
    const auto before=h.host.store().state();
    const auto beforePoints=CanvasContractAccess::audioGainPoints(h.canvas,h.id);
    const auto beforeStateCount=CanvasContractAccess::stateCount(h.canvas);
    const auto b=h.body();
    h.click({b.getCentreX(),b.getBottom()+4.0f});

    for(const std::string lane:{"voice.audio","voice.midi","voice.image"}) {
        const auto* oldTrack=findTrack(before,lane);const auto* track=findTrack(h.host.store().state(),lane);
        CHECK(track->clips.size()==oldTrack->clips.size()+1);
        const auto* left=fixtureClip(h.host.store().state(),{lane,lane+".one"});
        const auto* right=fixtureClip(h.host.store().state(),{lane,lane+".one_cut"});
        CHECK(left&&right&&left->length==3.0&&right->start==5.0&&right->length==3.0);
    }
    CHECK(h.host.store().lastError().empty());
    CHECK(CanvasContractAccess::stateCount(h.canvas)==beforeStateCount+1);
    CHECK(!CanvasContractAccess::audioGainPoints(h.canvas,h.id).empty());
    CHECK(!CanvasContractAccess::audioGainPoints(h.canvas,h.id+"_cut").empty());
    CHECK(h.host.store().state().panelMetadata->relations.size()==before.panelMetadata->relations.size()+1);
    CHECK(h.host.store().undo());
    CHECK(h.host.store().state().tracks==before.tracks);
    CHECK(h.host.store().state().panelMetadata->relations==before.panelMetadata->relations);
    CHECK(!beforePoints.empty());
}
template<class C>void linkedPeerSplitReconciles(const std::string& lane,const std::string& originLane)
{
    Harness<C> h(lane);
    const auto stateCount=CanvasContractAccess::stateCount(h.canvas);
    const auto sourceOffset=CanvasContractAccess::offset(h.canvas,h.id);
    const auto split=h.bridge->splitClip(originLane,originLane+".one",5.0);
    CHECK(split.result);h.bind(h.canvas.getHeight());
    CHECK(CanvasContractAccess::stateCount(h.canvas)==stateCount+1);
    CHECK(CanvasContractAccess::offset(h.canvas,h.id+"_cut")>sourceOffset);
}
template<class C>void cutRailCommitsDocument(const std::string& lane,const std::string& clipId)
{
    Harness<C> h(lane);h.id=clipId;h.setHeight(300);
    const auto before=*fixtureClip(h.host.store().state(),{lane,clipId});
    const auto beforeCount=findTrack(h.host.store().state(),lane)->clips.size();
    const auto bounds=h.body();
    const juce::Point<float> cut{bounds.getCentreX(),bounds.getBottom()+4.0f};
    CHECK(CanvasContractAccess::hit(h.canvas,cut).zone==TimelineClipZone::bottomCutRail);
    h.click(cut);h.bind(300);
    const auto* left=fixtureClip(h.host.store().state(),{lane,clipId});
    CHECK(left&&left->length<before.length);
    CHECK(findTrack(h.host.store().state(),lane)->clips.size()>beforeCount);
}
template<class C>void containerFeedbackContract(const std::string& lane,const std::string& label)
{
    Harness<C> h(lane);
    h.id=lane+".two";
    h.setHeight(300);
    const auto stateCount=CanvasContractAccess::stateCount(h.canvas);
    const auto before=h.canvas.createComponentSnapshot(h.canvas.getLocalBounds(),true);
    const auto rail=juce::Point<float>{h.body().getCentreX(),h.body().getBottom()+4.0f};
    constexpr int shift=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier;
    const auto selectedBefore=h.host.store().state().selectedClip;
    h.click(rail,1,shift);
    CHECK(waitFor([&]{const auto& state=h.host.store().state();
        const auto* selected=findClip(state,lane,state.selectedClip);
        return state.selectedClip!=selectedBefore&&selected&&selected->isContainer();}));

    h.bind(300);const auto containerId=h.host.store().state().selectedClip;
    CHECK(CanvasContractAccess::stateCount(h.canvas)==stateCount+1);
    const auto* container=fixtureClip(h.host.store().state(),{lane,containerId});CHECK(container&&container->children.size()==2);
    h.id=containerId;
    if constexpr(std::is_same_v<C,MidiClipCanvas>) {
        CanvasContractAccess::forceMidiScrollbar(h.canvas,h.id);
        const auto forced=h.visual();CHECK(!forced.showPitchScrollbar);
        const auto scrollbar=computeMidiPitchScrollbarGeometry(
            forced.bounds,forced.zoomLod,forced.displayMinPitch,forced.displayMaxPitch,
            forced.pitchScrollbarOnLeft);
        if(!scrollbar.trackBounds.isEmpty())
            CHECK(CanvasContractAccess::hit(h.canvas,scrollbar.trackBounds.getCentre()).zone
                  !=TimelineClipZone::midiPitchScrollbar);
        h.target().mouseMove(event(h.target(),h.body().getCentre(),1,0));
        CHECK(!CanvasContractAccess::midiScrollbarVisible(h.canvas,h.id));
    }
    const auto visual=h.visual();
    CHECK(visual.container.isContainer());CHECK(visual.container.numDirectChildren==2);
    CHECK(visual.numContainedClipPreviews==2);
    CHECK(visual.containedClipPreviews!=nullptr);
    CHECK(visual.containedClipPreviews[0].bounds.getX()>visual.bounds.getX());
    CHECK(!visual.containedClipPreviews[0].features.showWarpControls);
    CHECK(!visual.containedClipPreviews[0].features.showGainEnvelope);
    CHECK(visual.features.showWarpControls);

    std::vector<juce::Rectangle<float>> beforeWarpBounds;
    beforeWarpBounds.reserve(static_cast<std::size_t>(visual.numContainedClipPreviews));
    for(int i=0;i<visual.numContainedClipPreviews;++i)
        beforeWarpBounds.push_back(visual.containedClipPreviews[i].bounds);
    const juce::Point<float> warpRail{
        visual.bounds.getCentreX(),visual.bounds.getY()*0.5f};
    CHECK(CanvasContractAccess::hit(h.canvas,warpRail).zone
          ==TimelineClipZone::topWarpRail);
    h.click(warpRail);
    const auto withMarker=h.visual();
    CHECK(withMarker.numWarpMarkers==visual.numWarpMarkers+1);
    int markerIndex=0;
    float markerDistance=2.0f;
    for(int i=0;i<withMarker.numWarpMarkers;++i) {
        const float distance=std::abs(withMarker.warpMarkersNorm[i]-0.5f);
        if(distance<markerDistance){markerDistance=distance;markerIndex=i;}
    }
    const juce::Point<float> warpMarker{
        withMarker.bounds.getX()+withMarker.warpMarkersNorm[markerIndex]*withMarker.bounds.getWidth(),
        withMarker.bounds.getY()*0.5f};
    CHECK(CanvasContractAccess::hit(h.canvas,warpMarker).zone
          ==TimelineClipZone::warpMarker);
    const auto movedWarpMarker=warpMarker.translated(withMarker.bounds.getWidth()*0.16f,0.0f);
    h.target().mouseDown(event(h.target(),warpMarker));
    h.target().mouseDrag(event(h.target(),movedWarpMarker,1,
        juce::ModifierKeys::leftButtonModifier,warpMarker,true));
    h.target().mouseUp(event(h.target(),movedWarpMarker,1,0,warpMarker,true));
    const auto warpedVisual=h.visual();
    bool childGeometryChanged=false;
    for(int i=0;i<warpedVisual.numContainedClipPreviews;++i)
        childGeometryChanged=childGeometryChanged
            || warpedVisual.containedClipPreviews[i].bounds!=beforeWarpBounds[static_cast<std::size_t>(i)];
    CHECK(childGeometryChanged);

    CHECK(warpedVisual.container.showGainEnvelope);
    const auto previewLevelBeforeGain=containedPreviewLevel(warpedVisual);
    const auto gainBounds=containerGainCurveBounds(warpedVisual.bounds);
    const juce::Point<float> unityGainLine{
        gainBounds.getCentreX(),
        gainBounds.getBottom()-clip_gain_law::dbToNorm(0.0f)*gainBounds.getHeight()};
    CHECK(CanvasContractAccess::hit(h.canvas,unityGainLine).zone
          ==TimelineClipZone::gainEnvelopeLine);
    h.click(unityGainLine,2);
    auto gainPoints=CanvasContractAccess::containerGainPoints(h.canvas,h.id);
    CHECK(gainPoints.size()==3);
    juce::Point<float> gainPoint{
        gainBounds.getX()+gainPoints[1].normX*gainBounds.getWidth(),
        gainBounds.getBottom()-clip_gain_law::dbToNorm(gainPoints[1].gainDb)
                                   *gainBounds.getHeight()};
    h.target().mouseDown(event(h.target(),gainPoint));
    const auto loweredGainPoint=gainPoint.translated(0.0f,18.0f);
    h.target().mouseDrag(event(h.target(),loweredGainPoint,1,
        juce::ModifierKeys::leftButtonModifier,gainPoint,true));
    h.target().mouseUp(event(h.target(),loweredGainPoint,1,0,gainPoint,true));
    gainPoints=CanvasContractAccess::containerGainPoints(h.canvas,h.id);
    CHECK(gainPoints[1].gainDb<0.0f);
    const auto gainedVisual=h.visual();
    CHECK(containedPreviewLevel(gainedVisual)<previewLevelBeforeGain);
    gainPoint={gainBounds.getX()+gainPoints[1].normX*gainBounds.getWidth(),
        gainBounds.getBottom()-clip_gain_law::dbToNorm(gainPoints[1].gainDb)
                                   *gainBounds.getHeight()};
    h.click(gainPoint,1,juce::ModifierKeys::leftButtonModifier
                       |juce::ModifierKeys::altModifier);
    CHECK(CanvasContractAccess::containerGainPoints(h.canvas,h.id).size()==2);

    const auto after=h.canvas.createComponentSnapshot(h.canvas.getLocalBounds(),true);

    int changedPixels=0;
    for(int y=30;y<220;++y)
        for(int x=110;x<760;++x)
            if(before.getPixelAt(x,y)!=after.getPixelAt(x,y))++changedPixels;
    CHECK(changedPixels>200);
    bool opened=false;const auto bounds=h.body();
    for(int y=static_cast<int>(bounds.getY()+18);y<static_cast<int>(bounds.getBottom())&&!opened;y+=7)
        for(int x=static_cast<int>(bounds.getX()+12);x<static_cast<int>(bounds.getRight()-12)&&!opened;x+=9)
        {
            const auto hit=CanvasContractAccess::hit(h.canvas,{static_cast<float>(x),static_cast<float>(y)});
            if(hit.zone==TimelineClipZone::none||hit.zone==TimelineClipZone::clipBody)
            {h.click({static_cast<float>(x),static_cast<float>(y)},2);opened=true;}
        }
    CHECK(opened&&h.containerOpens==1);
    save(h.canvas,"glue-"+juce::String(label));
    const auto opens=h.containerOpens;h.doubleHeader();
    CHECK(h.cycles==1&&h.containerOpens==opens);
}
void nestedContainerPreviewIsRecursive()
{
    Harness<AudioClipCanvas> h("guitar.audio");
    h.id=h.lane+".two";h.setHeight(300);
    const auto rail=juce::Point<float>{h.body().getCentreX(),h.body().getBottom()+4.0f};
    constexpr int shift=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier;
    h.click(rail,1,shift);
    CHECK(waitFor([&]{const auto* clip=findClip(*findTrack(h.host.store().state(),h.lane),
                                                 h.host.store().state().selectedClip);
        return clip&&clip->isContainer();}));
    const auto inner=h.host.store().state().selectedClip;
    const auto snapshot=h.bridge->snapshot();
    panel::Command command;command.kind=panel::CommandKind::glueClips;
    command.session=snapshot.session;command.revision=snapshot.revision;
    command.events={{h.lane,h.lane+".one"},{h.lane,inner}};
    CHECK(h.bridge->apply(command));h.bind(300);
    h.id=h.host.store().state().selectedClip;
    const auto visual=h.visual();
    CHECK(visual.container.isContainer());
    CHECK(visual.container.numDirectChildren==2);
    CHECK(visual.numContainedClipPreviews==4);
    CHECK(std::count_if(visual.containedClipPreviews,
                        visual.containedClipPreviews+visual.numContainedClipPreviews,
                        [](const auto& child){return child.container.previewDepth==2;})==2);
    CHECK(std::all_of(visual.containedClipPreviews,
                      visual.containedClipPreviews+visual.numContainedClipPreviews,
                      [](const auto& child){return !child.features.showWarpControls
                                                  &&!child.features.showGainEnvelope;}));
}
void curvePaintContracts()
{
    AutomationPoint points[]{{.2f,.5f,.5f},{.8f,.5f,.5f}};
    for(bool preview:{false,true}) {
        juce::Image image(juce::Image::ARGB,330,110,true);juce::Graphics g(image);
        AutomationCurveParams p;p.bounds={10,10,300,80};p.points=points;p.numPoints=2;
        p.boundaryPolicy=AutomationBoundaryPolicy::fadeEndpoints;p.previewOnly=preview;
        ClipDefaultLookAndFeel::getDefaultInstance().drawAutomationCurve(g,p);
        ayra::curves::CurveBoundarySpec spec;spec.mode=p.boundaryPolicy;
        const auto geometry=computeAutomationBoundaryGeometry(p.bounds,points,2,spec);
        CHECK(geometry.hasFadeIn&&geometry.hasFadeOut);
        for(const auto segment:{geometry.fadeIn,geometry.fadeOut}) {
            int lit=0,dim=0;
            for(int n=12;n<88;++n) {
                const auto q=segment.start+(segment.end-segment.start)*(n/100.0f);
                int light=0;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                    const auto pixel=image.getPixelAt(juce::roundToInt(q.x)+dx,juce::roundToInt(q.y)+dy);
                    light=std::max(light,int(pixel.getRed())*int(pixel.getAlpha())/255);
                }
                if(light>100)++lit;else ++dim;
            }
            CHECK(lit>10);CHECK(dim>3); // actual holes survive: not a continuous over-stroke
        }
        if(preview) {
            // Neither a filled underlay nor interactive point handles leak into the preview.
            CHECK(image.getPixelAt(160,75).getAlpha()==0);
            CHECK(image.getPixelAt(160,48).getAlpha()==0);
        }
        const auto file=output.getChildFile(preview?"curve-preview.png":"curve-edit.png");file.deleteFile();
        juce::FileOutputStream stream(file);CHECK(stream.openedOk());CHECK(juce::PNGImageFormat{}.writeImageToStream(image,stream));
    }
}

void realDemoWiring()
{
    TimelinePanelDemo demo;demo.setSize(1480,1050);demo.addToDesktop(juce::ComponentPeer::windowIsTemporary);demo.setVisible(true);pump();
    auto& c=CanvasContractAccess::controller(demo);auto& surface=CanvasContractAccess::surface(demo);auto& store=CanvasContractAccess::store(demo);
    for(const auto& lane:c.snapshot().lanes)if(lane.heightPolicy==panel::HeightPolicy::manual)c.setHeight(lane.id,32);
    surface.refresh();pump();save(demo,"compact-all");
    if(c.view().mode!=panel::Mode::freeLanes)CHECK(c.setMode(panel::Mode::freeLanes));
    CHECK(c.select("guitar.audio",panel::Controller::Selection::replace));
    CHECK(c.select("guitar.midi",panel::Controller::Selection::toggle));CHECK(c.groupLayout("visual-pair"));
    const auto* groupFirst=c.layoutGroupForRow("lane:guitar.audio");
    const auto* groupLast=c.layoutGroupForRow("lane:guitar.midi");
    CHECK(groupFirst&&groupFirst->joinsNext&&!groupFirst->joinsPrevious);
    CHECK(groupLast&&groupLast->joinsPrevious&&!groupLast->joinsNext);
    surface.revealLane("guitar.audio");pump();save(demo,"lane-layout-group");
    CHECK(c.ungroupLayout());
    if(c.view().mode!=panel::Mode::structured)CHECK(c.setMode(panel::Mode::structured));
    surface.refresh();pump();
    auto chrome=c.view().chrome;chrome.nameSide=panel::presentation::TimelineNameSide::left;surface.setChrome(chrome);pump();save(demo,"compact-card-left");
    surface.revealLane("guitar.audio");pump();
    auto* canvas=dynamic_cast<AudioClipCanvas*>(find(demo,"canvas:guitar.audio"));CHECK(canvas);
    const auto* clip=fixtureClip(store.state(),{"guitar.audio","guitar.audio.one"});CHECK(clip);
    const auto b=canvas->clipBounds(*clip,canvas->panelView.time.pixelsPerSecond);
    juce::Component& target=*canvas;const juce::Point<float> p{b.getCentreX(),b.getBottom()+4};
    constexpr int shift=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier;
    const auto previous=store.state();
    CanvasContractAccess::selection(demo)={{"guitar.audio","guitar.audio.one"},
                                           {"guitar.audio","guitar.audio.two"},
                                           {"guitar.audio","guitar.audio.three"}};
    target.mouseDown(event(target,p,1,shift));target.mouseUp(event(target,p,1,juce::ModifierKeys::shiftModifier));pump();
    const auto* gluedTrack=findTrack(store.state(),"guitar.audio");
    CHECK(gluedTrack->clips.size()==1&&gluedTrack->clips.front().children.size()==3);
    c.setHeight("guitar.audio",300);surface.refresh();pump();
    canvas=dynamic_cast<AudioClipCanvas*>(find(demo,"canvas:guitar.audio"));CHECK(canvas);
    const auto gluedBounds=canvas->clipBounds(gluedTrack->clips.front(),canvas->panelView.time.pixelsPerSecond);
    bool opened=false;
    for(int y=static_cast<int>(gluedBounds.getY()+18);y<static_cast<int>(gluedBounds.getBottom())&&!opened;y+=7)
        for(int x=static_cast<int>(gluedBounds.getX()+12);x<static_cast<int>(gluedBounds.getRight()-12)&&!opened;x+=9)
        {
            const auto hit=CanvasContractAccess::hit(*canvas,{static_cast<float>(x),static_cast<float>(y)});
            if(hit.zone==TimelineClipZone::none||hit.zone==TimelineClipZone::clipBody)
            {juce::Component& target=*canvas;target.mouseDoubleClick(event(target,{static_cast<float>(x),static_cast<float>(y)},2));opened=true;}
        }
    CHECK(opened&&waitFor([&]{return CanvasContractAccess::hasContainerEditor(demo);}));
    CHECK(store.undo());CHECK(store.state()==previous);pump();
    CHECK(waitFor([&]{return !CanvasContractAccess::hasContainerEditor(demo);}));
    CHECK(c.setHeight("guitar.audio",32));surface.refresh();pump();
    // The same actual lane cycle is reached by its accent, including MIDI/continuous.
    for(const std::string lane:{"guitar.audio","voice.midi","tempo"}) {
        surface.revealLane(lane);pump();auto* row=find(demo,"lane:"+juce::String(lane));CHECK(row);
        const auto* laneModel=c.lane(lane);CHECK(laneModel);
        const auto geometry=panel::presentation::layoutTimelineLaneChrome(
            row->getWidth(),row->getHeight(),c.chromeCapabilities(*laneModel),c.view().chrome);
        const auto accent=!geometry.leftAccent.empty()?geometry.leftAccent:geometry.rightAccent;
        CHECK(!accent.empty());const juce::Point<float> accentPoint{
            static_cast<float>(accent.x+accent.width/2),static_cast<float>(accent.y+accent.height/2)};
        row->mouseDoubleClick(event(*row,accentPoint,2));
        CHECK(c.height(lane)==(lane=="voice.midi"?256:400));
        row=find(demo,"lane:"+juce::String(lane));CHECK(row);row->mouseDoubleClick(event(*row,accentPoint,2));
        if(lane=="voice.midi") {CHECK(c.height(lane)==400);row=find(demo,"lane:"+juce::String(lane));row->mouseDoubleClick(event(*row,accentPoint,2));}
        CHECK(c.height(lane)==32);
    }
    // Compact continuous lanes are previews, including double-click on blank graph.
    c.setHeight("tempo",32);surface.revealLane("tempo");surface.refresh();pump();
    auto* tempo=dynamic_cast<ContinuousLineCanvas*>(find(demo,"canvas:tempo"));CHECK(tempo);
    auto beforePoints=std::static_pointer_cast<std::vector<MidiEnvelopePoint>>(tempo->capturePanelState());
    juce::Component& tempoTarget=*tempo;
    tempoTarget.mouseDoubleClick(event(tempoTarget,{450,20},2));
    CHECK(std::static_pointer_cast<std::vector<MidiEnvelopePoint>>(tempo->capturePanelState())->size()==beforePoints->size());
    // Passive titles disappear immediately even though the compact curve is not editable.
    for(bool left:{false,true}) {
        auto show=c.view().chrome;show.nameSide=left?panel::presentation::TimelineNameSide::left:panel::presentation::TimelineNameSide::right;
        surface.setChrome(show);pump();
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(demo.localPointToGlobal(juce::Point<int>{5,5}).toFloat());pump();
        const auto layout=surface.getIdentityLayout("tempo");CHECK(layout);
        auto* row=find(demo,"lane:tempo");CHECK(row);
        const auto pos=demo.getLocalPoint(row,layout->titleHit.getCentre()).toFloat();
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(demo.localPointToGlobal(pos));
        demo.getPeer()->handleMouseEvent(juce::MouseInputSource::mouse,pos,{},0,0,juce::Time::currentTimeMillis());
        CHECK(surface.isIdentitySuppressed("tempo"));CHECK(!surface.hasPendingIdentityTransition());
        pump();
    }
    demo.removeFromDesktop();
}
}
int main(int argc,char** argv)
{
    try {
        juce::ScopedJuceInitialiser_GUI init;
        output=juce::File(argc>1?argv[1]:"/tmp/ayra-canvas-evidence");output.createDirectory();
        const int previousTimeout=juce::MouseEvent::getDoubleClickTimeout();juce::MouseEvent::setDoubleClickTimeout(40);
        canvasContracts<AudioClipCanvas>("guitar.audio","audio");
        canvasContracts<MidiClipCanvas>("guitar.midi","midi");
        canvasContracts<LineClipCanvas>("keys.automation","line");
        canvasContracts<ImageClipCanvas>("voice.image","image");
        gestureArbitration();linkedSplitIsAtomic();
        cutRailCommitsDocument<AudioClipCanvas>("guitar.audio","guitar.audio.one");
        cutRailCommitsDocument<MidiClipCanvas>("guitar.midi","guitar.midi.one");
        cutRailCommitsDocument<LineClipCanvas>("keys.automation","keys.automation.one");
        cutRailCommitsDocument<ImageClipCanvas>("voice.image","voice.image.two");
        linkedPeerSplitReconciles<AudioClipCanvas>("voice.audio","voice.midi");
        linkedPeerSplitReconciles<MidiClipCanvas>("voice.midi","voice.audio");
        linkedPeerSplitReconciles<ImageClipCanvas>("voice.image","voice.audio");
        containerFeedbackContract<AudioClipCanvas>("guitar.audio","audio");
        containerFeedbackContract<MidiClipCanvas>("guitar.midi","midi");
        containerFeedbackContract<LineClipCanvas>("keys.automation","line");
        containerFeedbackContract<ImageClipCanvas>("voice.image","image");
        curvePaintContracts();realDemoWiring();
        nestedContainerPreviewIsRecursive();
        juce::MouseEvent::setDoubleClickTimeout(previousTimeout);
        ayra::shadowCache().clear();juce::ImageCache::releaseUnusedImages();
        std::cout<<"PASS "<<checks<<" real canvas geometry, input, grouping, source-anchor and rendering contracts\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
