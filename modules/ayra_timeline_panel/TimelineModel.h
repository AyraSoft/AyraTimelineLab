#pragma once
// Portable, message-thread presentation contracts. No JUCE, DSP, media or document writes.
// Owner IDs, lane IDs and clip IDs are opaque canonical identities, never display indices.
#include <ayra_timeline/ayra_timeline_presentation.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ayra::timeline_panel
{
using Id = std::string;
enum class Mode { structured, freeLanes };
enum class Stream { audio, midi, dmx, osc, image, parameter, text, global };
enum class VisualKind { audio, midi, line, point, span, continuous, image };
enum class OwnerRole { song, performer, track, convergence, songMain, global };
enum class Monitoring { automatic, live, playback, merge };
enum class HeightPolicy { manual, compact, automatic };
enum class RelationKind { temporalGroup, clipLink };
struct Capabilities
{
    bool mute = true, solo = true, monitoring = true, open = true;
    bool operator==(const Capabilities&) const = default;
};
struct Owner
{
    Id id, parent;
    std::string name;
    std::uint32_t colour = 0xff808080u;
    OwnerRole role = OwnerRole::track;
    bool muted = false, soloed = false;
    Monitoring monitoring = Monitoring::automatic;
    Capabilities capabilities{};
    bool operator==(const Owner&) const = default;
};
struct Lane
{
    Id id, owner, bus;
    std::string name;
    Stream stream = Stream::audio;
    VisualKind visual = VisualKind::audio;
    int initialHeight = 176, minimumHeight = 64, maximumHeight = 1200;
    bool pinned = false;
    HeightPolicy heightPolicy = HeightPolicy::manual;
    bool navigation = true;
    int inputMeterCount = 0, outputMeterCount = 0;
    bool operator==(const Lane&) const = default;
};
struct EventRef
{
    Id lane, clip;
    bool operator==(const EventRef&) const = default;
    bool operator<(const EventRef& b) const { return std::tie(lane, clip) < std::tie(b.lane, b.clip); }
};
struct Event
{
    EventRef ref;
    double start = 0.0, end = 1.0;
    bool locked = false;
    bool operator==(const Event&) const = default;
};
struct Relation
{
    Id id;
    RelationKind kind = RelationKind::clipLink;
    std::vector<EventRef> members;
    bool operator==(const Relation&) const = default;
};
struct Snapshot
{
    Id session;
    std::uint64_t revision = 0;
    std::vector<Owner> owners;
    std::vector<Lane> lanes;
    std::vector<Event> events;
    std::vector<Relation> relations;
};
struct Result
{
    bool ok = true;
    std::string message;
    static Result fail(std::string text) { return {false, std::move(text)}; }
    explicit operator bool() const noexcept { return ok; }
};
inline Result validate(const Snapshot& s)
{
    if (s.session.empty()) return Result::fail("Missing session identity");
    if (s.owners.size() > 100000 || s.lanes.size() > 100000 || s.events.size() > 1000000)
        return Result::fail("Presentation safety budget exceeded");
    const auto enumInRange = [](auto value, auto last) {
        return static_cast<int>(value) >= 0 && static_cast<int>(value) <= static_cast<int>(last);
    };
    std::unordered_map<Id, const Owner*> owners;
    std::unordered_map<Id, const Lane*> lanes;
    std::set<EventRef> events;
    for (const auto& o : s.owners)
    {
        if (!enumInRange(o.role, OwnerRole::global) || !enumInRange(o.monitoring, Monitoring::merge))
            return Result::fail("Unknown owner role or monitoring policy");
        if (o.id.empty() || !owners.emplace(o.id, &o).second) return Result::fail("Duplicate owner identity");
        if (o.soloed && !o.capabilities.solo) return Result::fail("Solo unavailable for owner");
    }
    // Three-colour walk: bounded linear validation, including disconnected cycles.
    std::unordered_map<Id, unsigned> colour;
    for (const auto& o : s.owners)
    {
        std::vector<Id> path;
        Id at = o.id;
        while (!at.empty() && colour[at] != 2)
        {
            if (!owners.contains(at)) return Result::fail("Unknown parent owner");
            if (colour[at] == 1) return Result::fail("Cyclic owner hierarchy");
            colour[at] = 1; path.push_back(at); at = owners.at(at)->parent;
        }
        for (const auto& id : path) colour[id] = 2;
    }
    for (const auto& l : s.lanes)
    {
        if (!enumInRange(l.stream, Stream::global) || !enumInRange(l.visual, VisualKind::image))
            return Result::fail("Unknown lane stream or visual kind");
        if (l.id.empty() || !lanes.emplace(l.id, &l).second || !owners.contains(l.owner))
            return Result::fail("Invalid lane identity or owner");
        if (!enumInRange(l.heightPolicy, HeightPolicy::automatic)
            || l.inputMeterCount < 0 || l.inputMeterCount > 64
            || l.outputMeterCount < 0 || l.outputMeterCount > 64)
            return Result::fail("Invalid height policy or meter descriptor");
        if (l.minimumHeight < 16 || l.maximumHeight > 10000 || l.minimumHeight > l.maximumHeight
            || l.initialHeight < l.minimumHeight || l.initialHeight > l.maximumHeight)
            return Result::fail("Invalid lane height limits");
    }
    for (const auto& e : s.events)
        if (e.ref.clip.empty() || !lanes.contains(e.ref.lane) || !events.insert(e.ref).second
            || !std::isfinite(e.start) || !std::isfinite(e.end) || e.start < 0 || e.end < e.start)
            return Result::fail("Invalid event identity or time range");
    std::set<Id> relations;
    std::set<std::pair<RelationKind, EventRef>> memberships;
    for (const auto& r : s.relations)
    {
        if (!enumInRange(r.kind, RelationKind::clipLink)) return Result::fail("Unknown relation kind");
        if (r.id.empty() || !relations.insert(r.id).second || r.members.size() < 2)
            return Result::fail("Invalid relation identity or cardinality");
        std::set<Id> scopes;
        for (const auto& ref : r.members)
        {
            if (!events.contains(ref) || !memberships.emplace(r.kind, ref).second)
                return Result::fail("Missing or multiply related clip");
            scopes.insert(ref.lane);
        }
        if (r.kind == RelationKind::temporalGroup && scopes.size() != 1)
            return Result::fail("Temporal grouping requires one lane");
        if (r.kind == RelationKind::clipLink && scopes.size() < 2)
            return Result::fail("Vertical linking requires distinct lanes");
    }
    return {};
}
struct LayoutGroup
{
    Id id;
    Mode mode = Mode::freeLanes;
    // structured: WHOLE owner IDs; freeLanes: lane IDs. Never clip identities.
    std::vector<Id> members;
    bool operator==(const LayoutGroup&) const = default;
};
struct ViewState
{
    Mode mode = Mode::structured;
    std::vector<Id> freeOrder, ownerOrder;
    std::set<Id> collapsedOwners, selectedLanes, hiddenLanes;
    std::map<Id, int> heights;
    std::vector<LayoutGroup> layoutGroups;
    ayra::timeline::TimelineChromeVisibility chrome;
    Id activeLane, rangeAnchor;
    bool operator==(const ViewState&) const = default;
};
struct Row
{
    Id key, owner, lane;
    int depth = 0;
    double top = 0, height = 0;
    bool summary = false, firstForOwner = false;
    double bottom() const noexcept { return top + height; }
};
enum class CommandKind { mute, solo, monitoring, openOwner, temporalGroup, dissolveGroup,
                         linkClips, unlinkClips, moveClips, trimLeft, trimRight, deleteClips, renameLane, renameOwner };
struct Command
{
    CommandKind kind = CommandKind::openOwner;
    Id session, owner, lane;
    std::uint64_t revision = 0;
    bool value = false;
    Monitoring monitoring = Monitoring::automatic;
    std::vector<EventRef> events;
    double deltaSeconds = 0;
    std::string label;
};
// Semantic requests, not GUI events. The host prepares/admit/publishes one transaction.
using CommandSink = std::function<Result(const Command&)>;

class Controller
{
public:
    static constexpr double seamHeight = 0.0;
    static constexpr double summaryHeight = 24.0;
    const Snapshot& snapshot() const noexcept { return model; }
    const ViewState& view() const noexcept { return state; }
    // Compatibility accessor for inspection/tests; the native viewport uses visitRows.
    const std::vector<Row>& rows() const
    {
        if (offsetsDirty) {
            for (std::size_t i=0;i<layout.size();++i) {
                layout[i].top=static_cast<double>(rowIndex.top(i));
                layout[i].height=rowIndex.height(i);
            }
            offsetsDirty=false;
        }
        return layout;
    }
    double totalHeight() const noexcept { return static_cast<double>(rowIndex.total()); }
    const Row* rowForLane(const Id& id) const
    {
        const auto i=laneRowIndex.find(id);
        return i==laneRowIndex.end()?nullptr:&layout[i->second];
    }
    double laneTop(const Id& id) const
    {
        const auto i=laneRowIndex.find(id);
        return i==laneRowIndex.end()?0.0:static_cast<double>(rowIndex.top(i->second));
    }
    template<class Visitor> void visitRows(std::int64_t top, int extent, int overscan, Visitor&& visitor) const
    {
        rowIndex.visit(top,extent,overscan,[&](std::size_t i,std::int64_t y,int h){
            auto row=layout[i];row.top=static_cast<double>(y);row.height=h;visitor(row);
        });
    }
    std::uint64_t layoutGeneration() const noexcept { return layoutEpoch; }
    bool setChrome(ayra::timeline::TimelineChromeVisibility chrome)
    {
        if (resize) return false;
        auto next=state;next.chrome=chrome;return change(std::move(next));
    }
    bool setLaneVisible(const Id& id,bool visible)
    {
        const auto* l=lane(id);if(!l || l->pinned || resize)return false;
        auto next=state;
        if(visible)next.hiddenLanes.erase(id);else next.hiddenLanes.insert(id);
        return change(std::move(next));
    }
    bool setAutoHeight(const Id& id,int pixels)
    {
        const auto* l=lane(id);if(!l || l->heightPolicy!=HeightPolicy::automatic)return false;
        pixels=std::clamp(pixels,l->minimumHeight,l->maximumHeight);
        const auto before=height(id);
        autoHeights[id]=pixels;
        if(height(id)==before)return false;
        updateIndexedHeight(id);return true;
    }
    Result renameOwner(const Id& id,std::string label)
    {
        if(!owner(id)||label.empty()||label.size()>256)return Result::fail("Invalid owner rename");
        Command c;c.kind=CommandKind::renameOwner;c.owner=id;c.label=std::move(label);
        return dispatch(std::move(c));
    }
    Result rename(const Id& id,std::string label)
    {
        if(!lane(id) || label.empty() || label.size()>256)return Result::fail("Invalid lane name");
        Command c;c.kind=CommandKind::renameLane;c.lane=id;c.owner=lane(id)->owner;c.label=std::move(label);
        return dispatch(std::move(c));
    }
    bool resizing() const noexcept { return resize.has_value(); }
    CommandSink onCommand;

    Result adopt(Snapshot next)
    {
        if (auto valid = validate(next); !valid) return valid;
        const bool newSession = next.session != model.session;
        if (!newSession && next.revision < model.revision) return Result::fail("Stale presentation revision");
        if (newSession) { state = {}; history.clear(); future.clear(); resize.reset(); autoHeights.clear(); }
        model = std::move(next);
        index(); reconcile(); rebuild();
        return {};
    }
    const Owner* owner(const Id& id) const noexcept
    { const auto i = ownerIndex.find(id); return i == ownerIndex.end() ? nullptr : &model.owners[i->second]; }
    const Lane* lane(const Id& id) const noexcept
    { const auto i = laneIndex.find(id); return i == laneIndex.end() ? nullptr : &model.lanes[i->second]; }
    const Event* event(const EventRef& ref) const noexcept
    { const auto i = eventIndex.find(ref); return i == eventIndex.end() ? nullptr : &model.events[i->second]; }
    ayra::timeline::TimelineChromeCapabilities chromeCapabilities(const Lane& l) const noexcept
    {
        ayra::timeline::TimelineChromeCapabilities caps;
        const auto* o=owner(l.owner);
        caps.navigation=l.navigation;
        caps.mute=o&&o->capabilities.mute;caps.solo=o&&o->capabilities.solo;
        caps.open=o&&o->capabilities.open;caps.monitoring=o&&o->capabilities.monitoring;
        caps.resizable=l.heightPolicy==HeightPolicy::manual;
        caps.inputMeterCount=l.inputMeterCount;caps.outputMeterCount=l.outputMeterCount;
        return caps;
    }
    int minimumHeight(const Lane& l) const noexcept
    {return std::max(l.minimumHeight,ayra::timeline::minimumTimelineLaneChromeHeight(chromeCapabilities(l),state.chrome));}
    int maximumHeight(const Lane& l) const noexcept
    {return std::max(l.maximumHeight,minimumHeight(l));}
    int height(const Id& id) const
    {
        const auto* l=lane(id);if(!l)return 0;
        int requested=l->initialHeight;
        if(l->heightPolicy==HeightPolicy::automatic) {
            if(const auto a=autoHeights.find(id);a!=autoHeights.end())requested=a->second;
        } else if(l->heightPolicy==HeightPolicy::manual) {
            if(resize&&resize->lane==id)requested=resize->preview;
            else if(const auto i=state.heights.find(id);i!=state.heights.end())requested=i->second;
        }
        // The chrome floor is presentation-only, including compact/auto lanes.
        // Hiding buttons reveals the original requested height, not a mutated document.
        return std::clamp(requested,minimumHeight(*l),maximumHeight(*l));
    }
    bool setMode(Mode mode)
    {
        if (resize || state.mode == mode || (mode != Mode::structured && mode != Mode::freeLanes)) return false;
        auto next = state; next.mode = mode; return change(std::move(next));
    }
    bool toggleCollapse(const Id& id)
    {
        if (resize || !owner(id)) return false;
        auto next = state;
        if (!next.collapsedOwners.erase(id)) next.collapsedOwners.insert(id);
        return change(std::move(next));
    }
    enum class Selection { replace, toggle, range };
    bool select(const Id& id, Selection how)
    {
        if (!lane(id)) return false;
        std::vector<Id> visible;
        for (const auto& r : layout) if (!r.lane.empty() && !state.hiddenLanes.contains(r.lane)) visible.push_back(r.lane);
        auto at = std::find(visible.begin(), visible.end(), id);
        if (at == visible.end()) return false;
        auto anchor = std::find(visible.begin(), visible.end(), state.rangeAnchor);
        if (how == Selection::range && anchor != visible.end())
        {
            if (anchor > at) std::swap(anchor, at);
            state.selectedLanes = std::set<Id>(anchor, at + 1);
        }
        else if (how == Selection::toggle)
        {
            if (!state.selectedLanes.erase(id)) state.selectedLanes.insert(id);
            state.rangeAnchor = id;
        }
        else { state.selectedLanes = {id}; state.rangeAnchor = id; }
        state.activeLane = id;
        return true;
    }
    Result groupLayout(Id id)
    {
        if (resize || id.empty()) return Result::fail("Cannot group during a gesture");
        std::set<Id> units;
        for (const auto& selected : state.selectedLanes)
        {
            const auto* l = lane(selected);
            if (l && !l->pinned) units.insert(state.mode == Mode::structured ? l->owner : l->id);
        }
        if (units.size() < 2) return Result::fail("Select at least two layout units");
        for (const auto& g : state.layoutGroups)
        {
            if (g.id == id) return Result::fail("Duplicate layout group");
            if (g.mode == state.mode)
                for (const auto& member : g.members)
                    if (units.contains(member)) return Result::fail("Ungroup existing layout membership first");
        }
        auto next = state;
        next.layoutGroups.push_back({std::move(id), state.mode, {units.begin(), units.end()}});
        change(std::move(next)); return {};
    }
    bool ungroupLayout()
    {
        if (resize) return false;
        auto next = state;
        std::erase_if(next.layoutGroups, [&](const auto& g)
        {
            if (g.mode != state.mode) return false;
            return std::any_of(state.selectedLanes.begin(), state.selectedLanes.end(), [&](const auto& id)
            {
                const auto* l = lane(id);
                return l && std::find(g.members.begin(), g.members.end(),
                    state.mode == Mode::structured ? l->owner : id) != g.members.end();
            });
        });
        return change(std::move(next));
    }
    Result moveBefore(const std::vector<Id>& laneIds, const Id& anchorLane)
    {
        if (resize || laneIds.empty()) return Result::fail("No movable selection");
        const auto* anchor = lane(anchorLane);
        if (!anchor || anchor->pinned) return Result::fail("Invalid or pinned destination");
        std::set<Id> moving;
        for (const auto& id : laneIds)
        {
            const auto* l = lane(id);
            if (!l || l->pinned) return Result::fail("Invalid or pinned lane");
            moving.insert(state.mode == Mode::structured ? l->owner : l->id);
        }
        for (const auto& g : state.layoutGroups)
            if (g.mode == state.mode && std::any_of(g.members.begin(), g.members.end(),
                    [&](const Id& id) { return moving.contains(id); }))
                moving.insert(g.members.begin(), g.members.end());
        const Id target = state.mode == Mode::structured ? anchor->owner : anchor->id;
        if (moving.contains(target)) return Result::fail("Destination belongs to moved group");
        if (state.mode == Mode::structured)
        {
            for (const auto& id : moving)
                if (!owner(id) || owner(id)->parent != owner(target)->parent)
                    return Result::fail("Structural reorder cannot reparent tracks");
        }
        else
        {
            for (const auto& id : moving)
                if (!lane(id) || lane(id)->pinned) return Result::fail("Pinned layout group member");
        }
        auto next = state;
        auto& order = state.mode == Mode::structured ? next.ownerOrder : next.freeOrder;
        std::vector<Id> picked, rest;
        for (const auto& id : order) (moving.contains(id) ? picked : rest).push_back(id);
        const auto at = std::find(rest.begin(), rest.end(), target);
        if (at == rest.end()) return Result::fail("Missing destination");
        rest.insert(at, picked.begin(), picked.end()); order = std::move(rest);
        change(std::move(next)); return {};
    }
    bool setHeight(const Id& id, int pixels)
    {
        const auto* l = lane(id); if (!l || resize || l->heightPolicy!=HeightPolicy::manual) return false;
        pixels = std::clamp(pixels, minimumHeight(*l), maximumHeight(*l));
        if (pixels == height(id)) return false;
        auto next = state; next.heights[id] = pixels; return change(std::move(next));
    }
    bool beginResize(const Id& id)
    {
        const auto* l = lane(id);
        if (!l || resize || l->heightPolicy!=HeightPolicy::manual) return false;
        resize = Resize{id, height(id), height(id)}; return true;
    }
    bool updateResize(double delta)
    {
        if (!resize || !std::isfinite(delta)) return false;
        const auto* l = lane(resize->lane); if (!l) { resize.reset(); return false; }
        resize->preview = static_cast<int>(std::clamp(std::round(resize->original + delta),
            static_cast<double>(minimumHeight(*l)), static_cast<double>(maximumHeight(*l))));
        updateIndexedHeight(resize->lane); return true;
    }
    bool endResize(bool commit)
    {
        if (!resize) return false;
        const auto captured = *resize; resize.reset();
        if (commit && captured.preview != captured.original && lane(captured.lane))
        {
            auto next = state; next.heights[captured.lane] = captured.preview;
            const bool changed = change(std::move(next)); rebuild(); return changed;
        }
        rebuild(); return false;
    }
    bool undoView()
    {
        if (resize || history.empty()) return false;
        future.push_back(state); state = std::move(history.back()); history.pop_back();
        reconcile(); rebuild(); return true;
    }
    bool redoView()
    {
        if (resize || future.empty()) return false;
        history.push_back(state); state = std::move(future.back()); future.pop_back();
        reconcile(); rebuild(); return true;
    }
    // Half-open viewport. O(log(rows) + visible) at the consumer; no full row scan.
    std::pair<std::size_t, std::size_t> visible(double top, double extent, double overscan = 0) const
    {
        rows();
        if (!std::isfinite(top) || !std::isfinite(extent) || !std::isfinite(overscan) || extent <= 0)
            return {0, 0};
        overscan = std::max(0.0, overscan);
        const double lo = top - overscan, hi = top + extent + overscan;
        const auto first = std::upper_bound(layout.begin(), layout.end(), lo,
            [](double y, const Row& r) { return y < r.bottom(); });
        const auto last = std::lower_bound(first, layout.end(), hi,
            [](const Row& r, double y) { return r.top < y; });
        return {static_cast<std::size_t>(first - layout.begin()), static_cast<std::size_t>(last - layout.begin())};
    }
    std::optional<Id> resizeHit(double y, double tolerance = 4.0) const
    {
        rows();
        if (!std::isfinite(y) || !std::isfinite(tolerance) || tolerance < 0) return {};
        const auto at = std::lower_bound(layout.begin(), layout.end(), y - tolerance,
            [](const Row& r, double edge) { return r.bottom() < edge; });
        if (at != layout.end() && !at->lane.empty() && lane(at->lane)->heightPolicy==HeightPolicy::manual && std::abs(at->bottom() - y) <= tolerance)
            return at->lane;
        return {};
    }
    std::optional<double> adjacentEvent(const Id& laneId, double at, bool forward) const
    {
        if (!std::isfinite(at)) return {};
        const auto i = eventTimes.find(laneId); if (i == eventTimes.end()) return {};
        const auto& times = i->second;
        constexpr double epsilon = 1.0e-7;
        if (forward)
        {
            const auto found = std::upper_bound(times.begin(), times.end(), at + epsilon);
            if (found != times.end()) return *found;
        }
        else
        {
            auto found = std::lower_bound(times.begin(), times.end(), at - epsilon);
            if (found != times.begin()) return *--found;
        }
        return {};
    }
    std::optional<double> adjacentOwnerEvent(const Id& ownerId, double at, bool forward) const
    {
        if (!owner(ownerId) || !std::isfinite(at)) return {};
        std::unordered_map<Id, std::vector<Id>> children;
        for (const auto& o : model.owners) children[o.parent].push_back(o.id);
        std::vector<Id> pending{ownerId}; std::set<Id> included;
        for (std::size_t i = 0; i < pending.size(); ++i)
            if (included.insert(pending[i]).second)
                pending.insert(pending.end(), children[pending[i]].begin(), children[pending[i]].end());
        std::optional<double> best;
        for (const auto& l : model.lanes) if (included.contains(l.owner))
            if (const auto candidate = adjacentEvent(l.id, at, forward))
                if (!best || (forward ? *candidate < *best : *candidate > *best)) best = candidate;
        return best;
    }
    Result control(const Id& laneId, CommandKind kind, bool value = false,
                   Monitoring monitor = Monitoring::automatic)
    {
        const auto* l = lane(laneId); if (!l) return Result::fail("Unknown lane");
        return controlOwner(l->owner, kind, value, monitor, laneId);
    }
    Result controlOwner(const Id& ownerId, CommandKind kind, bool value = false,
                        Monitoring monitor = Monitoring::automatic, const Id& laneId = {})
    {
        const auto* o = owner(ownerId); if (!o) return Result::fail("Unknown owner");
        if (!laneId.empty() && (!lane(laneId) || lane(laneId)->owner != ownerId))
            return Result::fail("Lane does not belong to control owner");
        if (static_cast<int>(monitor) < 0 || static_cast<int>(monitor) > static_cast<int>(Monitoring::merge))
            return Result::fail("Unknown monitoring policy");
        const bool available = (kind == CommandKind::mute && o->capabilities.mute)
            || (kind == CommandKind::solo && o->capabilities.solo)
            || (kind == CommandKind::monitoring && o->capabilities.monitoring)
            || (kind == CommandKind::openOwner && o->capabilities.open);
        if (!available) return Result::fail("Owner control unavailable");
        Command c; c.kind = kind; c.owner = o->id; c.lane = laneId; c.value = value; c.monitoring = monitor;
        return dispatch(std::move(c));
    }
    Result edit(CommandKind kind, std::vector<EventRef> events, double delta = 0)
    {
        if (events.empty() || !std::isfinite(delta)) return Result::fail("Empty or invalid edit");
        std::set<EventRef> unique;
        std::set<Id> lanes;
        for (const auto& ref : events)
        {
            const auto* e = event(ref);
            if (!e || !unique.insert(ref).second) return Result::fail("Stale or duplicate event selection");
            if (e->locked) return Result::fail("Locked event in selection");
            lanes.insert(ref.lane);
        }
        if (kind == CommandKind::temporalGroup && (events.size() < 2 || lanes.size() != 1))
            return Result::fail("Temporal group requires two clips in one lane");
        if (kind == CommandKind::linkClips && lanes.size() < 2)
            return Result::fail("Vertical link requires clips from distinct lanes");
        switch (kind)
        {
            case CommandKind::temporalGroup: case CommandKind::dissolveGroup:
            case CommandKind::linkClips: case CommandKind::unlinkClips:
            case CommandKind::moveClips: case CommandKind::trimLeft:
            case CommandKind::trimRight: case CommandKind::deleteClips: break;
            default: return Result::fail("Not a content command");
        }
        Command c; c.kind = kind; c.events = std::move(events); c.deltaSeconds = delta;
        return dispatch(std::move(c));
    }
private:
    struct Resize { Id lane; int original, preview; };
    Snapshot model;
    ViewState state;
    std::vector<ViewState> history, future;
    mutable std::vector<Row> layout;
    ayra::timeline::TimelineRowIndex rowIndex;
    std::unordered_map<Id,std::size_t> laneRowIndex;
    std::map<Id,int> autoHeights;
    mutable bool offsetsDirty=false;
    std::uint64_t layoutEpoch=0;
    void updateIndexedHeight(const Id& id)
    {
        const auto i=laneRowIndex.find(id);if(i==laneRowIndex.end())return;
        rowIndex.setHeight(i->second,state.hiddenLanes.contains(id)?0:height(id));
        offsetsDirty=true;++layoutEpoch;
    }
    void finishLayout()
    {
        std::vector<int> heights;heights.reserve(layout.size());laneRowIndex.clear();
        for(std::size_t i=0;i<layout.size();++i) {
            const auto& r=layout[i];
            if(!r.lane.empty())laneRowIndex[r.lane]=i;
            heights.push_back(!r.lane.empty() && state.hiddenLanes.contains(r.lane)?0:static_cast<int>(r.height));
        }
        rowIndex.reset(heights);offsetsDirty=true;++layoutEpoch;
    }
    std::optional<Resize> resize;
    std::unordered_map<Id, std::size_t> ownerIndex, laneIndex;
    std::map<EventRef, std::size_t> eventIndex;
    std::unordered_map<Id, std::vector<double>> eventTimes;
    Result dispatch(Command command)
    {
        if (!onCommand) return Result::fail("No host command provider");
        command.session = model.session; command.revision = model.revision;
        return onCommand(command);
    }
    bool change(ViewState next)
    {
        if (next == state) return false;
        if (history.size() == 96) history.erase(history.begin());
        history.push_back(state); future.clear(); state = std::move(next); rebuild(); return true;
    }
    void index()
    {
        ownerIndex.clear(); laneIndex.clear(); eventIndex.clear(); eventTimes.clear();
        for (std::size_t i = 0; i < model.owners.size(); ++i) ownerIndex[model.owners[i].id] = i;
        for (std::size_t i = 0; i < model.lanes.size(); ++i) laneIndex[model.lanes[i].id] = i;
        for (std::size_t i = 0; i < model.events.size(); ++i)
        { eventIndex[model.events[i].ref] = i; eventTimes[model.events[i].ref.lane].push_back(model.events[i].start); }
        for (auto& [id, times] : eventTimes)
        { (void)id; std::sort(times.begin(), times.end()); times.erase(std::unique(times.begin(), times.end()), times.end()); }
    }
    void reconcile()
    {
        const auto cleanOrder = [](auto& order, const auto& items, const auto& exists)
        {
            std::set<Id> seen;
            std::erase_if(order, [&](const Id& id) { return !exists(id) || !seen.insert(id).second; });
            for (const auto& item : items) if (seen.insert(item.id).second) order.push_back(item.id);
        };
        cleanOrder(state.freeOrder, model.lanes, [&](const Id& id) { return lane(id) != nullptr; });
        cleanOrder(state.ownerOrder, model.owners, [&](const Id& id) { return owner(id) != nullptr; });
        std::erase_if(state.selectedLanes, [&](const Id& id) { return !lane(id); });
        std::erase_if(state.collapsedOwners, [&](const Id& id) { return !owner(id); });
        std::erase_if(state.hiddenLanes, [&](const Id& id) { return !lane(id); });
        std::erase_if(autoHeights, [&](const auto& p) { return !lane(p.first); });
        std::erase_if(state.heights, [&](const auto& pair) { return !lane(pair.first); });
        for (auto& [id, value] : state.heights)
            value = std::clamp(value, lane(id)->minimumHeight, lane(id)->maximumHeight);
        if (!lane(state.activeLane)) state.activeLane.clear();
        if (!lane(state.rangeAnchor)) state.rangeAnchor.clear();
        for (auto& group : state.layoutGroups)
            std::erase_if(group.members, [&](const Id& id)
                { return group.mode == Mode::structured ? !owner(id) : !lane(id); });
        std::erase_if(state.layoutGroups, [](const auto& g) { return g.members.size() < 2; });
        if (resize)
        {
            if (const auto* l = lane(resize->lane))
                resize->preview = std::clamp(resize->preview, minimumHeight(*l), maximumHeight(*l));
            else resize.reset();
        }
    }
    void rebuild()
    {
        layout.clear();
        double y = 0;
        const auto addLane = [&](const Lane& l, int depth, bool first)
        {
            const double h = static_cast<double>(height(l.id)) + seamHeight;
            layout.push_back({"lane:" + l.id, l.owner, l.id, depth, y, h, false, first}); y += h;
        };
        // Pinned global lanes share the SAME musical X transform as every other row.
        for (const auto& l : model.lanes) if (l.pinned) addLane(l, 0, true);
        if (state.mode == Mode::freeLanes)
        {
            for (const auto& id : state.freeOrder) if (const auto* l = lane(id); l && !l->pinned) addLane(*l, 0, true);
            finishLayout();return;
        }
        std::unordered_map<Id, std::vector<Id>> children;
        std::unordered_map<Id, std::vector<const Lane*>> ownerLanes;
        for (const auto& id : state.ownerOrder) children[owner(id)->parent].push_back(id);
        for (const auto& l : model.lanes) if (!l.pinned) ownerLanes[l.owner].push_back(&l);
        struct Pending { Id id; int depth; };
        std::vector<Pending> stack;
        const auto pushChildren = [&](const Id& id, int depth)
        {
            const auto& list = children[id];
            for (auto i = list.rbegin(); i != list.rend(); ++i) stack.push_back({*i, depth});
        };
        pushChildren({}, 0);
        while (!stack.empty())
        {
            const auto current = stack.back(); stack.pop_back();
            const auto* o = owner(current.id);
            if (o->role == OwnerRole::global) continue;
            const bool collapsed = state.collapsedOwners.contains(current.id);
            const auto& lanes = ownerLanes[current.id];
            if (collapsed || lanes.empty())
            {
                layout.push_back({"owner:" + current.id, current.id, {}, current.depth, y, summaryHeight, true, true});
                y += summaryHeight;
            }
            else
                for (std::size_t i = 0; i < lanes.size(); ++i) addLane(*lanes[i], current.depth, i == 0);
            if (!collapsed) pushChildren(current.id, current.depth + 1);
            else
            {
                // Collapsing a structural folder never makes a real convergence
                // or Song Main control owner disappear with its child tracks.
                std::vector<Pending> boundaries;
                const auto append = [&](const Id& id, int depth)
                { for (const auto& child : children[id]) boundaries.push_back({child, depth}); };
                append(current.id, current.depth + 1);
                for (std::size_t i = 0; i < boundaries.size(); ++i)
                {
                    const auto entry = boundaries[i];
                    const auto* child = owner(entry.id);
                    if (child->role == OwnerRole::convergence || child->role == OwnerRole::songMain)
                    {
                        layout.push_back({"owner:" + entry.id, entry.id, {}, entry.depth, y, summaryHeight, true, true});
                        y += summaryHeight;
                    }
                    append(entry.id, entry.depth + 1);
                }
            }
        }
        finishLayout();
    }
};
} // namespace ayra::timeline_panel
