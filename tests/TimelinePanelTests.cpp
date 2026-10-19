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
    bad = s; bad.relations = {{"g",RelationKind::temporalGroup,{{"a.audio","c1"},{"b.audio","c4"}}}}; CHECK(!validate(bad));
    bad = s; bad.relations = {{"l",RelationKind::clipLink,{{"a.audio","c1"},{"a.audio","c2"}}}}; CHECK(!validate(bad));
    s.relations = {{"g",RelationKind::temporalGroup,{{"a.audio","c1"},{"a.audio","c2"}}},
                   {"l",RelationKind::clipLink,{{"a.audio","c1"},{"a.midi","c3"}}}};
    CHECK(validate(s));
    bad = s; bad.relations[1].members.push_back({"a.audio","c1"}); CHECK(!validate(bad));
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
    CHECK(!c.groupLayout("overlap"));
    CHECK(c.moveBefore({"a.audio"},"main.audio"));
    CHECK(laneOrder(c) == std::vector<Id>({"tempo","a.midi","a.audio","b.audio","main.audio"}));
    CHECK(!c.moveBefore({"a.audio"},"b.audio"));
    CHECK(c.setMode(Mode::structured));
    CHECK(c.groupLayout("track-group"));
    CHECK(c.view().layoutGroups.back().members == std::vector<Id>({"a","b"}));
    CHECK(!c.moveBefore({"a.audio"},"main.audio")); // would reparent the tracks
    CHECK(c.ungroupLayout()); CHECK(c.moveBefore({"b.audio"},"a.audio"));
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
    CHECK(c.beginResize("a.audio")); CHECK(c.updateResize(-1000)); CHECK(c.height("a.audio") == 64);
    CHECK(c.endResize(true)); CHECK(c.height("a.audio") == 64); CHECK(c.undoView()); CHECK(c.height("a.audio") == 176);
    CHECK(c.redoView()); CHECK(c.height("a.audio") == 64);
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
    CHECK(c.edit(CommandKind::temporalGroup,{{"a.audio","c1"},{"a.audio","c2"}}));
    CHECK(!c.edit(CommandKind::temporalGroup,{{"a.audio","c1"},{"a.midi","c3"}}));
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
}
int main()
{
    validation(); layout(); resizeAndViewport(); commands(); largeHierarchy();
    std::cout << "PASS " << checks << " checks (presentation only; no native GUI claim)\n";
}
