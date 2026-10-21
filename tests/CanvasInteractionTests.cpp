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
void save(juce::Component& component,const juce::String& name)
{
    auto image=component.createComponentSnapshot(component.getLocalBounds(),true,2.0f);
    const auto file=output.getChildFile(name+".png");file.deleteFile();juce::FileOutputStream stream(file);
    CHECK(stream.openedOk());CHECK(juce::PNGImageFormat{}.writeImageToStream(image,stream));
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
    int cycles=0;
    std::vector<panel::EventRef> selection;
    explicit Harness(std::string laneId):canvas(host),lane(std::move(laneId)),id(lane+".one")
    {
        host.store().changed=nullptr;host.store()=Store(makePanelFixture());
        bridge=std::make_unique<PanelFixtureHost>(host.store());CHECK(controller.adopt(bridge->snapshot()));
        canvas.panelView.enabled=true;canvas.panelView.time={0,50,0};canvas.setLookAndFeel(&host.look());
        canvas.panelView.cycleHeight=[this]{++cycles;controller.cycleHeight(lane);bind(controller.height(lane));};
        canvas.panelView.prepareTemporalGroup=[this](const std::string& clip){
            const auto s=bridge->snapshot();const auto selected=selection;
            return [this,clip,session=s.session,revision=s.revision,selected](bool all){
                const auto current=bridge->snapshot();if(current.session!=session||current.revision!=revision)return;
                std::vector<panel::EventRef> refs;
                auto result=panel::temporalGroupTargets(current,lane,clip,selected,all,refs);CHECK(result);
                panel::Command cmd;cmd.session=session;cmd.revision=revision;
                cmd.kind=panel::CommandKind::temporalGroup;cmd.events=refs;CHECK(bridge->apply(cmd));
            };
        };
        root.addAndMakeVisible(canvas);root.setSize(1100,450);
        root.addToDesktop(juce::ComponentPeer::windowIsTemporary);root.setVisible(true);
        setHeight(32);pump();
    }
    ~Harness(){canvas.groupGesture.cancel();canvas.setLookAndFeel(nullptr);root.removeFromDesktop();}
    void bind(int h){canvas.setSize(1100,h);canvas.setModel(*findTrack(host.store().state(),lane),h);}
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
    h.click(p,1,shift);CHECK(h.canvas.groupGesture.pending());CHECK(h.host.store().state()==before);
    h.click(p,2,shift);CHECK(!h.canvas.groupGesture.pending());
    auto countMembers=[&]{const auto& rels=h.host.store().state().panelMetadata->relations;
        const auto it=std::find_if(rels.begin(),rels.end(),[](const auto& rel){return rel.kind==panel::RelationKind::temporalGroup&&rel.members[0].lane=="guitar.audio";});
        return it==rels.end()?0:static_cast<int>(it->members.size());};
    CHECK(countMembers()==3);pump(80);CHECK(countMembers()==3);
    CHECK(findTrack(h.host.store().state(),h.lane)->clips.size()==3);CHECK(h.host.store().undo());CHECK(h.host.store().state()==before);CHECK(!h.host.store().canUndo());
    h.click(p,1,shift);pump(80);CHECK(countMembers()==2);CHECK(h.host.store().undo());CHECK(!h.host.store().canUndo());
    h.selection={{h.lane,h.lane+".one"},{h.lane,h.lane+".three"}};
    h.click(p,1,shift);pump(80);CHECK(countMembers()==2);
    const auto& rel=h.host.store().state().panelMetadata->relations.back();
    CHECK(std::find(rel.members.begin(),rel.members.end(),panel::EventRef{h.lane,h.lane+".three"})!=rel.members.end());
    CHECK(h.host.store().undo());h.selection.clear();
    // Drag cancels rather than performing a hidden cut or delayed group.
    h.target().mouseDown(event(h.target(),p,1,shift));
    h.target().mouseDrag(event(h.target(),p.translated(15,0),1,shift,p,true));
    h.target().mouseUp(event(h.target(),p.translated(15,0),1,0,p,true));pump(80);CHECK(countMembers()==0);
    // Escape and document revision changes cancel safely.
    h.click(p,1,shift);CHECK(h.target().keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));pump(80);CHECK(countMembers()==0);
    h.click(p,1,shift);h.host.store().selectClip(h.lane+".two");pump(80);CHECK(countMembers()==0);
    CHECK(findTrack(h.host.store().state(),h.lane)->clips==findTrack(before,h.lane)->clips);
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

// Compare the actual MIDI painter with/without notes. A colourful background
// cannot satisfy this test; every extreme-pitch note must contribute pixels.
void midiPreviewPaintContracts()
{
    Harness<MidiClipCanvas> h("guitar.midi");
    MidiClipComponent component("midi-overview-probe",h.canvas);
    const std::array<MidiNoteVisual,4> notes{{
        {0,.05f,.18f,0,1,1}, {1,.28f,.45f,42,40,1},
        {2,.55f,.72f,84,100,1}, {3,.82f,.95f,127,127,1}}};
    for(bool drum:{false,true}) for(int laneHeight:{32,48,64,96}) for(float scale:{1.0f,2.0f}) {
        h.setHeight(laneHeight);auto v=h.visual();
        v.bounds=v.bounds.withX(4).withWidth(160);
        v.notes=notes.data();v.numNotes=int(notes.size());v.displayMinPitch=0;v.displayMaxPitch=127;
        v.isDrumMode=drum;v.features.showWarpRail=false;v.features.showWarpControls=false;
        v.title="A long clip name must not erase the compact notes";
        v.overlap={};
        const auto paint=[&](bool includeNotes){
            juce::Image image(juce::Image::ARGB,int(170*scale),int((laneHeight+10)*scale),true);
            juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(scale));
            auto view=v;if(!includeNotes)view.numNotes=0;
            ClipDefaultLookAndFeel::getDefaultInstance().drawMidiClip(g,view,component);
            return image;
        };
        const auto withNotes=paint(true),empty=paint(false);
        const auto piano=computeMidiClipLayout(v.bounds,v.zoomLod).pianoRollBounds;
        if(laneHeight==32)CHECK(piano.getHeight()==18);
        for(const auto& note:notes){
            const auto geom=computeMidiNoteGeometry(note,v.bounds,0,127,drum,v.zoomLod);
            CHECK(!geom.bounds.isEmpty());CHECK(piano.contains(geom.bounds));
            const auto pixels=(geom.bounds*scale).getSmallestIntegerContainer();
            int changed=0;
            for(int y=pixels.getY();y<pixels.getBottom();++y)
                for(int x=pixels.getX();x<pixels.getRight();++x)
                    changed+=withNotes.getPixelAt(x,y)!=empty.getPixelAt(x,y);
            CHECK(changed>=2);
        }
        if(laneHeight==32&&scale==2){
            const auto file=output.getChildFile(drum?"midi-compact-drums.png":"midi-compact-notes.png");
            file.deleteFile();juce::FileOutputStream stream(file);CHECK(stream.openedOk());
            CHECK(juce::PNGImageFormat{}.writeImageToStream(withNotes,stream));
        }
    }
    // Preview remains read-only even though its notes are now actually visible.
    h.setHeight(32);
    const auto v=h.visual();
    for(int i=0;i<v.numNotes;++i){
        const auto geom=computeMidiNoteGeometry(v.notes[i],v.bounds,v.displayMinPitch,v.displayMaxPitch,v.isDrumMode,v.zoomLod);
        if(geom.bounds.isEmpty())continue;
        const auto zone=CanvasContractAccess::hit(h.canvas,geom.centre).zone;
        CHECK(zone!=TimelineClipZone::midiNoteBody&&zone!=TimelineClipZone::midiNoteLeftEdge&&zone!=TimelineClipZone::midiNoteRightEdge);
    }
}

void realDemoWiring()
{
    TimelinePanelDemo demo;demo.setSize(1480,1050);demo.addToDesktop(juce::ComponentPeer::windowIsTemporary);demo.setVisible(true);pump();
    auto& c=CanvasContractAccess::controller(demo);auto& surface=CanvasContractAccess::surface(demo);auto& store=CanvasContractAccess::store(demo);
    for(const auto& lane:c.snapshot().lanes)if(lane.heightPolicy==panel::HeightPolicy::manual)c.setHeight(lane.id,32);
    surface.refresh();pump();save(demo,"compact-all");
    auto chrome=c.view().chrome;chrome.nameSide=panel::presentation::TimelineNameSide::left;surface.setChrome(chrome);pump();save(demo,"compact-card-left");
    surface.revealLane("guitar.audio");pump();
    auto* canvas=dynamic_cast<AudioClipCanvas*>(find(demo,"canvas:guitar.audio"));CHECK(canvas);
    const auto* clip=fixtureClip(store.state(),{"guitar.audio","guitar.audio.one"});CHECK(clip);
    const auto b=canvas->clipBounds(*clip,canvas->panelView.time.pixelsPerSecond);
    juce::Component& target=*canvas;const juce::Point<float> p{b.getCentreX(),b.getBottom()+4};
    constexpr int shift=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier;
    const auto previous=store.state();
    target.mouseDown(event(target,p,1,shift));target.mouseUp(event(target,p,1,juce::ModifierKeys::shiftModifier));
    target.mouseDown(event(target,p,2,shift));target.mouseDoubleClick(event(target,p,2,shift));target.mouseUp(event(target,p,2,juce::ModifierKeys::shiftModifier));pump(80);
    CHECK(findTrack(store.state(),"guitar.audio")->clips.size()==3);
    const auto& groups=store.state().panelMetadata->relations;
    CHECK(std::any_of(groups.begin(),groups.end(),[](const auto& rel){return rel.kind==panel::RelationKind::temporalGroup&&rel.members.size()==3&&rel.members[0].lane=="guitar.audio";}));
    CHECK(store.undo());CHECK(store.state()==previous);pump();
    // The same actual lane cycle is reached by its accent, including MIDI/continuous.
    for(const std::string lane:{"guitar.audio","voice.midi","tempo"}) {
        surface.revealLane(lane);pump();auto* row=find(demo,"lane:"+juce::String(lane));CHECK(row);
        row->mouseDoubleClick(event(*row,{1,6},2));
        CHECK(c.height(lane)==(lane=="voice.midi"?256:400));
        row=find(demo,"lane:"+juce::String(lane));CHECK(row);row->mouseDoubleClick(event(*row,{1,6},2));
        if(lane=="voice.midi") {CHECK(c.height(lane)==400);row=find(demo,"lane:"+juce::String(lane));row->mouseDoubleClick(event(*row,{1,6},2));}
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
        demo.getPeer()->handleMouseEvent(juce::MouseInputSource::mouse,pos,{},0,0,juce::Time::currentTimeMillis());pump();
        CHECK(surface.isIdentitySuppressed("tempo"));CHECK(!surface.hasPendingIdentityTransition());
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
        gestureArbitration();curvePaintContracts();midiPreviewPaintContracts();realDemoWiring();
        juce::MouseEvent::setDoubleClickTimeout(previousTimeout);
        ayra::shadowCache().clear();juce::ImageCache::releaseUnusedImages();
        std::cout<<"PASS "<<checks<<" real canvas geometry, input, grouping, source-anchor and rendering contracts\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
