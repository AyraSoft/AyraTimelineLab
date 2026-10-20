/** Content-driven event rows shared by the canvas, painter and headless tests.
    No GUI/document mutation. Input uses the already projected screen geometry.
    Rebuild on event/viewport changes, never in paint or from current row height. */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <queue>
#include <vector>

namespace ayra::lab::clips::keyframe_lane_layout
{
inline constexpr float padY=3.0f,pillHeight=18.0f,rowGap=4.0f,spanRowGap=12.0f;
// Retained for legacy callers; new placement has no six-row collision clamp.
inline constexpr int maxRows=6;
inline int contentHeight(int rows,bool span) noexcept
{
    rows=std::max(1,rows);
    return static_cast<int>(2*padY+rows*pillHeight+(rows-1)*(span?spanRowGap:rowGap)+(span?8:0));
}
struct VisualInterval { std::size_t source=0; double left=0,right=0; };
struct RowPlacement { std::size_t source=0;int row=0; };
struct RowLayout { std::vector<RowPlacement> placements; int rows=1; };
inline RowLayout packVisibleRows(std::vector<VisualInterval> intervals,double width)
{
    if(!std::isfinite(width)||width<=0)return {};
    std::erase_if(intervals,[&](const auto& i){return !std::isfinite(i.left)||!std::isfinite(i.right)
        ||i.right<=i.left||i.right<=0||i.left>=width;});
    std::stable_sort(intervals.begin(),intervals.end(),[](const auto& a,const auto& b){
        return a.left<b.left||(a.left==b.left&&a.source<b.source);});
    using End=std::pair<double,int>;
    std::priority_queue<End,std::vector<End>,std::greater<End>> occupied;
    std::priority_queue<int,std::vector<int>,std::greater<int>> freeRows;
    RowLayout result;result.placements.reserve(intervals.size());int used=0;
    for(const auto& i:intervals) {
        while(!occupied.empty()&&occupied.top().first<=i.left) {
            freeRows.push(occupied.top().second);occupied.pop();
        }
        int row;
        if(freeRows.empty())row=used++;
        else {row=freeRows.top();freeRows.pop();}
        result.placements.push_back({i.source,row});occupied.push({i.right,row});
    }
    result.rows=std::max(1,used);return result;
}
}
