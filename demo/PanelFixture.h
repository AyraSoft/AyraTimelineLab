#pragma once
// The ONLY fixture adapter. Never promote this synthetic document/Undo/media policy into Multiverse.
#include "../core/LabModel.h"
#include "../modules/ayra_timeline_panel/TimelineModel.h"
#include <array>
#include <memory>

namespace ayra::lab
{
namespace panel = ayra::timeline_panel;
struct PanelFixtureMetadata
{
    std::vector<panel::Owner> owners;
    std::vector<panel::Lane> lanes;
    std::vector<panel::Relation> relations;
    std::set<panel::EventRef> locked;
    // Defaults are fixtures, NOT a replacement for the product's settings colour resolver.
    std::array<std::uint32_t, 8> streamColours {
        0xff16c95c, 0xffe34b4b, 0xffdbaa34, 0xff925de0,
        0xff548fda, 0xff00beb2, 0xffc5c5c5, 0xff999999 };
    bool operator==(const PanelFixtureMetadata&) const = default;
};
inline const char* monitoringName(panel::Monitoring value)
{
    switch (value) {
        case panel::Monitoring::automatic: return "Auto";
        case panel::Monitoring::live: return "Live";
        case panel::Monitoring::playback: return "Playback";
        case panel::Monitoring::merge: return "Merge";
    }
    return "?";
}
inline Clip* fixtureClip(Session& session, const panel::EventRef& ref)
{
    auto* lane = findTrack(session,ref.lane); if (!lane) return nullptr;
    const auto found = std::find_if(lane->clips.begin(),lane->clips.end(),[&](const auto& c){return c.id==ref.clip;});
    return found == lane->clips.end() ? nullptr : &*found;
}
inline const Clip* fixtureClip(const Session& session, const panel::EventRef& ref)
{
    const auto* lane = findTrack(session,ref.lane); if (!lane) return nullptr;
    const auto found = std::find_if(lane->clips.begin(),lane->clips.end(),[&](const auto& c){return c.id==ref.clip;});
    return found == lane->clips.end() ? nullptr : &*found;
}
inline panel::Snapshot panelSnapshot(const Session& session, std::uint64_t revision)
{
    panel::Snapshot s; s.session=session.songId; s.revision=revision;
    if (!session.panelMetadata) return s;
    const auto& meta=*session.panelMetadata;
    s.owners=meta.owners; s.lanes=meta.lanes; s.relations=meta.relations;
    for (auto& lane:s.lanes)
        if (const auto* source=findTrack(session,lane.id))
        {
            if(lane.heightPolicy==panel::HeightPolicy::manual)lane.initialHeight=std::clamp(source->height,lane.minimumHeight,lane.maximumHeight);
            for (const auto& clip:source->clips)
            {
                panel::EventRef ref{lane.id,clip.id};
                s.events.push_back({ref,clip.start,clip.start+clip.length,meta.locked.contains(ref)});
            }
        }
    return s;
}
inline Session makePanelFixture()
{
    Session s; s.songId="panel-showcase-song"; s.mode=Mode::free;
    auto m=std::make_shared<PanelFixtureMetadata>();
    using R=panel::OwnerRole; using M=panel::Monitoring;
    m->owners={
        {"song",{},"Northern Lights",0xffd4ae48,R::song,false,false,M::automatic,{false,false,false,false}},
        {"globals",{},"Song events",0xff858b96,R::global,false,false,M::automatic,{false,false,false,false}},
        {"backing","song","Backing",0xff84ab60},
        {"p-a","song","Ayra",0xff43c7bb,R::performer,false,false,M::automatic,{false,false,false,false}},
        {"voice","p-a","Voice",0xff43c7bb},
        {"keys","p-a","Keys",0xff5b9bbd},
        {"mix-a","p-a","Ayra MIX",0xff43c7bb,R::convergence},
        {"p-b","song","Noah",0xffbb86c7,R::performer,false,false,M::automatic,{false,false,false,false}},
        {"guitar","p-b","Guitar",0xffbb86c7},
        {"lights","p-b","Lights",0xffc28d61},
        {"mix-b","p-b","Noah MIX",0xffbb86c7,R::convergence},
        {"song-main","song","Song Main",0xffd4ae48,R::songMain,false,false,M::automatic,{true,false,true,true}}
    };
    // Explicit synthetic readbacks demonstrate the passive state indicators.
    // They do not imply that the sandbox owns a runtime/monitoring engine.
    m->owners[2].muted=true; // Backing: yellow M
    m->owners[4].status.canvasActive=true;
    m->owners[4].status.monitoringActive=true; // Auto policy may actually be live
    m->owners[5].soloed=true; // Keys: red S
    m->owners[5].status.bufferized=true;m->owners[5].status.oversampled=true;
    m->owners[5].status.latencyCompensated=true;
    m->owners[8].status.shutdown=true;m->owners[8].status.bypassed=true;
    using S=panel::Stream; using V=panel::VisualKind;
    m->lanes={
        {"range","globals",{},"Range",S::global,V::span,28,24,1200,true},
        {"tempo","globals",{},"Tempo",S::global,V::continuous,96,48,1200,true},
        {"markers","globals",{},"Markers",S::global,V::point,24,24,1200,true},
        {"timesig","globals",{},"Metrica",S::global,V::point,24,24,1200,true},
        {"counter","globals",{},"Counter",S::global,V::point,24,24,1200,true},
        {"backing.audio","backing","backing.bus","Audio",S::audio,V::audio},
        {"voice.audio","voice","voice.audio-bus","Audio",S::audio,V::audio},
        {"voice.midi","voice","voice.midi-bus","MIDI",S::midi,V::midi,256,96,1200},
        {"voice.image","voice","voice.image-bus","Image",S::image,V::image},
        {"keys.midi","keys","keys.midi-bus","MIDI",S::midi,V::midi,96,96,1200},
        {"keys.audio","keys","keys.audio-bus","Audio",S::audio,V::audio},
        {"keys.automation","keys","keys.param-bus","Parameter",S::parameter,V::line},
        {"mix-a.automation","mix-a","mix-a.param-bus","Automation",S::parameter,V::continuous,96,48,1200},
        {"guitar.audio","guitar","guitar.audio-bus","Audio",S::audio,V::audio},
        {"guitar.midi","guitar","guitar.midi-bus","MIDI",S::midi,V::midi,400,96,1200},
        {"lights.dmx","lights","lights.dmx-bus","DMX",S::dmx,V::span,96,48,1200},
        {"lights.osc","lights","lights.osc-bus","OSC",S::osc,V::point,64,48,1200},
        {"lights.text","lights","lights.text-bus","Text draft",S::text,V::span,64,48,1200},
        {"song-main.audio","song-main","main.audio-bus","Audio",S::audio,V::audio}
    };
    for(auto& lane:m->lanes) {
        if(lane.visual==V::point||lane.visual==V::span) {
            lane.heightPolicy=panel::HeightPolicy::automatic;
            lane.initialHeight=lane.minimumHeight=lane.visual==V::point?24:32;
            lane.maximumHeight=10000;
        }
        if(lane.stream!=S::global){lane.inputMeterCount=2;lane.outputMeterCount=2;}
    }
    // Legacy canvases consume one Lab Track per lane. These are explicit compatibility
    // rows, not new musical owners. Only metadata.owners carries M/S/monitoring authority.
    Track hidden; hidden.id="legacy-main-sentinel"; hidden.name="Legacy fixture sentinel";
    hidden.group="song"; hidden.groupName="Song"; hidden.route="Program"; hidden.monitor="Auto";
    hidden.role=Role::songMain; hidden.audioChannels=2; hidden.order={0,1};
    s.tracks.push_back(hidden); s.manualOrder.push_back(hidden.id);
    int seed=5;
    for (const auto& lane:m->lanes)
    {
        Track t; t.id=lane.id; t.name=lane.name; t.group=lane.owner; t.groupName=lane.owner;
        t.route="Fixture only"; t.monitor="Auto"; t.audioChannels=2; t.order={0,1};
        t.height=std::max(minimumHeight(t),lane.initialHeight); t.colour=m->streamColours[static_cast<std::size_t>(lane.stream)];
        // The empty keys/audio lane explicitly demonstrates control and navigation availability.
        if (lane.id!="keys.audio")
        {
            t.clips.push_back({lane.id+".one", "", 2, 6, 2, seed++, 0.5, 0.5});
            t.clips.push_back({lane.id+".two", "", 10, 5, 2, seed++, 0.25, 0.5});
            t.clips.push_back({lane.id+".three", "", 18, 8, 2, seed++, 0.5, 1});
        }
        s.manualOrder.push_back(t.id); s.tracks.push_back(std::move(t));
    }
    m->relations={
        {"temporal-example",panel::RelationKind::temporalGroup,
            {{"backing.audio","backing.audio.one"},{"backing.audio","backing.audio.two"}}},
        {"vertical-example",panel::RelationKind::clipLink,
            {{"voice.audio","voice.audio.one"},{"voice.midi","voice.midi.one"},{"voice.image","voice.image.one"}}}
    };
    s.panelMetadata=std::move(m);
    return s;
}

// One hook before the EXISTING Store commit: linked geometry is admitted together,
// and the Store's existing Undo snapshot includes the immutable relation metadata.
inline std::string preparePanelFixture(const Session& before, Session& candidate)
{
    if (!candidate.panelMetadata) return "Timeline showcase metadata missing; legacy JSON cannot replace this document";
    if (before.panelMetadata)
    {
        struct Change { double start=0, end=0; bool removed=false; };
        std::map<panel::EventRef, Change> changed;
        const auto previous=panelSnapshot(before,0);
        std::map<panel::EventRef,std::vector<panel::EventRef>> adjacency;
        for (const auto& r:before.panelMetadata->relations)
            for (const auto& ref:r.members)
                adjacency[ref].insert(adjacency[ref].end(),r.members.begin(),r.members.end());
        for (const auto& event:previous.events)
        {
            const auto* now=fixtureClip(candidate,event.ref);
            if (!now) changed[event.ref]={0,0,true};
            else
            {
                const double ds=now->start-event.start, de=now->start+now->length-event.end;
                if (std::abs(ds)>1e-9 || std::abs(de)>1e-9) changed[event.ref]={ds,de,false};
            }
        }
        std::set<panel::EventRef> handled;
        for (const auto& [origin, delta]:changed)
        {
            if (handled.contains(origin)) continue;
            std::vector<panel::EventRef> component{origin}; handled.insert(origin);
            for (std::size_t at=0;at<component.size();++at)
                for (const auto& linked:adjacency[component[at]])
                    if (handled.insert(linked).second) component.push_back(linked);
            for (const auto& ref:component)
            {
                if (before.panelMetadata->locked.contains(ref)) return "Locked linked clip: entire edit rejected";
                const auto other=changed.find(ref);
                if (other!=changed.end() && (other->second.removed!=delta.removed
                    || std::abs(other->second.start-delta.start)>1e-8 || std::abs(other->second.end-delta.end)>1e-8))
                    return "Conflicting edits inside one relation: entire edit rejected";
                const auto* old=fixtureClip(before,ref); if (!old) return "Stale relation member";
                auto* target=fixtureClip(candidate,ref);
                if (delta.removed)
                {
                    if (auto* lane=findTrack(candidate,ref.lane))
                        std::erase_if(lane->clips,[&](const Clip& c){return c.id==ref.clip;});
                    if (candidate.selectedClip==ref.clip) candidate.selectedClip.clear();
                }
                else if (target && other==changed.end())
                {
                    target->start=old->start+delta.start;
                    target->length=old->length+delta.end-delta.start;
                }
            }
        }
        // Do not silently turn a split on one linked clip into unrelated trims elsewhere.
        if (!changed.empty())
            for (const auto& lane:candidate.tracks)
                for (const auto& clip:lane.clips)
                    if (!fixtureClip(before,{lane.id,clip.id}))
                        for (const auto& [ref, delta]:changed)
                            if (ref.lane==lane.id && adjacency.contains(ref) && !adjacency[ref].empty()
                                && !delta.removed)
                                return "Detach the relation before split/duplicate; relation-aware split is not implemented by this fixture";
    }
    auto metadata=std::make_shared<PanelFixtureMetadata>(*candidate.panelMetadata);
    for (auto& relation:metadata->relations)
        std::erase_if(relation.members,[&](const auto& ref){return !fixtureClip(candidate,ref);});
    std::erase_if(metadata->relations,[](const auto& r){return r.members.size()<2;});
    std::erase_if(metadata->locked,[&](const auto& ref){return !fixtureClip(candidate,ref);});
    if (*metadata!=*candidate.panelMetadata) candidate.panelMetadata=metadata;
    const auto projected=panelSnapshot(candidate,0);
    if (const auto valid=panel::validate(projected); !valid) return valid.message;
    std::unordered_map<panel::Id,const panel::Owner*> owners;
    for (const auto& owner:candidate.panelMetadata->owners) owners[owner.id]=&owner;
    for (const auto& lane:candidate.panelMetadata->lanes)
    {
        auto* legacy=findTrack(candidate,lane.id); if (!legacy) return "Missing legacy canvas binding";
        const auto* owner=owners.at(lane.owner);
        legacy->mute=owner->muted; legacy->solo=owner->soloed; legacy->monitor=monitoringName(owner->monitoring);
        const auto index=static_cast<std::size_t>(lane.stream);
        if (index>=candidate.panelMetadata->streamColours.size()) return "Unknown stream colour scope";
        legacy->colour=candidate.panelMetadata->streamColours[index];
    }
    return {};
}

class PanelFixtureHost
{
public:
    explicit PanelFixtureHost(Store& store) : model(store), previousChanged(store.changed), previousPrepare(store.prepareCommit)
    {
        model.prepareCommit=[this](const Session& a,Session& b)
        {
            if (previousPrepare) { const auto e=previousPrepare(a,b); if (!e.empty()) return e; }
            return preparePanelFixture(a,b);
        };
        model.changed=[this]
        {
            ++generation;
            if (previousChanged) previousChanged();
            if (onChanged) onChanged();
        };
    }
    ~PanelFixtureHost() { model.changed=std::move(previousChanged); model.prepareCommit=std::move(previousPrepare); }
    PanelFixtureHost(const PanelFixtureHost&)=delete;
    PanelFixtureHost& operator=(const PanelFixtureHost&)=delete;
    std::function<void()> onChanged;
    std::function<void(const panel::Id&)> onOpenOwner;
    panel::Snapshot snapshot() const { return panelSnapshot(model.state(),generation); }
    panel::Result apply(const panel::Command& c)
    {
        if (c.session!=model.state().songId || c.revision!=generation) return panel::Result::fail("Stale command revision");
        if (!model.state().panelMetadata) return panel::Result::fail("No fixture document");
        auto next=model.state(); auto meta=std::make_shared<PanelFixtureMetadata>(*next.panelMetadata);
        using K=panel::CommandKind;
        if(c.kind==K::renameOwner) {
            const auto owner=std::find_if(meta->owners.begin(),meta->owners.end(),[&](const auto& v){return v.id==c.owner;});
            if(owner==meta->owners.end()||owner->role==panel::OwnerRole::global||c.label.empty()||c.label.size()>256
               ||std::any_of(c.label.begin(),c.label.end(),[](unsigned char ch){return ch<32;}))
                return panel::Result::fail("Invalid owner rename");
            owner->name=c.label;
        }
        else if(c.kind==K::renameLane) {
            const auto lane=std::find_if(meta->lanes.begin(),meta->lanes.end(),[&](const auto& v){return v.id==c.lane;});
            if(lane==meta->lanes.end()||lane->owner!=c.owner||c.label.empty()||c.label.size()>256
               ||std::any_of(c.label.begin(),c.label.end(),[](unsigned char ch){return ch<32;}))
                return panel::Result::fail("Invalid lane rename");
            lane->name=c.label;if(auto* t=findTrack(next,c.lane))t->name=c.label;
        }
        else if (c.kind==K::openOwner || c.kind==K::mute || c.kind==K::solo || c.kind==K::monitoring)
        {
            auto owner=std::find_if(meta->owners.begin(),meta->owners.end(),[&](const auto& o){return o.id==c.owner;});
            if (owner==meta->owners.end()) return panel::Result::fail("Unknown owner");
            if (c.kind==K::openOwner)
            {
                if (!owner->capabilities.open || !onOpenOwner) return panel::Result::fail("No owner editor provider");
                onOpenOwner(c.owner); return {};
            }
            if (c.kind==K::mute)
            { if (!owner->capabilities.mute) return panel::Result::fail("Mute unavailable"); owner->muted=c.value; if(c.value)owner->soloed=false; }
            if (c.kind==K::solo)
            { if (!owner->capabilities.solo) return panel::Result::fail("Solo unavailable"); owner->soloed=c.value; if(c.value)owner->muted=false; }
            if (c.kind==K::monitoring)
            { if (!owner->capabilities.monitoring) return panel::Result::fail("Monitoring unavailable"); owner->monitoring=c.monitoring; }
        }
        else
        {
            if (c.events.empty() || !std::isfinite(c.deltaSeconds)) return panel::Result::fail("Invalid event command");
            std::set<panel::EventRef> unique;
            std::set<panel::Id> lanes;
            for (const auto& ref:c.events)
            {
                if (!fixtureClip(next,ref) || !unique.insert(ref).second) return panel::Result::fail("Invalid event selection");
                if (meta->locked.contains(ref)) return panel::Result::fail("Locked event");
                lanes.insert(ref.lane);
            }
            if (c.kind==K::temporalGroup || c.kind==K::linkClips)
            {
                const auto kind=c.kind==K::temporalGroup ? panel::RelationKind::temporalGroup : panel::RelationKind::clipLink;
                if (unique.size()<2 || (kind==panel::RelationKind::temporalGroup ? lanes.size()!=1 : lanes.size()<2))
                    return panel::Result::fail("Invalid relation scope");
                panel::Id id;
                do { id="relation-"+std::to_string(++nextRelation); }
                while (std::any_of(meta->relations.begin(),meta->relations.end(),[&](const auto& r){return r.id==id;}));
                meta->relations.push_back({id,kind,c.events});
            }
            else if (c.kind==K::dissolveGroup || c.kind==K::unlinkClips)
            {
                const auto kind=c.kind==K::dissolveGroup ? panel::RelationKind::temporalGroup : panel::RelationKind::clipLink;
                std::erase_if(meta->relations,[&](const auto& r)
                { return r.kind==kind && std::any_of(r.members.begin(),r.members.end(),[&](const auto& ref){return unique.contains(ref);}); });
            }
            else
            {
                for (const auto& ref:c.events)
                {
                    auto* clip=fixtureClip(next,ref);
                    if (c.kind==K::moveClips) clip->start+=c.deltaSeconds;
                    else if(c.kind==K::trimLeft) { clip->start+=c.deltaSeconds; clip->length-=c.deltaSeconds; }
                    else if(c.kind==K::trimRight) clip->length+=c.deltaSeconds;
                    else if(c.kind==K::deleteClips)
                    {
                        auto* lane=findTrack(next,ref.lane);
                        std::erase_if(lane->clips,[&](const Clip& value){return value.id==ref.clip;});
                        if (next.selectedClip==ref.clip) next.selectedClip.clear();
                    }
                    else return panel::Result::fail("Unsupported fixture command");
                }
            }
        }
        if (*meta!=*next.panelMetadata) next.panelMetadata=std::move(meta);
        const bool committed=model.commit(std::move(next),"Timeline panel fixture edit");
        return committed || model.lastError().empty() ? panel::Result{} : panel::Result::fail(model.lastError());
    }
    panel::Result setStreamColour(panel::Stream stream, std::uint32_t argb)
    {
        auto next=model.state(); if (!next.panelMetadata) return panel::Result::fail("No fixture metadata");
        const auto index=static_cast<std::size_t>(stream);
        if (index>=next.panelMetadata->streamColours.size()) return panel::Result::fail("Unknown stream");
        auto meta=std::make_shared<PanelFixtureMetadata>(*next.panelMetadata);
        meta->streamColours[index]=argb | 0xff000000u;
        if (*meta==*next.panelMetadata) return {};
        next.panelMetadata=std::move(meta);
        return model.commit(std::move(next),"Stream colour fixture setting") ? panel::Result{} : panel::Result::fail(model.lastError());
    }
private:
    Store& model;
    std::function<void()> previousChanged;
    std::function<std::string(const Session&,Session&)> previousPrepare;
    std::uint64_t generation=1, nextRelation=0;
};
}
