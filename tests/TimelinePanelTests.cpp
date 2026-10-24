#include "TimelineModel.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
using namespace ayra::timeline_panel;
namespace {
int checks = 0;
void check(bool ok, const char* expression, int line)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL line " << line << ": " << expression << '\n'; std::exit(1); }
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)
Snapshot fixture()
{
    Snapshot s; s.session = "song-a"; s.revision = 1;
    s.owners = {
        {"song", {}, "Song", 0xffffc400, OwnerRole::song, false, false, Monitoring::automatic, {false,false,false,false}},
        {"performer", "song", "Performer", 0xff00c0d0, OwnerRole::performer},
        {"a", "performer", "Track A", 0xff00c0d0},
        {"b", "performer", "Track B", 0xff7755aa},
        {"mix", "performer", "Convergence", 0xff00c0d0, OwnerRole::convergence},
        {"main", "song", "Song Main", 0xffffc400, OwnerRole::songMain, false, false, Monitoring::automatic, {true,false,true,true}},
        {"global", {}, "Global", 0xff808080, OwnerRole::global, false, false, Monitoring::automatic, {false,false,false,false}}
    };
    s.lanes = {
        {"a.audio", "a", "a.bus1", "Audio", Stream::audio},
        {"a.midi", "a", "a.bus2", "MIDI", Stream::midi, VisualKind::midi},
        {"b.audio", "b", "b.bus1", "Audio", Stream::audio},
        {"main.audio", "main", "main.bus1", "Audio", Stream::audio},
        {"tempo", "global", {}, "Tempo", Stream::global, VisualKind::continuous, 64, 32, 1200, true}
    };
    s.events = {{{"a.audio","c1"},0,2}, {{"a.audio","c2"},4,6},
                {{"a.midi","c3"},0,2}, {{"b.audio","c4"},4,6},
                {{"a.audio","same-time"},4,5}, {{"tempo","first"},0,0}};
    return s;
}
std::vector<Id> laneOrder(const Controller& c)
{
    std::vector<Id> result;
    for (const auto& r : c.rows()) if (!r.lane.empty()) result.push_back(r.lane);
    return result;
}
void validation()
{
    auto s = fixture(); CHECK(validate(s));
    auto bad = s; bad.lanes[0].stream=static_cast<Stream>(88); CHECK(!validate(bad));
    bad=s; bad.owners[0].monitoring=static_cast<Monitoring>(-1); CHECK(!validate(bad));
    bad=s; bad.owners[0].parent = "a"; CHECK(!validate(bad));
    bad = s; bad.owners[0].parent = "missing"; CHECK(!validate(bad));
    bad = s; bad.owners.push_back(bad.owners[0]); CHECK(!validate(bad));
    bad = s; bad.lanes[0].owner = "missing"; CHECK(!validate(bad));
    bad = s; bad.lanes.push_back(bad.lanes[0]); CHECK(!validate(bad));
    bad = s; bad.lanes[0].minimumHeight = 2000; CHECK(!validate(bad));
    bad = s; bad.events[0].start = std::numeric_limits<double>::quiet_NaN(); CHECK(!validate(bad));
    bad = s; bad.events[0].end = -1; CHECK(!validate(bad));
    bad = s; bad.events.push_back(bad.events[0]); CHECK(!validate(bad));
    bad = s; bad.owners[5].soloed = true; CHECK(!validate(bad));
    bad = s; bad.relations = {{"l",RelationKind::clipLink,{{"a.audio","c1"},{"a.audio","c2"}}}}; CHECK(!validate(bad));
    s.relations = {{"l",RelationKind::clipLink,{{"a.audio","c1"},{"a.midi","c3"}}}};
    CHECK(validate(s));
    bad = s; bad.relations[0].members.push_back({"a.audio","c1"}); CHECK(!validate(bad));
    Controller c; CHECK(c.adopt(s)); const auto count = c.rows().size();
    CHECK(!c.adopt(bad)); CHECK(c.rows().size() == count); CHECK(c.snapshot().revision == 1);
}
void layout()
{
    Controller c; auto s = fixture(); CHECK(c.adopt(s));
    CHECK(c.rows().front().lane == "tempo");
    const auto initial = laneOrder(c);
    CHECK(initial == std::vector<Id>({"tempo","a.audio","a.midi","b.audio","main.audio"}));
    CHECK(c.toggleCollapse("performer"));
    CHECK(laneOrder(c) == std::vector<Id>({"tempo","main.audio"}));
    CHECK(std::any_of(c.rows().begin(),c.rows().end(),[](const auto& r){return r.owner=="mix"&&r.summary;}));
    CHECK(c.toggleCollapse("performer")); CHECK(laneOrder(c) == initial);
    CHECK(c.toggleCollapse("a")); CHECK(c.setMode(Mode::freeLanes));
    CHECK(laneOrder(c) == initial); // structural collapse cannot hide free lanes
    CHECK(c.moveBefore({"b.audio"},"a.midi"));
    CHECK(laneOrder(c) == std::vector<Id>({"tempo","a.audio","b.audio","a.midi","main.audio"}));
    CHECK(c.snapshot().owners == s.owners); CHECK(c.snapshot().lanes == s.lanes); CHECK(c.snapshot().events == s.events);
    CHECK(c.setMode(Mode::structured)); CHECK(c.toggleCollapse("a")); CHECK(laneOrder(c) == initial);
    CHECK(c.setMode(Mode::freeLanes)); CHECK(laneOrder(c)[2] == "b.audio");
    CHECK(!c.moveBefore({"tempo"},"a.audio")); CHECK(!c.moveBefore({"a.audio"},"tempo"));
    CHECK(!c.moveBefore({"absent"},"a.audio"));
    CHECK(c.select("a.audio",Controller::Selection::replace));
    CHECK(c.select("b.audio",Controller::Selection::toggle)); CHECK(c.groupLayout("free-group"));
    const auto* freeA=c.layoutGroupForRow("lane:a.audio");
    const auto* freeB=c.layoutGroupForRow("lane:b.audio");
    CHECK(freeA&&freeA->id=="free-group"&&freeA->ordinal==0&&!freeA->joinsPrevious&&freeA->joinsNext);
    CHECK(freeB&&freeB->id=="free-group"&&freeB->ordinal==0&&freeB->joinsPrevious&&!freeB->joinsNext);
    CHECK(!c.layoutGroupForRow("lane:a.midi"));
    CHECK(!c.groupLayout("overlap"));
    CHECK(c.moveBefore({"a.audio"},"main.audio"));
    CHECK(laneOrder(c) == std::vector<Id>({"tempo","a.midi","a.audio","b.audio","main.audio"}));
    CHECK(!c.moveBefore({"a.audio"},"b.audio"));
    CHECK(c.setMode(Mode::structured));
    CHECK(c.groupLayout("track-group"));
    CHECK(c.view().layoutGroups.back().members == std::vector<Id>({"a","b"}));
    const auto* structuredFirst=c.layoutGroupForRow("lane:a.audio");
    const auto* structuredMiddle=c.layoutGroupForRow("lane:a.midi");
    const auto* structuredLast=c.layoutGroupForRow("lane:b.audio");
    CHECK(structuredFirst&&!structuredFirst->joinsPrevious&&structuredFirst->joinsNext);
    CHECK(structuredMiddle&&structuredMiddle->joinsPrevious&&structuredMiddle->joinsNext);
    CHECK(structuredLast&&structuredLast->joinsPrevious&&!structuredLast->joinsNext);
    CHECK(!c.moveBefore({"a.audio"},"main.audio")); // would reparent the tracks
    CHECK(c.ungroupLayout());CHECK(!c.layoutGroupForRow("lane:a.audio"));
    CHECK(c.moveBefore({"b.audio"},"a.audio"));
    CHECK(laneOrder(c) == std::vector<Id>({"tempo","b.audio","a.audio","a.midi","main.audio"}));
    CHECK(c.undoView()); CHECK(laneOrder(c) == initial); CHECK(c.redoView()); CHECK(laneOrder(c)[1] == "b.audio");
    CHECK(c.snapshot().relations.empty()); // layout groups never create musical links
}
void resizeAndViewport()
{
    Controller c; CHECK(c.adopt(fixture()));
    CHECK(c.beginResize("a.audio")); CHECK(!c.endResize(true)); CHECK(!c.undoView());
    CHECK(c.beginResize("a.audio")); CHECK(!c.beginResize("a.midi")); CHECK(!c.setMode(Mode::freeLanes));
    CHECK(!c.updateResize(std::numeric_limits<double>::infinity()));
    CHECK(c.updateResize(50000)); CHECK(c.height("a.audio") == 1200);
    CHECK(!c.endResize(false)); CHECK(c.height("a.audio") == 176);
    CHECK(c.beginResize("a.audio")); CHECK(c.updateResize(-1000)); CHECK(c.height("a.audio") == height_presets::compact);
    CHECK(c.endResize(true)); CHECK(c.height("a.audio") == height_presets::compact); CHECK(c.undoView()); CHECK(c.height("a.audio") == 176);
    CHECK(c.redoView()); CHECK(c.height("a.audio") == height_presets::compact);
    std::mt19937 random(8173);
    for (int iteration = 0; iteration < 400; ++iteration)
    {
        const double y = static_cast<int>(random() % 1800) - 200;
        const double h = 1 + random() % 700;
        const auto visible = c.visible(y,h);
        std::vector<std::size_t> expected;
        for (std::size_t i=0;i<c.rows().size();++i)
            if (c.rows()[i].bottom() > y && c.rows()[i].top < y+h) expected.push_back(i);
        CHECK(visible.second-visible.first == expected.size());
        if (!expected.empty()) { CHECK(visible.first == expected.front()); CHECK(visible.second == expected.back()+1); }
    }
    CHECK(c.visible(0,0) == std::pair<std::size_t,std::size_t>(0,0));
    for (const auto& r:c.rows()) if (!r.lane.empty())
    { CHECK(c.resizeHit(r.bottom()) == r.lane); CHECK(c.resizeHit(r.bottom()-2) == r.lane); }
    auto s = fixture(); CHECK(c.beginResize("a.midi")); s.lanes.erase(s.lanes.begin()+1);
    std::erase_if(s.events,[](const auto& e){return e.ref.lane=="a.midi";}); s.revision=2;
    CHECK(c.adopt(s)); CHECK(!c.resizing()); CHECK(!c.lane("a.midi"));
}
void commands()
{
    Controller c; CHECK(c.adopt(fixture()));
    CHECK(!c.control("a.audio",CommandKind::mute,true)); // no host is not success
    std::vector<Command> commands;
    c.onCommand = [&](const Command& command){commands.push_back(command); return Result{};};
    CHECK(c.control("a.audio",CommandKind::monitoring,false,Monitoring::live));
    CHECK(c.control("a.midi",CommandKind::monitoring,false,Monitoring::live));
    CHECK(commands[0].owner == "a" && commands[1].owner == "a"); CHECK(commands[0].revision == 1);
    CHECK(!c.control("main.audio",CommandKind::solo,true)); CHECK(!c.control("tempo",CommandKind::mute,true));
    CHECK(c.edit(CommandKind::glueClips,{{"a.audio","c1"},{"a.audio","c2"}}));
    CHECK(!c.edit(CommandKind::glueClips,{{"a.audio","c1"},{"a.midi","c3"}}));
    CHECK(c.edit(CommandKind::linkClips,{{"a.audio","c1"},{"a.midi","c3"}}));
    CHECK(!c.edit(CommandKind::linkClips,{{"a.audio","c1"},{"a.audio","c2"}}));
    CHECK(!c.edit(CommandKind::moveClips,{{"a.audio","c1"},{"a.audio","c1"}},1));
    CHECK(!c.edit(CommandKind::moveClips,{{"a.audio","missing"}},1));
    CHECK(!c.edit(CommandKind::openOwner,{{"a.audio","c1"}}));
    CHECK(c.adjacentEvent("a.audio",0,true) == 4); CHECK(c.adjacentEvent("a.audio",4,false) == 0);
    CHECK(!c.adjacentEvent("a.audio",4,true)); CHECK(!c.adjacentEvent("a.audio",0,false));
    CHECK(c.adjacentOwnerEvent("performer",0,true)==4);
    CHECK(c.adjacentOwnerEvent("a",4,false)==0);
    CHECK(!c.adjacentOwnerEvent("mix",0,true));
    CHECK(!c.controlOwner("mix",CommandKind::mute,true,Monitoring::automatic,"a.audio"));
    CHECK(c.controlOwner("mix",CommandKind::openOwner)); CHECK(commands.back().owner=="mix");
    CHECK(!c.controlOwner("mix",CommandKind::monitoring,false,static_cast<Monitoring>(99)));
    CHECK(!c.adjacentEvent("missing",0,true)); CHECK(!c.adjacentEvent("a.audio",NAN,true));
    auto s=fixture(); s.events[0].locked=true; ++s.revision; CHECK(c.adopt(s));
    CHECK(!c.edit(CommandKind::moveClips,{{"a.audio","c1"}},1));
    s.revision=0; CHECK(!c.adopt(s));
    s.session="other-song"; CHECK(c.adopt(s)); CHECK(c.view().selectedLanes.empty()); CHECK(!c.undoView());
}
void largeHierarchy()
{
    Snapshot s; s.session="deep";
    for(int i=0;i<10000;++i) s.owners.push_back({std::to_string(i),i?std::to_string(i-1):"","Owner"});
    s.lanes.push_back({"deep-lane","9999",{},"Audio"});
    Controller c; CHECK(c.adopt(s)); CHECK(c.rows().size()==10000);
    CHECK(c.rows().back().depth==9999); CHECK(c.visible(c.totalHeight()-10,10).second==10000);
}
void noHeaderPolicies()
{
    auto s=fixture();s.lanes.back().heightPolicy=HeightPolicy::compact;
    s.lanes.back().minimumHeight=16;s.lanes.back().initialHeight=24;
    Controller c;CHECK(c.adopt(s));CHECK(c.setMode(Mode::freeLanes));CHECK(c.setChrome(ayra::timeline::TimelineChromeVisibility{}));
    double sum=0;for(const auto& lane:s.lanes)sum+=c.height(lane.id);
    CHECK(c.totalHeight()==sum);CHECK(Controller::seamHeight==0);
    CHECK(c.height("tempo")==32);CHECK(!c.beginResize("tempo"));CHECK(!c.setHeight("tempo",200));
    const auto oldHeight=c.height("a.audio");CHECK(c.beginResize("a.audio"));
    CHECK(c.updateResize(31));CHECK(c.height("a.audio")==oldHeight+31);c.endResize(false);
    CHECK(c.height("a.audio")==oldHeight);CHECK(c.totalHeight()==sum);
    CHECK(c.setLaneVisible("a.audio",false));CHECK(c.totalHeight()==sum-oldHeight);
    std::vector<Id> visited;c.visitRows(0,10000,0,[&](const Row& r){visited.push_back(r.lane);});
    CHECK(std::find(visited.begin(),visited.end(),"a.audio")==visited.end());
    CHECK(c.setLaneVisible("a.audio",true));CHECK(c.totalHeight()==sum);
    CHECK(!c.setLaneVisible("tempo",false));
    auto chrome=c.view().chrome;chrome.navigation=false;chrome.inputMeters=true;
    CHECK(c.setChrome(chrome));CHECK(c.view().chrome==chrome);CHECK(c.height("tempo")==24);CHECK(c.totalHeight()==sum-8);
    Command captured;c.onCommand=[&](const Command& command){captured=command;return Result{};};
    CHECK(c.rename("a.audio","New name"));CHECK(captured.kind==CommandKind::renameLane);
    CHECK(captured.owner=="a"&&captured.lane=="a.audio"&&captured.label=="New name");
    CHECK(!c.rename("missing","bad"));
    s.lanes[0].heightPolicy=HeightPolicy::automatic;CHECK(c.adopt(s));
    CHECK(c.setAutoHeight("a.audio",81));CHECK(c.height("a.audio")==81);CHECK(!c.beginResize("a.audio"));
}
void chromeHeightFloor()
{
    auto s=fixture();s.lanes[0].minimumHeight=16;s.lanes[0].initialHeight=24;s.lanes[0].maximumHeight=40;
    Controller c;CHECK(c.adopt(s));CHECK(c.setMode(Mode::freeLanes));CHECK(c.setChrome(ayra::timeline::TimelineChromeVisibility{}));
    CHECK(c.height("a.audio")==56);CHECK(c.snapshot().lanes[0].initialHeight==24);
    CHECK(c.beginResize("a.audio"));CHECK(c.updateResize(-10000));CHECK(c.height("a.audio")==56);
    CHECK(!c.endResize(true)); // clamped no-op
    auto chrome=c.view().chrome;chrome.navigation=false;chrome.mute=false;chrome.solo=false;
    chrome.monitoring=false;chrome.open=false;
    CHECK(c.setChrome(chrome));CHECK(c.height("a.audio")==24);
    CHECK(c.setHeight("a.audio",16));CHECK(c.height("a.audio")==16);
    chrome.open=true;chrome.monitoring=true;CHECK(c.setChrome(chrome));CHECK(c.height("a.audio")==56);
    CHECK(c.undoView());CHECK(c.height("a.audio")==16);
    CHECK(c.redoView());CHECK(c.height("a.audio")==56);
    s.lanes[0].heightPolicy=HeightPolicy::automatic;++s.revision;CHECK(c.adopt(s));
    CHECK(!c.setAutoHeight("a.audio",18));CHECK(c.height("a.audio")==56);
    CHECK(!c.setAutoHeight("a.audio",18)); // no auto-layout loop under the floor
    chrome.open=false;chrome.monitoring=false;CHECK(c.setChrome(chrome));CHECK(c.height("a.audio")==18);
}

}
int main()
{
    { Controller c;CHECK(c.adopt(fixture()));
      const auto initial=c.view();const auto& v=initial.chrome;
      CHECK(!v.navigation&&!v.mute&&!v.solo&&!v.open&&!v.monitoring);
      CHECK(!v.inputMeters&&!v.outputMeters&&!v.resizeIndicator);
      CHECK(initial.identityNavigation);
      CHECK(c.setIdentityNavigation(false));CHECK(!c.view().identityNavigation);
      CHECK(c.undoView());CHECK(c.view().identityNavigation);CHECK(c.redoView());
      CHECK(!c.view().identityNavigation);
      auto snap=c.snapshot();snap.owners[0].status={true,true,true,true,true,true,true};
      ++snap.revision;CHECK(c.adopt(snap));
      const auto active=activeTrackIndicators(c.snapshot().owners[0].status);
      CHECK(std::count(active.begin(),active.end(),true)==7);
    }
    noHeaderPolicies();chromeHeightFloor();
    validation(); layout(); resizeAndViewport(); commands(); largeHierarchy();
    std::cout << "PASS " << checks << " checks (presentation only; no native GUI claim)\n";
}
