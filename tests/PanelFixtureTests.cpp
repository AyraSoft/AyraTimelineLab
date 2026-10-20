#include "../demo/PanelFixture.h"
#include "../demo/PanelClipWindow.h"
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
    // Distinct temporal group composes with the explicit vertical link, exactly once per clip.
    auto group=command(host,panel::CommandKind::temporalGroup);
    group.events={{"voice.audio","voice.audio.one"},{"voice.audio","voice.audio.two"}};CHECK(host.apply(group));
    auto move=command(host,panel::CommandKind::moveClips);move.events={{"voice.audio","voice.audio.two"}};move.deltaSeconds=1;
    CHECK(host.apply(move));CHECK(start(store,"voice.audio","voice.audio.two")==11);
    CHECK(start(store,"voice.audio","voice.audio.one")==3);CHECK(start(store,"voice.midi","voice.midi.one")==3);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.two"})->start=7;
    CHECK(!store.commit(candidate,"Would push linked first clip below zero"));
    CHECK(start(store,"voice.audio","voice.audio.two")==11);CHECK(start(store,"voice.audio","voice.audio.one")==3);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->fadeIn=1;
    CHECK(store.commit(candidate,"Independent fade"));
    CHECK(fixtureClip(store.state(),{"voice.midi","voice.midi.one"})->fadeIn==0.5);
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->start+=1;
    fixtureClip(candidate,{"voice.midi","voice.midi.one"})->start+=2;
    CHECK(!store.commit(candidate,"Conflicting deltas"));CHECK(start(store,"voice.audio","voice.audio.one")==3);
    candidate=store.state();auto meta=std::make_shared<PanelFixtureMetadata>(*candidate.panelMetadata);
    meta->locked.insert({"voice.image","voice.image.one"});candidate.panelMetadata=meta;CHECK(store.commit(candidate,"Lock peer"));
    candidate=store.state();fixtureClip(candidate,{"voice.audio","voice.audio.one"})->start+=1;
    CHECK(!store.commit(candidate,"Locked peer"));CHECK(start(store,"voice.audio","voice.audio.one")==3);
    CHECK(store.undo());
    auto erase=command(host,panel::CommandKind::deleteClips);erase.events={{"voice.audio","voice.audio.one"}};
    CHECK(host.apply(erase));CHECK(!fixtureClip(store.state(),{"voice.audio","voice.audio.one"}));
    CHECK(!fixtureClip(store.state(),{"voice.midi","voice.midi.one"}));CHECK(!fixtureClip(store.state(),{"voice.audio","voice.audio.two"}));
    CHECK(panel::validate(host.snapshot()));CHECK(store.undo());CHECK(start(store,"voice.audio","voice.audio.one")==3);
    auto unlink=command(host,panel::CommandKind::unlinkClips);unlink.events={{"voice.midi","voice.midi.one"}};CHECK(host.apply(unlink));
    candidate=store.state();fixtureClip(candidate,{"voice.midi","voice.midi.one"})->start+=1;
    CHECK(store.commit(candidate,"Detached move"));CHECK(start(store,"voice.midi","voice.midi.one")==4);
    CHECK(start(store,"voice.audio","voice.audio.one")==3);
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
int main(){visibleClipProjection();controls();relations();failuresAndLifetime();std::cout<<"PASS "<<count<<" fixture/transaction checks\n";}
