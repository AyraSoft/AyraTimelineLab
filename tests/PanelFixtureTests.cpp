#include "../demo/PanelFixture.h"
#include "../demo/PanelClipWindow.h"
#include "../modules/ayra_timeline_panel/TimelineGrouping.h"
#include <cstdlib>
#include <iostream>
using namespace ayra::lab;
namespace {
int count=0;
void check(bool ok,const char* text,int line){++count;if(!ok){std::cerr<<"FAIL "<<line<<" "<<text<<'\n';std::exit(1);}}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
panel::Command command(PanelFixtureHost& host,panel::CommandKind kind)
{const auto s=host.snapshot(); panel::Command c;c.kind=kind;c.session=s.session;c.revision=s.revision;return c;}
double start(const Store& s,const std::string& lane,const std::string& clip)
{const auto* c=fixtureClip(s.state(),{lane,clip});CHECK(c);return c->start;}
void controls()
{
    Store store(makePanelFixture()); CHECK(validate(store.state()).empty());
    PanelFixtureHost host(store); CHECK(panel::validate(host.snapshot()));
    panel::Controller c; CHECK(c.adopt(host.snapshot()));
    c.onCommand=[&](const auto& cmd){return host.apply(cmd);};
    CHECK(c.control("voice.audio",panel::CommandKind::monitoring,false,panel::Monitoring::live));
    CHECK(findTrack(store.state(),"voice.audio")->monitor=="Live");
    CHECK(findTrack(store.state(),"voice.midi")->monitor=="Live");
    CHECK(findTrack(store.state(),"voice.image")->monitor=="Live");
    CHECK(findTrack(store.state(),"guitar.audio")->monitor=="Auto");
    CHECK(!c.control("voice.midi",panel::CommandKind::mute,true)); // snapshot now stale
    CHECK(c.adopt(host.snapshot())); CHECK(c.control("voice.midi",panel::CommandKind::mute,true));
    CHECK(findTrack(store.state(),"voice.audio")->mute); CHECK(findTrack(store.state(),"voice.midi")->mute);
    CHECK(store.undo()); CHECK(!findTrack(store.state(),"voice.audio")->mute);
    CHECK(store.redo()); CHECK(findTrack(store.state(),"voice.audio")->mute);
    const auto ownerColour=store.state().panelMetadata->owners[4].colour;
    CHECK(host.setStreamColour(panel::Stream::audio,0x112233));
    CHECK(findTrack(store.state(),"voice.audio")->colour==0xff112233);
    CHECK(findTrack(store.state(),"guitar.audio")->colour==0xff112233);
    CHECK(findTrack(store.state(),"voice.midi")->colour==0xffe34b4b);
    CHECK(store.state().panelMetadata->owners[4].colour==ownerColour);
    CHECK(c.adopt(host.snapshot()));CHECK(c.rename("voice.midi","MIDI solo label"));
    CHECK(findTrack(store.state(),"voice.midi")->name=="MIDI solo label");
    CHECK(findTrack(store.state(),"voice.audio")->name=="Audio");
    CHECK(store.undo());CHECK(findTrack(store.state(),"voice.midi")->name=="MIDI");
    CHECK(c.adopt(host.snapshot())); CHECK(c.renameOwner("voice","Voce principale"));
    const auto renamed=host.snapshot();
    const auto owned=std::find_if(renamed.owners.begin(),renamed.owners.end(),[](const auto& o){return o.id=="voice";});
    CHECK(owned!=renamed.owners.end()); CHECK(owned->name=="Voce principale");
    CHECK(findTrack(store.state(),"voice.audio")->name=="Audio");
    CHECK(findTrack(store.state(),"voice.midi")->name=="MIDI");
    CHECK(!c.renameOwner("voice","Stale request"));
    CHECK(store.undo()); CHECK(c.adopt(host.snapshot()));
    CHECK(c.owner("voice")->name=="Voice");
    CHECK(!c.renameOwner("voice","\n")); CHECK(!c.renameOwner("missing","New name"));
    auto bad=store.state();bad.panelMetadata.reset();CHECK(!store.commit(bad,"Bad import"));
    CHECK(store.state().panelMetadata);
}
void relations()
{
    Store store(makePanelFixture());PanelFixtureHost host(store);
    auto candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->start=3;
    CHECK(store.commit(candidate,"Approved canvas move"));
    CHECK(start(store,"voice.audio","voice.audio.one")==3);
    CHECK(start(store,"voice.midi","voice.midi.one")==3);
    CHECK(start(store,"voice.image","voice.image.one")==3);
    CHECK(start(store,"guitar.audio","guitar.audio.one")==2);
    CHECK(store.undo());CHECK(start(store,"voice.midi","voice.midi.one")==2);CHECK(!store.canUndo());
    CHECK(store.redo());CHECK(start(store,"voice.image","voice.image.one")==3);
    CHECK(store.undo());
    // A linked member cannot be reparented into a horizontal container.
    auto group=command(host,panel::CommandKind::glueClips);
    group.events={{"voice.audio","voice.audio.one"},{"voice.audio","voice.audio.two"}};CHECK(!host.apply(group));
    auto move=command(host,panel::CommandKind::moveClips);move.events={{"voice.audio","voice.audio.two"}};move.deltaSeconds=1;
    CHECK(host.apply(move));CHECK(start(store,"voice.audio","voice.audio.two")==11);
    CHECK(start(store,"voice.audio","voice.audio.one")==2);CHECK(start(store,"voice.midi","voice.midi.one")==2);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.two"})->start=7;
    CHECK(store.commit(candidate,"Independent sibling move"));
    CHECK(start(store,"voice.audio","voice.audio.two")==7);CHECK(start(store,"voice.audio","voice.audio.one")==2);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->fadeIn=1;
    CHECK(store.commit(candidate,"Independent fade"));
    CHECK(fixtureClip(store.state(),{"voice.midi","voice.midi.one"})->fadeIn==0.5);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->start+=1;
    fixtureClip(candidate,{"voice.midi","voice.midi.one"})->start+=2;
    CHECK(!store.commit(candidate,"Conflicting deltas"));CHECK(start(store,"voice.audio","voice.audio.one")==2);
    candidate=store.state();auto meta=std::make_shared<PanelFixtureMetadata>(*candidate.panelMetadata);
    meta->locked.insert({"voice.image","voice.image.one"});candidate.panelMetadata=meta;CHECK(store.commit(candidate,"Lock peer"));
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->start+=1;
    CHECK(!store.commit(candidate,"Locked peer"));CHECK(start(store,"voice.audio","voice.audio.one")==2);
    CHECK(store.undo());
    auto erase=command(host,panel::CommandKind::deleteClips);erase.events={{"voice.audio","voice.audio.one"}};
    CHECK(host.apply(erase));CHECK(!fixtureClip(store.state(),{"voice.audio","voice.audio.one"}));
    CHECK(!fixtureClip(store.state(),{"voice.midi","voice.midi.one"}));CHECK(fixtureClip(store.state(),{"voice.audio","voice.audio.two"}));
    CHECK(panel::validate(host.snapshot()));CHECK(store.undo());CHECK(start(store,"voice.audio","voice.audio.one")==2);
    auto unlink=command(host,panel::CommandKind::unlinkClips);unlink.events={{"voice.midi","voice.midi.one"}};CHECK(host.apply(unlink));
    candidate=store.state();fixtureClip(candidate,{"voice.midi","voice.midi.one"})->start+=1;
    CHECK(store.commit(candidate,"Detached move"));CHECK(start(store,"voice.midi","voice.midi.one")==3);
    CHECK(start(store,"voice.audio","voice.audio.one")==2);
}
void glueContainers()
{
    Store store(makePanelFixture());PanelFixtureHost host(store);
    auto snapshot=host.snapshot();std::vector<panel::EventRef> refs;
    const std::string lane="guitar.audio",one=lane+".one",two=lane+".two",three=lane+".three";
    CHECK(panel::glueContainerTargets(snapshot,lane,one,{},refs));
    CHECK(refs==std::vector<panel::EventRef>({{lane,one},{lane,two}}));
    CHECK(!panel::glueContainerTargets(snapshot,lane,three,{},refs));
    CHECK(!panel::glueContainerTargets(snapshot,lane,"missing",{},refs));
    const std::vector<panel::EventRef> nonAdjacent{{lane,one},{lane,three},{"voice.midi","voice.midi.one"}};
    CHECK(panel::glueContainerTargets(snapshot,lane,two,nonAdjacent,refs));
    CHECK(refs==std::vector<panel::EventRef>({{lane,one},{lane,three}}));
    std::reverse(snapshot.events.begin(),snapshot.events.end());
    CHECK(panel::glueContainerTargets(snapshot,lane,two,nonAdjacent,refs));
    CHECK(refs==std::vector<panel::EventRef>({{lane,one},{lane,three}}));
    const auto verticalBefore=std::find_if(snapshot.relations.begin(),snapshot.relations.end(),[](const auto& rel){return rel.kind==panel::RelationKind::clipLink;})->members;
    auto glue=command(host,panel::CommandKind::glueClips);glue.events={{lane,one},{lane,two}};
    CHECK(host.apply(glue));
    auto* track=findTrack(store.state(),lane);CHECK(track&&track->clips.size()==2);
    auto container=std::find_if(track->clips.begin(),track->clips.end(),[](const auto& clip){return clip.isContainer();});
    CHECK(container!=track->clips.end());CHECK(container->children.size()==2);
    CHECK(container->children[0].id==one&&container->children[0].start==0);
    CHECK(container->children[1].id==two&&container->children[1].start==8);
    CHECK(std::find_if(store.state().panelMetadata->relations.begin(),store.state().panelMetadata->relations.end(),
        [](const auto& rel){return rel.kind==panel::RelationKind::clipLink;})->members==verticalBefore);
    CHECK(panel::validate(host.snapshot()));

    const auto innerId=container->id;
    glue=command(host,panel::CommandKind::glueClips);glue.events={{lane,innerId},{lane,three}};
    CHECK(host.apply(glue));track=findTrack(store.state(),lane);CHECK(track->clips.size()==1);
    CHECK(track->clips.front().children.size()==2);
    CHECK(track->clips.front().children.front().id==innerId);
    CHECK(track->clips.front().children.front().children.size()==2);

    auto dissolve=command(host,panel::CommandKind::dissolveContainer);
    dissolve.events={{lane,track->clips.front().id}};CHECK(host.apply(dissolve));
    track=findTrack(store.state(),lane);CHECK(track->clips.size()==2);
    CHECK(findClip(*track,innerId)&&findClip(*track,three));
    dissolve=command(host,panel::CommandKind::dissolveContainer);dissolve.events={{lane,innerId}};
    CHECK(host.apply(dissolve));CHECK(findTrack(store.state(),lane)->clips.size()==3);
    CHECK(store.undo());CHECK(findTrack(store.state(),lane)->clips.size()==2);

    auto candidate=store.state();auto meta=std::make_shared<PanelFixtureMetadata>(*candidate.panelMetadata);
    meta->locked.insert({lane,innerId});candidate.panelMetadata=meta;CHECK(store.commit(candidate,"Lock container"));
    const auto lockedState=store.state();dissolve=command(host,panel::CommandKind::dissolveContainer);dissolve.events={{lane,innerId}};
    CHECK(!host.apply(dissolve));CHECK(store.state()==lockedState);
    auto linkedGlue=command(host,panel::CommandKind::glueClips);
    linkedGlue.events={{"voice.audio","voice.audio.one"},{"voice.audio","voice.audio.two"}};
    CHECK(!host.apply(linkedGlue));
    // Explicit host minimums are not bypassed by the compact presentation profile.
    panel::Controller controller;snapshot=host.snapshot();
    for(auto& l:snapshot.lanes)if(l.id==lane)l.minimumHeight=80;
    CHECK(controller.adopt(snapshot));CHECK(controller.setHeight(lane,32));CHECK(controller.height(lane)==80);
    snapshot=controller.snapshot();++snapshot.revision;
    for(auto& l:snapshot.lanes)if(l.id==lane){l.maximumHeight=200;l.initialHeight=176;}
    CHECK(controller.adopt(snapshot));
    CHECK(controller.cycleHeight(lane));CHECK(controller.height(lane)==200);
    CHECK(controller.cycleHeight(lane));CHECK(controller.height(lane)==80);
}

void failuresAndLifetime()
{
    Store store(makePanelFixture());int calls=0;store.changed=[&]{++calls;};
    {
        PanelFixtureHost host(store);
        auto c=command(host,panel::CommandKind::linkClips);
        c.events={{"voice.audio","voice.audio.two"},{"voice.midi","voice.midi.two"}};CHECK(host.apply(c));
        c=command(host,panel::CommandKind::linkClips);
        c.events={{"voice.audio","voice.audio.two"},{"voice.image","voice.image.two"}};CHECK(!host.apply(c));
        auto before=store.state();auto candidate=before;
        auto* clip=fixtureClip(candidate,{"voice.audio","voice.audio.one"});clip->length=2;
        auto sibling=*clip;sibling.id="split-result";sibling.start=4;sibling.length=4;
        findTrack(candidate,"voice.audio")->clips.push_back(sibling);
        CHECK(!store.commit(candidate,"Unsupported linked split"));CHECK(store.state()==before);
        c=command(host,panel::CommandKind::moveClips);c.events={{"bad","bad"}};c.deltaSeconds=1;CHECK(!host.apply(c));
        CHECK(!host.setStreamColour(static_cast<panel::Stream>(99),0));
        c=command(host,panel::CommandKind::solo);c.owner="song-main";c.value=true;CHECK(!host.apply(c));
        c=command(host,panel::CommandKind::openOwner);c.owner="voice";CHECK(!host.apply(c));
        panel::Id opened;host.onOpenOwner=[&](const auto& id){opened=id;};CHECK(host.apply(c));CHECK(opened=="voice");
    }
    CHECK(!store.prepareCommit); const int previous=calls;
    store.selectClip("voice.audio.one");CHECK(calls==previous+1);
}
void relationAwareSplit()
{
    Store store(makePanelFixture());PanelFixtureHost host(store);const auto before=store.state();
    const auto split=host.splitClip("voice.audio","voice.audio.one",5.0);CHECK(split.result);
    CHECK(split.rightClipId=="voice.audio.one_cut");
    for(const std::string lane:{"voice.audio","voice.midi","voice.image"})
    {
        const auto* left=fixtureClip(store.state(),{lane,lane+".one"});
        const auto* right=fixtureClip(store.state(),{lane,lane+".one_cut"});
        CHECK(left&&right);CHECK(left->start==2&&left->length==3);
        CHECK(right->start==5&&right->length==3);
    }
    const auto& relations=store.state().panelMetadata->relations;
    CHECK(relations.size()==before.panelMetadata->relations.size()+1);
    CHECK(std::count_if(relations.begin(),relations.end(),[](const auto& relation)
        {return relation.kind==panel::RelationKind::clipLink;})==2);
    CHECK(panel::validate(host.snapshot()));CHECK(store.undo());CHECK(store.state()==before);
}
void containerSplitDoesNotDuplicate()
{
    Store store(makePanelFixture());PanelFixtureHost host(store);
    const std::string lane="backing.audio";
    const auto split=host.splitClip(lane,"backing.audio.glue",9.0);CHECK(split.result);
    const auto* track=findTrack(store.state(),lane);CHECK(track);
    const auto* left=findClip(*track,"backing.audio.glue");
    const auto* right=findClip(*track,split.rightClipId);
    CHECK(left&&right&&left->isContainer()&&right->isContainer());
    CHECK(left->children.size()==1&&left->children.front().id=="backing.audio.one");
    CHECK(right->children.size()==1&&right->children.front().id=="backing.audio.two");
    CHECK(right->children.front().start==1.0);
    CHECK(panel::validate(host.snapshot()));

    auto dissolve=command(host,panel::CommandKind::dissolveContainer);
    dissolve.events={{lane,left->id},{lane,right->id}};CHECK(host.apply(dissolve));
    track=findTrack(store.state(),lane);CHECK(track&&track->clips.size()==3);
    CHECK(track->clips[0].id=="backing.audio.one");
    CHECK(track->clips[1].id=="backing.audio.two");
    CHECK(track->clips[2].id=="backing.audio.three");
}
void visibleClipProjection()
{
    Track source;source.id="dense";
    for(int i=0;i<10000;++i)source.clips.push_back({std::to_string(i),"",i*10.0,2,1,1});
    PanelClipWindow window;window.update(source,1);
    ayra::timeline::TimelineTimeWindow t{0,10,0};
    auto visible=window.visible(t,100,false);CHECK(visible.size()==2);
    const auto projected=window.projection(visible,80);CHECK(projected.clips.size()==2);CHECK(projected.height==80);
    CHECK(projected.clips[0].id=="0"&&projected.clips[1].id=="1");CHECK(source.clips.size()==10000);
    t.startSeconds=50000;visible=window.visible(t,100,false);CHECK(!visible.contains("0"));CHECK(visible.contains("5000"));
    CHECK(window.canonicalIds->contains("0")); // invisible is not deletion
    CHECK(window.visible(t,100,true).size()==10000); // captured gesture retains its scope
    source.clips.erase(source.clips.begin());window.update(source,2);CHECK(!window.canonicalIds->contains("0"));
}
}
int main(){glueContainers();containerSplitDoesNotDuplicate();visibleClipProjection();controls();relations();failuresAndLifetime();relationAwareSplit();std::cout<<"PASS "<<count<<" fixture/transaction checks\n";}
