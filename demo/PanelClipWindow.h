#pragma once
#include "../core/LabModel.h"
#include <ayra_timeline/ayra_timeline_presentation.h>
#include <limits>
#include <memory>
#include <set>
#include <unordered_set>
#include <unordered_map>
namespace ayra::lab
{
/** Derived fixture projection only. Queries use the actual ayra_timeline index.
    The Store remains authoritative; invisible clips are not deleted. */
class PanelClipWindow final
{
public:
    void update(const Track& source,std::uint64_t revision)
    {
        if(generation==revision&&full.id==source.id)return;
        std::vector<ayra::timeline::TimelineIntervalIndex::Interval> intervals;
        auto ids=std::make_shared<std::unordered_set<std::string>>();
        byId.clear();for(std::size_t i=0;i<source.clips.size();++i){const auto& clip=source.clips[i];intervals.push_back({clip.start,clip.start+clip.length});ids->insert(clip.id);byId[clip.id]=i;}
        index.reset(intervals);full=source;shell=source;shell.clips={};canonicalIds=std::move(ids);generation=revision;
    }
    std::set<std::string> visible(const ayra::timeline::TimelineTimeWindow& time,int width,bool capture)const
    {
        std::set<std::string> ids;
        if(capture){for(const auto& clip:full.clips)ids.insert(clip.id);return ids;}
        index.visit(time.xToTime(-32),time.xToTime(width+32),[&](auto i){ids.insert(full.clips[i].id);});return ids;
    }
    Track projection(const std::set<std::string>& wanted,int height)const
    {
        auto out=shell;out.height=height;std::vector<std::size_t> indices;indices.reserve(wanted.size());
        for(const auto& id:wanted)if(const auto it=byId.find(id);it!=byId.end())indices.push_back(it->second);
        std::sort(indices.begin(),indices.end());out.clips.reserve(indices.size());
        for(auto i:indices)out.clips.push_back(full.clips[i]);return out;
    }
    std::shared_ptr<const std::unordered_set<std::string>> canonicalIds;
private:
    Track full,shell;
    std::unordered_map<std::string,std::size_t> byId;
    ayra::timeline::TimelineIntervalIndex index;
    std::uint64_t generation=std::numeric_limits<std::uint64_t>::max();
};
}
