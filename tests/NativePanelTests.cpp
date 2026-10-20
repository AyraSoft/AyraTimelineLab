/** Native component-level contracts. Runs the actual TimelinePanel with a small
    instrumented CanvasFactory; no media/DSP or fabricated application timings. */
#include "../modules/ayra_timeline_panel/TimelinePanel.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <chrono>
using namespace ayra::timeline_panel;
namespace {
int checks=0;
void check(bool value,const char* expression,int line)
{++checks;if(!value){std::cerr<<"FAIL "<<line<<" "<<expression<<'\n';std::exit(1);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
struct Record {int down=0,drag=0,up=0,doubled=0,wheel=0,binds=0,saved=0;CanvasFrame frame;
    juce::Rectangle<float> clip;};
class MockCanvas final:public juce::Component
{
public:
    explicit MockCanvas(Record& r):record(r){}
    void mouseDown(const juce::MouseEvent&)override{++record.down;}
    void mouseDrag(const juce::MouseEvent&)override{++record.drag;}
    void mouseUp(const juce::MouseEvent&)override{++record.up;}
    void mouseDoubleClick(const juce::MouseEvent&)override{++record.doubled;}
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override{++record.wheel;}
    Record& record;
};
class TestArtwork final:public TimelinePanelLookAndFeel
{
public:
    mutable int identitiesPrepared=0,underlaysPrepared=0,groupsPainted=0;mutable TimelineIdentityContent identity;
    juce::Image prepareIdentity(const TimelineIdentityContent& c,const TimelineIdentityLayout&)const override
    {++identitiesPrepared;identity=c;return {};}
    int identityBackdropPadding()const noexcept override{return 15;}
    juce::Image prepareIdentityUnderlay(const juce::Image& image,float)const override
    {++underlaysPrepared;return image;}
    std::unique_ptr<juce::Button> makeButton(LaneButton)override{return std::make_unique<juce::TextButton>();}
    void drawControlGroup(juce::Graphics&,juce::Rectangle<float>)const override{++groupsPainted;}
    void drawAccent(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c)const override{g.setColour(c);g.fillRect(r);}
    void drawMeter(juce::Graphics&,juce::Rectangle<float>,float)const override{}
};
juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* c:root.getChildren())if(auto* result=find(*c,id))return result;
    return nullptr;
}
juce::MouseEvent event(juce::Component& target,juce::Point<float> position,int clicks=1,
                       juce::ModifierKeys modifiers=juce::ModifierKeys::leftButtonModifier,
                       juce::Point<float> start={-1,-1},bool drag=false)
{
    if(start.x<0)start=position;
    return {juce::Desktop::getInstance().getMainMouseSource(),position,modifiers,
            1.0f,0.0f,0.0f,0.0f,0.0f,&target,&target,juce::Time::getCurrentTime(),
            start,juce::Time::getCurrentTime(),clicks,drag};
}
void pump(){juce::MessageManager::getInstance()->runDispatchLoopUntil(10);}
Snapshot fixture(int count)
{
    Snapshot s;s.session="native-test";s.revision=1;
    s.owners={{"performer",{},"Ayra",0xff9988cc,OwnerRole::performer},{"owner","performer","Shared Tensor"},{"global",{},"Globals",0xff888888,OwnerRole::global,false,false,Monitoring::automatic,{false,false,false,false}}};
    Lane global{"global-lane","global",{},"Metrica",Stream::global,VisualKind::point,24,16,24,true};
    global.heightPolicy=HeightPolicy::compact;s.lanes.push_back(global);
    for(int i=0;i<count;++i){Lane lane{std::to_string(i),"owner",{},"Lane "+std::to_string(i),i%2?Stream::midi:Stream::audio,VisualKind::audio,96,32,1200};
        lane.inputMeterCount=2;lane.outputMeterCount=2;s.lanes.push_back(lane);
        s.events.push_back({{lane.id,"clip"+lane.id},static_cast<double>(i),static_cast<double>(i)+2});}
    return s;
}

void cardStatusContracts()
{
    Controller c;auto s=fixture(4);
    s.lanes[2].visual=VisualKind::continuous;
    s.owners[1].muted=true;s.owners[1].soloed=true;
    s.owners[1].status={true,true,true,true,true,true,true};
    // Explicit source policy is independent of the effective monitoring bit.
    s.owners[1].monitoring=Monitoring::automatic;
    CHECK(c.adopt(s));CHECK(c.setMode(Mode::freeLanes));
    std::map<Id,Record> records;TestArtwork artwork;int commands=0,seeks=0;
    c.onCommand=[&](const Command&){++commands;return Result{};};
    TimelinePanel p(c,[&](const Lane& lane){
        LaneCanvas result;auto& r=records[lane.id];result.component=std::make_unique<MockCanvas>(r);
        result.bind=[&r](const Lane&,const CanvasFrame& frame){r.frame=frame;};
        // Exercise the new generic editing query, including a continuous lane.
        result.containsEditableAt=[&r](juce::Point<float> q){return r.clip.contains(q);};
        return result;
    },artwork);
    p.onSeek=[&](double){++seeks;};
    p.setSize(900,650);p.addToDesktop(juce::ComponentPeer::windowIsTemporary);p.setVisible(true);p.refresh();pump();
    CHECK(c.height("global-lane")==24);CHECK(artwork.groupsPainted==0);
    for(int i=0;i<6;++i){auto* button=find(p,"button:"+juce::String(i)+":0");CHECK(button&&!button->isVisible());}
    CHECK(!p.hasPendingIdentityTransition());
    auto a=p.getIdentityReadout("0"),b=p.getIdentityReadout("1");CHECK(a&&b);
    CHECK(a->status==b->status&&a->status.monitoringActive&&a->muted&&a->soloed);
    auto* next=dynamic_cast<juce::Button*>(find(p,"card-nav:next:0"));
    auto* prev=dynamic_cast<juce::Button*>(find(p,"card-nav:previous:0"));
    CHECK(next&&prev&&next->isShowing()&&next->getWidth()==16&&next->getHeight()==18);
    // Lane 0 has one event starting at 0: no next event at playhead zero.
    CHECK(!next->isEnabled());p.setPlayhead(1);CHECK(prev->isEnabled());
    prev->triggerClick();pump();CHECK(seeks==1&&p.getPlayhead()==0&&commands==0);
    auto* canvas=find(p,"canvas:1");CHECK(canvas);
    auto lens=p.getIdentityBounds("1");CHECK(!lens.isEmpty());
    auto point=juce::Point<int>{lens.getX()+28,lens.getY()+8};auto q=canvas->getLocalPoint(&p,point).toFloat();
    records["1"].clip={q.x-12,q.y-10,24,20};
    auto pointer=[&](juce::Point<int> pos){
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p.localPointToGlobal(pos).toFloat());
        p.getPeer()->handleMouseEvent(juce::MouseInputSource::mouse,pos.toFloat(),{},0,0,juce::Time::currentTimeMillis());pump();
    };
    auto wait=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    pointer(point);CHECK(!p.isIdentitySuppressed("1"));CHECK(p.hasPendingIdentityTransition());wait(100);
    auto* arrow=find(p,"card-nav:next:1");CHECK(arrow);
    auto arrowPoint=p.getLocalPoint(arrow,arrow->getLocalBounds().getCentre());
    pointer(arrowPoint);CHECK(!p.hasPendingIdentityTransition());wait(550);CHECK(!p.isIdentitySuppressed("1"));
    pointer(point);CHECK(p.hasPendingIdentityTransition());wait(100);CHECK(!p.isIdentitySuppressed("1"));wait(450);
    CHECK(p.isIdentitySuppressed("1"));CHECK(!arrow->isShowing());CHECK(!p.hasPendingIdentityTransition());
    CHECK(p.getComponentAt(arrowPoint)==canvas); // no invisible navigation targets
    pointer({lens.getX()+2,lens.getY()+2});CHECK(p.isIdentitySuppressed("1"));
    pointer({lens.getX()-3,lens.getCentreY()});CHECK(!p.isIdentitySuppressed("1"));CHECK(arrow->isShowing());
    // Repaint/normal pointer motion cannot manufacture new domain commands.
    canvas->mouseDoubleClick(event(*canvas,q,2));CHECK(records["1"].doubled==1);CHECK(commands==0);
    // Continuous lanes: manual resize yes, double-click height cycle no.
    auto* resize=find(p,"resize:1");CHECK(resize&&resize->isVisible());
    const auto old=c.height("1");resize->mouseDoubleClick(event(*resize,{3,1},2));CHECK(c.height("1")==old);
    resize->mouseDown(event(*resize,{3,1}));resize->mouseDrag(event(*resize,{3,21},1,juce::ModifierKeys::leftButtonModifier,{3,1},true));
    resize->mouseUp(event(*resize,{3,21}));CHECK(c.height("1")==old+20);CHECK(c.undoView());p.refresh();
    // State changes propagate to siblings; no inferred active monitor in Auto.
    s=c.snapshot();s.owners[1].status={};s.owners[1].muted=false;s.owners[1].soloed=false;++s.revision;CHECK(c.adopt(s));p.refresh();
    CHECK(!p.getIdentityReadout("0")->status.monitoringActive);CHECK(p.getIdentityReadout("0")->status==p.getIdentityReadout("1")->status);
    CHECK(!p.getIdentityReadout("1")->muted&&!p.getIdentityReadout("1")->soloed);
    // The optional legacy side controls still exist and use the same owner.
    auto settings=c.view().chrome;settings.mute=true;p.setChrome(settings);
    auto* mute=dynamic_cast<juce::Button*>(find(p,"button:2:0"));CHECK(mute&&mute->isVisible());
    mute->triggerClick();pump();CHECK(commands==1);
    settings.mute=false;p.setChrome(settings);
    CHECK(c.setIdentityNavigation(false));p.refresh();CHECK(!find(p,"card-nav:next:0")->isShowing());
    CHECK(c.undoView());p.refresh();CHECK(find(p,"card-nav:next:0")->isShowing());
    // Optional meters share the one scheduler and stop completely when hidden.
    int meterReads=0;p.readMeter=[&](const Lane&,bool){++meterReads;MeterReadback r;r.available=true;r.count=1;r.values[0]=0.5f;return r;};
    settings=c.view().chrome;settings.inputMeters=true;p.setChrome(settings);wait(120);CHECK(meterReads>0);
    settings.inputMeters=false;p.setChrome(settings);const auto atStop=meterReads;wait(120);CHECK(meterReads==atStop);
    p.readMeter=nullptr;
    // A pending wake-up must not survive removing the owning lane or hiding UI.
    lens=p.getIdentityBounds("1");canvas=find(p,"canvas:1");point={lens.getX()+28,lens.getY()+8};q=canvas->getLocalPoint(&p,point).toFloat();
    records["1"].clip={q.x-10,q.y-10,20,20};pointer(point);CHECK(p.hasPendingIdentityTransition());
    p.setVisible(false);CHECK(!p.hasPendingIdentityTransition());wait(550);
    p.setVisible(true);p.refresh();wait(100);pointer(point); // let the native window-manager remap settle
    CHECK(!p.isIdentitySuppressed("1"));CHECK(p.hasPendingIdentityTransition());
    // Deadline law is tested with injected time. Native Timer delivery is
    // message-thread scheduled, so allow bounded dispatch jitter under CI load.
    const auto limit=juce::Time::getMillisecondCounterHiRes()+1500;
    while(!p.isIdentitySuppressed("1")&&juce::Time::getMillisecondCounterHiRes()<limit)pump();
    CHECK(p.isIdentitySuppressed("1"));
    p.setIdentityNavigation(false);CHECK(!find(p,"card-nav:next:0")->isShowing());
    p.removeFromDesktop();CHECK(commands==1);
}void adaptiveTitleContracts()
{
    Controller c;auto initial=fixture(30);
    initial.owners[1].name="A complete and intentionally long name for this shared track";
    CHECK(c.adopt(initial));CHECK(c.setMode(Mode::freeLanes));
    std::map<Id,Record> records;TestArtwork artwork;int commands=0,errors=0;Command last;
    std::map<Id,int> sizes;std::map<Id,std::function<void()>> contentChanged;
    c.onCommand=[&](const Command& cmd){
        ++commands;last=cmd;
        auto next=c.snapshot();++next.revision;
        if(cmd.kind==CommandKind::renameOwner)for(auto& o:next.owners)if(o.id==cmd.owner)o.name=cmd.label;
        return c.adopt(std::move(next));
    };
    TimelinePanel p(c,[&](const Lane& lane){
        LaneCanvas result;auto& r=records[lane.id];result.component=std::make_unique<MockCanvas>(r);
        result.bind=[&r](const Lane&,const CanvasFrame& f){r.frame=f;++r.binds;};
        result.containsEditableAt=[&r](juce::Point<float> q){return r.clip.contains(q);};
        result.preferredHeight=[&,id=lane.id](int){return sizes.contains(id)?sizes[id]:24;};
        result.setContentChangeListener=[&,id=lane.id](auto callback){contentChanged[id]=std::move(callback);};
        return result;
    },artwork);
    p.onResult=[&](const Result& r){if(!r)++errors;};
    p.setSize(1000,650);p.addToDesktop(juce::ComponentPeer::windowIsTemporary);p.setVisible(true);p.refresh();pump();
    auto wait=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    auto pointer=[&](juce::Point<int> pos){
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p.localPointToGlobal(pos).toFloat());
        p.getPeer()->handleMouseEvent(juce::MouseInputSource::mouse,pos.toFloat(),{},0,0,juce::Time::currentTimeMillis());pump();
    };
    auto title=[&](const Id& lane){auto* t=find(p,"identity-title:"+text(lane));CHECK(t&&t->isShowing());return t;};
    auto titlePoint=[&](const Id& lane){auto* t=title(lane);return p.getLocalPoint(t,t->getLocalBounds().getCentre());};
    auto begin=[&](const Id& lane){
        pointer(titlePoint(lane));auto* t=title(lane);t->mouseDoubleClick(event(*t,t->getLocalBounds().getCentre().toFloat(),2));pump();
        auto* e=dynamic_cast<juce::TextEditor*>(find(p,"identity-editor:"+text(lane)));CHECK(e&&e->isShowing());CHECK(p.isIdentityEditing(lane));return e;
    };
    auto* canvas=find(p,"canvas:0");CHECK(canvas);records["0"].clip=canvas->getLocalBounds().toFloat();
    const auto small=p.getIdentityBounds("0");const auto initialHeight=c.height("0");const auto original=initial.owners[1].name;
    pointer(titlePoint("0"));wait(550);
    CHECK(!p.isIdentitySuppressed("0"));CHECK(!p.hasPendingIdentityTransition());
    CHECK(p.getIdentityBounds("0").getWidth()>small.getWidth());CHECK(c.height("0")==initialHeight);
    CHECK(!p.getIdentityLayout("0")->titleTruncated);
    CHECK(records["0"].doubled==0); // title hover never synthesizes canvas input
    auto* arrow=find(p,"card-nav:next:0");pointer(p.getLocalPoint(arrow,arrow->getLocalBounds().getCentre()));
    CHECK(p.getIdentityBounds("0").getWidth()>small.getWidth());CHECK(!p.hasPendingIdentityTransition());
    // Leave ONLY the protected targets: metadata starts the existing delay.
    auto lens=p.getIdentityBounds("0");pointer({lens.getRight()-90,lens.getY()+6});
    CHECK(p.hasPendingIdentityTransition());wait(550);CHECK(p.isIdentitySuppressed("0"));
    CHECK(!find(p,"identity-title:0")->isShowing());CHECK(!arrow->isShowing());
    pointer({10,5});CHECK(!p.isIdentitySuppressed("0"));
    auto* editor=begin("0");CHECK(editor->getText()==text(original));
    CHECK(commands==0&&records["0"].doubled==0);editor->setText("Renamed shared track",false);
    CHECK(editor->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));pump();
    CHECK(commands==1&&last.kind==CommandKind::renameOwner&&last.owner=="owner");
    CHECK(c.owner("owner")->name=="Renamed shared track");CHECK(!p.isIdentityEditing("0"));
    CHECK(p.getIdentityReadout("1")->track=="Renamed shared track");
    editor=begin("0");editor->setText("Cancelled",false);
    CHECK(editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));pump();CHECK(commands==1);
    CHECK(c.owner("owner")->name=="Renamed shared track");
    editor=begin("0");editor->setText("   ",false);editor->onReturnKey();pump();CHECK(commands==1&&errors==1);
    // A status-only revision can rebase the rename, but a concurrent rename cannot.
    editor=begin("0");editor->setText("After status update",false);
    auto changed=c.snapshot();++changed.revision;changed.owners[1].muted=true;CHECK(c.adopt(changed));p.refresh();
    CHECK(p.isIdentityEditing("0"));editor->onReturnKey();pump();CHECK(commands==2);CHECK(c.owner("owner")->name=="After status update");
    editor=begin("0");editor->setText("Lost edit",false);
    changed=c.snapshot();++changed.revision;changed.owners[1].name="External rename";CHECK(c.adopt(changed));p.refresh();
    CHECK(!p.isIdentityEditing("0"));CHECK(c.owner("owner")->name=="External rename");CHECK(commands==2);
    editor=begin("0");editor->setText("Focus loss commits once",false);title("1")->grabKeyboardFocus();pump();
    CHECK(commands==3);CHECK(c.owner("owner")->name=="Focus loss commits once");
    // Scroll does not recycle an active editor; finishing removes the lease.
    editor=begin("0");editor->setText("Offscreen",false);p.setVerticalPosition(1800);pump();
    CHECK(p.isIdentityEditing("0"));CHECK(find(p,"identity-editor:0")==editor);
    editor->onEscapeKey();p.refresh();p.setVerticalPosition(0);pump();CHECK(commands==3);
    // A left-anchored lens expands towards the interior. Preserve the original
    // title hit region through this reflow, so the second click cannot hit a clip.
    changed=c.snapshot();++changed.revision;changed.owners[1].name=original;CHECK(c.adopt(changed));
    auto leftChrome=c.view().chrome;leftChrome.nameSide=presentation::TimelineNameSide::left;p.setChrome(leftChrome);
    pointer({10,5});const auto originalClick=titlePoint("0");pointer(originalClick);
    auto* clicked=p.getComponentAt(originalClick);CHECK(clicked==find(p,"identity-title:0"));
    clicked->mouseDoubleClick(event(*clicked,clicked->getLocalPoint(&p,originalClick).toFloat(),2));pump();
    CHECK(p.isIdentityEditing("0"));
    editor=dynamic_cast<juce::TextEditor*>(find(p,"identity-editor:0"));CHECK(editor);editor->onEscapeKey();pump();
    leftChrome.nameSide=presentation::TimelineNameSide::right;p.setChrome(leftChrome);pointer({10,5});
    // Automatic lane size is measured from content, independent of lens layout.
    changed=c.snapshot();++changed.revision;
    for(auto& l:changed.lanes)if(l.id=="0"||l.id=="1"){
        l.heightPolicy=HeightPolicy::automatic;l.minimumHeight=24;l.initialHeight=24;l.visual=VisualKind::point;
    }
    sizes["0"]=24;sizes["1"]=32;CHECK(c.adopt(changed));p.refresh();pump();pump();
    CHECK(c.height("0")==24&&c.height("1")==32);CHECK(!p.getIdentityLayout("0")->twoLines);
    CHECK(!find(p,"resize:0")->isShowing());CHECK(!c.beginResize("0"));
    CHECK(contentChanged["0"]);sizes["0"]=46;contentChanged["0"]();pump();pump();
    CHECK(c.height("0")==46);CHECK(p.getIdentityLayout("0")->twoLines);
    const auto bindings=records["0"].binds;for(int i=0;i<20;++i){contentChanged["0"]();p.refresh();}
    CHECK(records["0"].binds==bindings); // no measurement/layout feedback loop
    sizes["0"]=24;contentChanged["0"]();pump();CHECK(c.height("0")==24);CHECK(!p.getIdentityLayout("0")->twoLines);
    // All active icons fit below arrows even on a one-line 24px event lane.
    changed=c.snapshot();++changed.revision;changed.owners[1].status={true,true,true,true,true,true,true};
    CHECK(c.adopt(changed));p.refresh();auto geometry=*p.getIdentityLayout("0");
    for(auto r:geometry.indicators){CHECK(!r.isEmpty());CHECK(r.getY()>=geometry.navigationGroup.getBottom());}
    // Keep scroll anchor when an automatic row just above it changes height.
    p.setVerticalPosition(c.laneTop("1")+5);pump();auto* row=find(p,"lane:2");CHECK(row);
    const int oldY=p.getLocalArea(row,row->getLocalBounds()).getY();
    sizes["0"]=46;contentChanged["0"]();pump();
    row=find(p,"lane:2");CHECK(p.getLocalArea(row,row->getLocalBounds()).getY()==oldY);
    p.setVerticalPosition(0);pump();
    editor=begin("0");editor->setText("Never another song",false);
    changed=c.snapshot();changed.session="new-session";++changed.revision;CHECK(c.adopt(changed));p.refresh();pump();
    CHECK(!p.isIdentityEditing("0"));CHECK(commands==3);
    p.removeFromDesktop();
}

}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Controller controller;CHECK(controller.adopt(fixture(10000)));CHECK(controller.setMode(Mode::freeLanes));
    CHECK(controller.setChrome(presentation::TimelineChromeVisibility{}));
    std::map<Id,Record> records;TestArtwork artwork;Command received;int commands=0;
    controller.onCommand=[&](const Command& c){received=c;++commands;return Result{};};
    TimelinePanel panel(controller,[&](const Lane& lane){
        LaneCanvas c;auto& record=records[lane.id];c.component=std::make_unique<MockCanvas>(record);
        c.bind=[&record](const Lane&,const CanvasFrame& frame){++record.binds;record.frame=frame;};
        c.containsClipAt=[&record](juce::Point<float> point){return record.clip.contains(point);};
        c.saveState=[&record]{++record.saved;};c.preferredHeight=[](int){return 75;};return c;
    },artwork);
    panel.setSize(900,650);panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    panel.setVisible(true);panel.refresh();pump();
    CHECK(controller.totalHeight()==32+10000*96);
    CHECK(panel.getVisibleRowCount()<12);CHECK(panel.getResidentRowCount()<18);
    auto* canvas=find(panel,"canvas:0");CHECK(canvas);CHECK(canvas->getWidth()<900);CHECK(canvas->getHeight()==96);
    auto* row=find(panel,"lane:0");CHECK(row);CHECK(row->getHeight()==96);
    auto* globalResize=find(panel,"resize:global-lane");CHECK(globalResize&&!globalResize->isVisible());
    auto* global=find(panel,"lane:global-lane");CHECK(global->getHeight()==32);
    const auto created=panel.getCreatedRowCount();const auto binds=records["0"].binds;
    for(int i=0;i<100;++i)panel.refresh();CHECK(panel.getCreatedRowCount()==created);CHECK(records["0"].binds==binds);
    const auto identityCount=artwork.identitiesPrepared;
    panel.setViewStart(50);CHECK(artwork.identitiesPrepared==identityCount);
    panel.setViewStart(0);CHECK(artwork.identitiesPrepared==identityCount);
    const auto selectedBefore=controller.view().selectedLanes;
    auto lens=panel.getIdentityBounds("0");CHECK(!lens.isEmpty());
    CHECK(!find(panel,"name:0")&&!find(panel,"lane-name:0")&&!find(panel,"rename:0"));
    CHECK(artwork.identity.performer=="Ayra");CHECK(artwork.identity.track=="Shared Tensor");
    const auto centre=juce::Point<int>{lens.getX()+28,lens.getY()+8};
    CHECK(panel.getComponentAt(centre)==canvas); // metadata remains a real canvas target, even while visible
    CHECK(std::abs(panel.getLocalArea(row,row->getLocalBounds()).getCentreY()-lens.getCentreY())<=1);
    for(int h:{56,65,96,177,400}) {
        controller.setHeight("0",h);panel.refresh();
        const auto badge=panel.getIdentityBounds("0");row=find(panel,"lane:0");
        CHECK(std::abs(panel.getLocalArea(row,row->getLocalBounds()).getCentreY()-badge.getCentreY())<=1);
        for(int i=0;i<6;++i){auto* b=find(panel,"button:"+juce::String(i)+":0");CHECK(b&&b->getWidth()==22&&b->getHeight()==22);CHECK(row->getLocalBounds().contains(b->getBounds()));}
    }
    controller.setHeight("0",96);panel.refresh();
    const auto local=canvas->getLocalPoint(&panel,centre).toFloat();
    auto e=event(*canvas,local);canvas->mouseDown(e);
    canvas->mouseDrag(event(*canvas,local+juce::Point<float>(6,1),1,juce::ModifierKeys::leftButtonModifier,local,true));canvas->mouseUp(e);
    CHECK(records["0"].down==1&&records["0"].drag==1&&records["0"].up==1);
    CHECK(controller.view().selectedLanes==selectedBefore);
    juce::MouseWheelDetails wheel{};wheel.deltaY=0.1f;canvas->mouseWheelMove(e,wheel);CHECK(records["0"].wheel==1);
    canvas->mouseDown(event(*canvas,local,2));canvas->mouseDoubleClick(event(*canvas,local,2));canvas->mouseUp(event(*canvas,local,2));
    CHECK(records["0"].down==2&&records["0"].up==2&&records["0"].doubled==1);CHECK(commands==0);
    CHECK(panel.getIdentityBounds("global-lane").getHeight()<=32);
    // Drive JUCE's actual peer dispatch including mouse listeners.
    auto pointer=[&](juce::Point<int> p){
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(panel.localPointToGlobal(p).toFloat());
        panel.getPeer()->handleMouseEvent(juce::MouseInputSource::mouse,p.toFloat(),{},0,0,juce::Time::currentTimeMillis());
        pump();
    };
    pointer(centre);CHECK(!panel.isIdentitySuppressed("0"));
    records["0"].clip={local.x-6,local.y-6,12,12};
    panel.notifyCanvasContentChanged("0");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(550);CHECK(panel.isIdentitySuppressed("0"));
    CHECK(panel.getIdentityBounds("0")==lens);CHECK(panel.getComponentAt(centre)==canvas);
    pointer({lens.getX()+2,lens.getY()+2});CHECK(panel.isIdentitySuppressed("0"));
    records["0"].clip={};panel.notifyCanvasContentChanged("0");CHECK(panel.isIdentitySuppressed("0"));
    pointer({lens.getX()-4,lens.getCentreY()});CHECK(!panel.isIdentitySuppressed("0"));
    pointer(centre);CHECK(!panel.isIdentitySuppressed("0"));
    pointer({20,5});auto image=panel.createComponentSnapshot(panel.getLocalBounds());
    const int underlays=artwork.underlaysPrepared;CHECK(artwork.groupsPainted>0);
    for(int i=0;i<10;++i)image=panel.createComponentSnapshot(panel.getLocalBounds());
    CHECK(artwork.underlaysPrepared==underlays);
    panel.notifyCanvasContentChanged("0");image=panel.createComponentSnapshot(panel.getLocalBounds());
    CHECK(artwork.underlaysPrepared==underlays+1);
    auto* resize=find(panel,"resize:0");CHECK(resize&&resize->isVisible());
    auto* hit=panel.getComponentAt(panel.getLocalPoint(resize,juce::Point<int>{3,1}));CHECK(hit==resize);
    resize->mouseDown(event(*resize,{3,1}));CHECK(controller.resizing());
    resize->mouseDrag(event(*resize,{3,31},1,juce::ModifierKeys::leftButtonModifier,{3,1},true));CHECK(controller.height("0")==126);
    resize->mouseUp(event(*resize,{3,31}));CHECK(!controller.resizing());CHECK(controller.undoView());CHECK(controller.height("0")==96);panel.refresh();
    auto settings=controller.view().chrome;settings.navigation=false;settings.mute=false;settings.solo=false;settings.open=false;settings.monitoring=false;
    panel.setChrome(settings);canvas=find(panel,"canvas:0");const int fullWidth=canvas->getWidth();
    const double screenXBefore=canvas->getX()+records["0"].frame.time.timeToX(5);
    settings.inputMeters=true;settings.outputMeters=true;panel.setChrome(settings);canvas=find(panel,"canvas:0");
    CHECK(canvas->getWidth()<fullWidth);CHECK(records["0"].frame.time.timeToX(0)>=0);
    CHECK(canvas->getX()+records["0"].frame.time.timeToX(5)==find(panel,"canvas:1")->getX()+records["1"].frame.time.timeToX(5));
    juce::ignoreUnused(screenXBefore);
    CHECK(find(panel,"canvas:1")->getWidth()==canvas->getWidth()); // MIDI meters permitted.
    settings.nameSide=presentation::TimelineNameSide::left;settings.accentSide=presentation::TimelineAccentSide::both;panel.setChrome(settings);
    CHECK(panel.getIdentityBounds("0").getX()<200);
    lens=panel.getIdentityBounds("0");canvas=find(panel,"canvas:0");
    const auto leftPoint=juce::Point<int>{lens.getX()+28,lens.getY()+8};
    const auto leftLocal=canvas->getLocalPoint(&panel,leftPoint).toFloat();
    records["0"].clip={leftLocal.x-10,leftLocal.y-10,20,20};pointer(leftPoint);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(550);
    CHECK(panel.isIdentitySuppressed("0"));pointer({lens.getRight()+4,lens.getCentreY()});
    CHECK(!panel.isIdentitySuppressed("0"));records["0"].clip={};
    row=find(panel,"lane:0");row->mouseDown(event(*row,{1,20}));row->mouseUp(event(*row,{1,20}));CHECK(controller.view().selectedLanes.contains("0"));
    auto* row1=find(panel,"lane:1");auto ctrl=juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier);
    row1->mouseDown(event(*row1,{row1->getWidth()-2.0f,20},1,ctrl));row1->mouseUp(event(*row1,{row1->getWidth()-2.0f,20},1,ctrl));
    CHECK(controller.view().selectedLanes.contains("0")&&controller.view().selectedLanes.contains("1"));
    // Visibility removes only presentation height, retains identities/selection invariants.
    const double oldTotal=controller.totalHeight();CHECK(controller.setLaneVisible("0",false));panel.refresh();CHECK(controller.totalHeight()==oldTotal-96);
    CHECK(controller.setLaneVisible("0",true));panel.refresh();
    panel.setViewStart(100000);canvas=find(panel,"canvas:0");CHECK(canvas->getWidth()<900);CHECK(records["0"].frame.time.startSeconds==100000);
    std::size_t maximum=0;
    for(int i=0;i<300;++i){panel.setVerticalPosition(i*2301);maximum=std::max(maximum,panel.getResidentRowCount());CHECK(panel.getResidentRowCount()<=panel.getVisibleRowCount()+6);}
    CHECK(maximum<20);CHECK(records["0"].saved>0);panel.setVerticalPosition(0);CHECK(find(panel,"canvas:0"));
    auto modified=controller.snapshot();++modified.revision;modified.lanes[3].heightPolicy=HeightPolicy::automatic;
    CHECK(controller.adopt(modified));panel.refresh();pump();CHECK(controller.height("2")==75);CHECK(!controller.beginResize("2"));
    const auto previousMade=panel.getCreatedRowCount();modified.lanes[1].visual=VisualKind::midi;++modified.revision;
    CHECK(controller.adopt(modified));panel.refresh();CHECK(panel.getCreatedRowCount()==previousMade+1);
    panel.removeFromDesktop();
    cardStatusContracts();
    adaptiveTitleContracts();
    std::cout<<"PASS "<<checks<<" native component assertions; 10000 logical lanes; maximum resident rows "<<maximum<<"\n";
}
