/** @file LabModel.cpp
    Invarianti del banco: ordine visuale separato dal routing; N canali per bus;
    mapping condiviso, identita' stabili, comandi atomici. @see LabModel.h */
#include "LabModel.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace ayra::lab
{
namespace
{
bool has(const std::vector<std::string>& v, const std::string& x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}
bool validOrder(const std::vector<int>& order, int count)
{
    if (count < 1 || count > 64 || order.size() != static_cast<std::size_t>(count)) return false;
    std::vector<bool> seen(static_cast<std::size_t>(count));
    for (int v : order)
    {
        if (v < 0 || v >= count || seen[static_cast<std::size_t>(v)]) return false;
        seen[static_cast<std::size_t>(v)] = true;
    }
    return true;
}

template <typename ClipVector>
auto* findClipRecursive(ClipVector& clips, const std::string& id) noexcept
{
    for (auto& clip : clips)
    {
        if (clip.id == id) { return &clip; }
        if (auto* nested = findClipRecursive(clip.children, id)) { return nested; }
    }
    using Pointer = decltype(&clips.front());
    return Pointer(nullptr);
}

template <typename TrackType>
auto* findClipScope(TrackType& track, const std::string& parentId) noexcept
{
    using ClipVector = decltype(&track.clips);
    if (parentId.empty()) { return ClipVector(&track.clips); }
    auto* parent = findClipRecursive(track.clips, parentId);
    return parent != nullptr ? ClipVector(&parent->children) : nullptr;
}

bool validateClipTree(const Clip& clip, const Track& track,
                      std::unordered_set<std::string>& ids,
                      std::size_t& total, const int depth)
{
    if (++total > 4096 || depth > 16
        || clip.id.empty() || clip.id.size() > 128 || !ids.insert(clip.id).second
        || clip.name.size() > 256)
        return false;
    if (clip.channels < 1 || clip.channels > track.audioChannels
        || clip.seed < 0 || clip.seed > 100000
        || !std::isfinite(clip.start) || clip.start < 0.0 || clip.start > 3600.0
        || !std::isfinite(clip.length) || clip.length < 0.125 || clip.length > 3600.0
        || clip.start + clip.length > 3600.0)
        return false;
    if (!std::isfinite(clip.fadeIn) || !std::isfinite(clip.fadeOut)
        || clip.fadeIn < 0.0 || clip.fadeOut < 0.0
        || clip.fadeIn + clip.fadeOut > clip.length + 1.0e-9)
        return false;
    if ((!clip.container && !clip.children.empty()) || clip.children.size() > 64)
        return false;
    for (const auto& child : clip.children)
        if (!validateClipTree(child, track, ids, total, depth + 1))
            return false;
    return true;
}
}
const char* roleKey(Role r) noexcept
{
    switch(r) { case Role::songTrack:return "songTrack"; case Role::performerTrack:return "performerTrack";
        case Role::performerMix:return "performerMix"; case Role::songMain:return "songMain"; }
    return "invalid";
}
const char* roleLabel(Role r) noexcept
{
    switch(r) { case Role::songTrack:return "SONG TRACK"; case Role::performerTrack:return "PERFORMER TRACK";
        case Role::performerMix:return "PERFORMER MIX"; case Role::songMain:return "SONG MAIN"; }
    return "INVALID";
}
bool parseRole(const std::string& s, Role& out) noexcept
{
    for (auto r : {Role::songTrack, Role::performerTrack, Role::performerMix, Role::songMain})
        if (s == roleKey(r)) { out = r; return true; }
    return false;
}
const Track* findTrack(const Session& s, const std::string& id) noexcept
{
    auto i = std::find_if(s.tracks.begin(),s.tracks.end(),[&](const auto& t){return t.id==id;});
    return i == s.tracks.end() ? nullptr : &*i;
}
Track* findTrack(Session& s, const std::string& id) noexcept
{
    auto i = std::find_if(s.tracks.begin(),s.tracks.end(),[&](const auto& t){return t.id==id;});
    return i == s.tracks.end() ? nullptr : &*i;
}
const Clip* findClip(const Track& track, const std::string& id) noexcept
{
    return findClipRecursive(track.clips, id);
}
Clip* findClip(Track& track, const std::string& id) noexcept
{
    return findClipRecursive(track.clips, id);
}
const Clip* findClip(const Clip& clip, const std::string& id) noexcept
{
    if (clip.id == id) { return &clip; }
    return findClipRecursive(clip.children, id);
}
Clip* findClip(Clip& clip, const std::string& id) noexcept
{
    if (clip.id == id) { return &clip; }
    return findClipRecursive(clip.children, id);
}
const Clip* findClip(const Session& session, const std::string& trackId,
                     const std::string& clipId) noexcept
{
    const auto* track = findTrack(session, trackId);
    return track != nullptr ? findClip(*track, clipId) : nullptr;
}
Clip* findClip(Session& session, const std::string& trackId,
               const std::string& clipId) noexcept
{
    auto* track = findTrack(session, trackId);
    return track != nullptr ? findClip(*track, clipId) : nullptr;
}
ClipSiblingScope findClipSiblingScope(Track& track, const std::string& clipId) noexcept
{
    const auto search = [&](auto&& self, std::vector<Clip>& siblings,
                            const std::string& parentId) -> ClipSiblingScope
    {
        for (auto& clip:siblings)
        {
            if (clip.id==clipId) { return {&siblings,parentId}; }
            if (auto nested=self(self,clip.children,clip.id); nested.clips!=nullptr)
                return nested;
        }
        return {};
    };
    return search(search,track.clips,{});
}
void collectClipIds(const Clip& clip, std::vector<std::string>& output)
{
    output.push_back(clip.id);
    for (const auto& child : clip.children) { collectClipIds(child, output); }
}

ClipContainerEditResult createClipContainer(
    Track& track, const std::string& parentContainerId,
    const std::vector<std::string>& clipIds, std::string containerId)
{
    auto* siblings = findClipScope(track, parentContainerId);
    if (siblings == nullptr || clipIds.size() < 2 || containerId.empty())
        return {false, {}, "Invalid Glue scope"};
    if (findClip(track, containerId) != nullptr)
        return {false, {}, "Duplicate container identity"};

    std::unordered_set<std::string> requested;
    requested.reserve(clipIds.size());
    for (const auto& id : clipIds)
        if (id.empty() || !requested.insert(id).second)
            return {false, {}, "Duplicate Glue selection"};

    std::vector<std::size_t> indices;
    indices.reserve(requested.size());
    for (std::size_t i = 0; i < siblings->size(); ++i)
        if (requested.contains((*siblings)[i].id)) { indices.push_back(i); }
    if (indices.size() != requested.size())
        return {false, {}, "Glue clips must be direct siblings"};

    double start = std::numeric_limits<double>::max();
    double end = 0.0;
    int channels = 1;
    int seed = 0;
    for (const auto index : indices)
    {
        const auto& clip = (*siblings)[index];
        start = std::min(start, clip.start);
        end = std::max(end, clip.start + clip.length);
        channels = std::max(channels, clip.channels);
        seed = (seed * 131 + clip.seed) % 100001;
    }
    if (!std::isfinite(start) || !std::isfinite(end) || end - start < 0.125)
        return {false, {}, "Invalid Glue range"};

    Clip container;
    container.id = std::move(containerId);
    container.name = "Glue";
    container.start = start;
    container.length = end - start;
    container.channels = channels;
    container.seed = seed;
    container.container = true;
    container.children.reserve(indices.size());
    for (const auto index : indices)
    {
        auto child = (*siblings)[index];
        child.start -= start;
        container.children.push_back(std::move(child));
    }

    const auto insertion = indices.front();
    std::erase_if(*siblings, [&requested](const Clip& clip)
    {
        return requested.contains(clip.id);
    });
    siblings->insert(siblings->begin() + static_cast<std::ptrdiff_t>(insertion),
                     std::move(container));
    return {true, (*siblings)[insertion].id, {}};
}

bool dissolveClipContainer(Track& track, const std::string& containerId)
{
    const auto dissolveIn = [&](auto&& self, std::vector<Clip>& siblings) -> bool
    {
        for (auto at = siblings.begin(); at != siblings.end(); ++at)
        {
            if (at->id == containerId)
            {
                if (!at->isContainer()) { return false; }
                const double parentStart = at->start;
                std::vector<Clip> children = std::move(at->children);
                for (auto& child : children) { child.start += parentStart; }
                at = siblings.erase(at);
                siblings.insert(at, std::make_move_iterator(children.begin()),
                                std::make_move_iterator(children.end()));
                return true;
            }
            if (self(self, at->children)) { return true; }
        }
        return false;
    };
    return dissolveIn(dissolveIn, track.clips);
}

ClipSplitEditResult splitClip(
    Track& track, const std::string& clipId, const double splitTime,
    const std::function<std::string(const std::string&)>& mintId)
{
    constexpr double minimumSegmentLength = 0.2;
    if (!std::isfinite(splitTime) || !mintId)
        return {false, {}, "Invalid split request", {}};

    const auto scope = findClipSiblingScope(track, clipId);
    if (scope.clips == nullptr)
        return {false, {}, "Unknown clip scope", {}};

    auto& siblings = *scope.clips;
    const auto at = std::find_if(siblings.begin(), siblings.end(),
        [&](const Clip& clip) { return clip.id == clipId; });
    if (at == siblings.end())
        return {false, {}, "Unknown clip", {}};

    const double relative = splitTime - at->start;
    if (relative <= minimumSegmentLength
        || relative >= at->length - minimumSegmentLength)
        return {false, {}, "Split outside clip interior", {}};

    ClipSplitEditResult result;
    result.rightClipId = mintId(at->id + "_cut");
    if (result.rightClipId.empty())
        return {false, {}, "Cannot allocate split identity", {}};

    const auto partitionChildren = [&] (auto&& self,
                                        std::vector<Clip> source,
                                        const double cut,
                                        std::vector<Clip>& left,
                                        std::vector<Clip>& right) -> bool
    {
        left.clear();
        right.clear();
        left.reserve(source.size());
        right.reserve(source.size());

        for (auto& child : source)
        {
            const double childStart = child.start;
            const double childEnd = child.start + child.length;
            if (childEnd <= cut + 1.0e-9)
            {
                left.push_back(std::move(child));
                continue;
            }
            if (childStart >= cut - 1.0e-9)
            {
                child.start -= cut;
                right.push_back(std::move(child));
                continue;
            }

            const double childCut = cut - childStart;
            if (childCut <= 0.0 || childCut >= child.length)
                return false;

            Clip leftChild = child;
            Clip rightChild = std::move(child);
            const double originalLength = leftChild.length;
            rightChild.id = mintId(leftChild.id + "_cut");
            if (rightChild.id.empty()) { return false; }

            leftChild.length = childCut;
            leftChild.fadeOut = 0.0;
            rightChild.start = 0.0;
            rightChild.length = originalLength - childCut;
            rightChild.fadeIn = 0.0;
            rightChild.fadeOut = 0.0;

            if (leftChild.isContainer())
            {
                std::vector<Clip> leftNested, rightNested;
                if (!self(self, std::move(leftChild.children), childCut,
                          leftNested, rightNested))
                    return false;
                leftChild.children = std::move(leftNested);
                rightChild.children = std::move(rightNested);
            }

            result.lineage.push_back(
                {leftChild.id, rightChild.id, childCut / originalLength});
            left.push_back(std::move(leftChild));
            right.push_back(std::move(rightChild));
        }
        return true;
    };

    Clip left = *at;
    Clip right = *at;
    const double originalLength = left.length;
    right.id = result.rightClipId;
    left.length = relative;
    left.fadeOut = 0.0;
    right.start = splitTime;
    right.length = originalLength - relative;
    right.fadeIn = 0.0;
    right.fadeOut = 0.0;

    if (left.isContainer())
    {
        std::vector<Clip> leftChildren, rightChildren;
        if (!partitionChildren(partitionChildren, std::move(left.children),
                               relative, leftChildren, rightChildren))
            return {false, {}, "Cannot partition nested arrangement", {}};
        left.children = std::move(leftChildren);
        right.children = std::move(rightChildren);
    }

    result.lineage.insert(result.lineage.begin(),
                          {left.id, right.id, relative / originalLength});
    *at = std::move(left);
    siblings.insert(at + 1, std::move(right));
    result.ok = true;
    return result;
}
Capabilities capabilities(const Track& t, Mode mode) noexcept
{
    Capabilities c;
    c.solo = t.role != Role::songMain;
    c.reorder = mode == Mode::free && !t.pinned;
    return c;
}
int minimumHeight(const Track& t) noexcept { return std::max(48, 16+std::clamp(t.audioChannels,1,64)*8); }
int visualHeight(const Track& t) noexcept
{
    return std::max(minimumHeight(t), t.height) + (t.expanded ? 34*(t.midiPins+t.oscPins+t.dmxPins) + 40 : 0);
}
std::vector<std::string> visibleOrder(const Session& s)
{
    std::vector<std::string> out, pinned;
    const auto add = [&](const Track& t)
    {
        if (t.pinned) { pinned.push_back(t.id); return; }
        if (s.mode == Mode::grouped && s.groupCollapsed && t.role == Role::performerTrack) return;
        out.push_back(t.id);
    };
    if (s.mode == Mode::free)
    {
        for (const auto& id:s.manualOrder) if (auto* t=findTrack(s,id)) add(*t);
    }
    else
    {
        for (const auto& t:s.tracks) if(t.role==Role::songTrack) add(t);
        std::vector<std::string> groups;
        for(const auto& t:s.tracks)
            if ((t.role==Role::performerTrack || t.role==Role::performerMix) && !has(groups,t.group)) groups.push_back(t.group);
        for(const auto& group:groups)
            for(auto role:{Role::performerTrack,Role::performerMix})
                for(const auto& t:s.tracks) if(t.group==group && t.role==role) add(t);
        for (const auto& t:s.tracks) if(t.role==Role::songMain) add(t);
    }
    out.insert(out.end(),pinned.begin(),pinned.end());
    return out;
}
Audibility audibility(const Session& s, const Track& t)
{
    Audibility a;
    const auto quiet=[&](std::string reason){ a.program=false; a.cue=false; a.reason=std::move(reason); };
    if(!t.enabled) quiet("Tensor spento");
    else if(t.mute && t.role!=Role::songMain) quiet("Mute diretto");
    else
    {
        const Track* mix=nullptr;
        bool groupSolo=false;
        for(const auto& other:s.tracks)
        {
            if(other.group==t.group && other.role==Role::performerMix) mix=&other;
            if(other.group==t.group && other.role==t.role && other.solo) groupSolo=true;
        }
        if(t.role==Role::performerTrack && mix && (!mix->enabled || mix->mute)) quiet("Esclusa dal MIX");
        else if(groupSolo && !t.solo && (t.role==Role::performerTrack || t.role==Role::songTrack)) quiet("Esclusa da Solo");
    }
    const bool mainMuted=std::any_of(s.tracks.begin(),s.tracks.end(),[](const auto& x){return x.role==Role::songMain && x.mute;});
    if(a.program && mainMuted) {a.program=false; a.reason="Program in mute / Cue attive";}
    return a;
}
std::vector<int> effectiveChannels(const Track& t, const Clip& c)
{
    if(!validOrder(t.order,t.audioChannels) || c.channels<1 || c.channels>64) return {};
    std::vector<int> out;
    out.reserve(t.order.size());
    for(int src:t.order) out.push_back(src < c.channels ? src : -1);
    return out;
}
std::string validate(const Session& s)
{
    if(s.songId.empty() || s.songId.size()>128 || s.tracks.empty() || s.tracks.size()>256) return "Sessione fuori dai limiti";
    if((s.mode!=Mode::grouped && s.mode!=Mode::free) || s.headerWidth<320 || s.headerWidth>480 || !std::isfinite(s.zoom) || s.zoom<20 || s.zoom>100) return "Vista non valida";
    std::unordered_set<std::string> ids,clips;
    int mainCount=0;
    for(const auto& t:s.tracks)
    {
        if(t.id.empty() || t.id.size()>128 || !ids.insert(t.id).second) return "ID traccia duplicato o invalido";
        Role parsed{};
        if(!parseRole(roleKey(t.role), parsed)) return "Ruolo non valido";
        if(t.role==Role::songMain) ++mainCount;
        if(t.name.empty() || t.name.size()>256 || t.group.empty() || t.group.size()>128 || t.route.size()>256 || t.groupName.size()>256) return "Metadata non validi";
        if(t.monitor!="Auto" && t.monitor!="Live" && t.monitor!="Playback" && t.monitor!="Merge") return "Monitoring non valido";
        if(t.height<minimumHeight(t) || t.height>1200 || t.midiPins<0 || t.midiPins>8 || t.oscPins<0 || t.oscPins>8 || t.dmxPins<0 || t.dmxPins>8 || t.automations<0 || t.automations>64) return "Dimensioni fuori range";
        if(!validOrder(t.order,t.audioChannels)) return "Ordine canali non valido";
        if(t.solo && !capabilities(t,s.mode).solo) return "Song Main non ha Solo";
        if(t.mute && t.solo) return "Mute e Solo non simultanei";
        if(!std::isfinite(t.gain) || t.gain < -60 || t.gain >12 || !std::isfinite(t.pan) || t.pan < -1 || t.pan >1) return "Gain/Pan non validi";
        if(t.clips.size()>64) return "Troppe clip fixture";
        std::size_t totalClips = 0;
        for(const auto& c:t.clips)
        {
            if (!validateClipTree(c, t, clips, totalClips, 0))
                return "Albero clip non valido";
        }
    }
    if(mainCount!=1) return "Serve esattamente una Song Main";
    std::unordered_set<std::string> order;
    for(const auto& id:s.manualOrder) if(!ids.contains(id) || !order.insert(id).second) return "Ordine tracce non valido";
    if(order.size()!=ids.size()) return "Ordine tracce incompleto";
    std::unordered_set<std::string> selected;
    for(const auto& id:s.selected) if(!ids.contains(id) || !selected.insert(id).second) return "Selezione non valida";
    if(!s.active.empty() && !ids.contains(s.active)) return "Traccia attiva mancante";
    if(!s.selectedClip.empty() && !clips.contains(s.selectedClip)) return "Clip attiva mancante";
    return {};
}
Store::Store(Session s):current(std::move(s))
{
    if(auto e=validate(current);!e.empty()) throw std::invalid_argument(e);
}
void Store::notify() { if(changed) changed(); }
bool Store::commit(Session candidate,std::string label)
{
    error = prepareCommit ? prepareCommit(current, candidate) : std::string{};
    if (!error.empty()) { notify(); return false; }
    error=validate(candidate);
    if(!error.empty()) {notify();return false;}
    if(candidate==current)return false;
    if(past.size()==96) past.erase(past.begin());
    past.push_back({current,action}); future.clear();
    current=std::move(candidate); action=std::move(label); notify(); return true;
}
bool Store::replace(Session s) {return commit(std::move(s),"Importa sessione Lab");}
bool Store::undo()
{
    if(past.empty()) return false;
    future.push_back({current,action}); current=std::move(past.back().state);action=std::move(past.back().action);past.pop_back();error.clear();notify();return true;
}
bool Store::redo()
{
    if(future.empty())return false;
    past.push_back({current,action});current=std::move(future.back().state);action=std::move(future.back().action);future.pop_back();error.clear();notify();return true;
}
void Store::select(const std::string& id,Selection how)
{
    if(!findTrack(current,id))return;
    auto visible=visibleOrder(current);
    if(!has(visible,id))return;
    if(how==Selection::range && has(visible,rangeAnchor))
    {
        auto a=std::find(visible.begin(),visible.end(),rangeAnchor), b=std::find(visible.begin(),visible.end(),id);
        if(a>b)std::swap(a,b);
        current.selected.assign(a,b+1);
    }
    else if(how==Selection::toggle)
    {
        auto i=std::find(current.selected.begin(),current.selected.end(),id);
        if(i==current.selected.end())current.selected.push_back(id);else current.selected.erase(i);
        rangeAnchor=id;
    }
    else {current.selected={id};rangeAnchor=id;}
    current.active=id;notify();
}
void Store::selectClip(const std::string& id)
{
    for(const auto& t:current.tracks)
        if(findClip(t,id)!=nullptr){current.selectedClip=id;notify();return;}
}
bool Store::removeClip (const std::string& trackId, const std::string& clipId)
{
    auto candidate = current;
    auto* track = findTrack (candidate, trackId);
    if (track == nullptr) { return false; }

    const auto* removedRoot = findClip(*track, clipId);
    if (removedRoot == nullptr) { return false; }
    const bool removesSelection = !candidate.selectedClip.empty()
        && findClip(*removedRoot, candidate.selectedClip) != nullptr;

    const auto removeFrom = [&clipId](auto&& self, std::vector<Clip>& clips) -> bool
    {
        const auto clip = std::find_if(clips.begin(), clips.end(),
            [&clipId](const Clip& value) { return value.id == clipId; });
        if (clip != clips.end()) { clips.erase(clip); return true; }
        for (auto& candidate : clips)
            if (self(self, candidate.children)) { return true; }
        return false;
    };
    if (!removeFrom(removeFrom, track->clips)) { return false; }
    if (removesSelection) { candidate.selectedClip.clear(); }
    return commit (std::move (candidate), "Elimina Clip");
}
bool Store::toggle(const std::string& id,Flag f)
{
    auto s=current;auto* t=findTrack(s,id);if(!t)return false;
    switch(f)
    {
        case Flag::mute:t->mute=!t->mute;if(t->mute)t->solo=false;break;
        case Flag::solo:if(!capabilities(*t,s.mode).solo)return false;t->solo=!t->solo;if(t->solo)t->mute=false;break;
        case Flag::enabled:t->enabled=!t->enabled;break;
        case Flag::bypass:t->bypass=!t->bypass;break;
        case Flag::buffer:t->buffer=!t->buffer;break;
        case Flag::oversample:t->oversample=!t->oversample;break;
        case Flag::expanded:t->expanded=!t->expanded;break;
        case Flag::pinned:t->pinned=!t->pinned;break;
    }
    return commit(std::move(s),"Controllo traccia");
}
bool Store::setGain(const std::string& id,double value)
{
    if(!std::isfinite(value))return false;
    auto s=current;auto* t=findTrack(s,id);if(!t)return false;t->gain=std::clamp(value,-60.0,12.0);return commit(std::move(s),"Gain fixture");
}
bool Store::setPan(const std::string& id,double value)
{
    if(!std::isfinite(value))return false;
    auto s=current;auto* t=findTrack(s,id);if(!t)return false;t->pan=std::clamp(value,-1.0,1.0);return commit(std::move(s),"Pan fixture");
}
bool Store::resize(const std::string& id,int value)
{
    auto s=current;auto* t=findTrack(s,id);if(!t)return false;t->height=std::clamp(value,minimumHeight(*t),1200);return commit(std::move(s),"Altezza traccia");
}
bool Store::rename(const std::string& id,const std::string& value)
{
    if(value.empty() || value.size()>256)return false;
    auto s=current;auto* t=findTrack(s,id);if(!t)return false;t->name=value;return commit(std::move(s),"Rinomina traccia");
}
bool Store::setMode(Mode m) {auto s=current;s.mode=m;return commit(std::move(s),"Modalita' di vista");}
bool Store::setOrder(const std::string& id,const std::vector<int>& order)
{
    auto s=current;auto* t=findTrack(s,id);if(!t || !validOrder(order,t->audioChannels))return false;
    t->order=order;return commit(std::move(s),"Ordine condiviso canali");
}
bool Store::move(const std::vector<std::string>& ids,const std::string& anchor,bool after)
{
    if(current.mode!=Mode::free || ids.empty())return false;
    auto s=current;
    const auto* target=findTrack(s,anchor);
    if(!target || target->pinned || has(ids,anchor))return false;
    std::unordered_set<std::string> unique;
    for(const auto& id:ids)
    {
        const auto* t=findTrack(s,id);
        if(!t || t->pinned || !unique.insert(id).second)return false;
    }
    std::vector<std::string> moving,rest;
    for(const auto& id:s.manualOrder) (has(ids,id)?moving:rest).push_back(id);
    auto at=std::find(rest.begin(),rest.end(),anchor);
    if(at==rest.end())return false;
    if(after)++at;
    rest.insert(at,moving.begin(),moving.end());s.manualOrder=std::move(rest);
    return commit(std::move(s),"Riordina tracce (solo vista)");
}
} // namespace ayra::lab
