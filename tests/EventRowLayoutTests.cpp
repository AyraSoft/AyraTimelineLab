/** Tests the SAME projected interval packer used by KeyframeLaneCanvas. */
#include "../app/clips/keyframe/ayra_KeyframeRowLayout.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <limits>
#include <map>
using namespace ayra::lab::clips::keyframe_lane_layout;
namespace {int count=0;
#define CHECK(x) do{++count;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';std::exit(1);}}while(false)
void valid(const std::vector<VisualInterval>& input,const RowLayout& out)
{
 std::map<std::size_t,VisualInterval> source;for(auto i:input)source[i.source]=i;
 for(std::size_t i=0;i<out.placements.size();++i){const auto a=out.placements[i];CHECK(a.row>=0&&a.row<out.rows);
  for(std::size_t j=0;j<i;++j){const auto b=out.placements[j];if(a.row!=b.row)continue;
   CHECK(source[a.source].left>=source[b.source].right||source[b.source].left>=source[a.source].right);}}
}
}
int main()
{
 CHECK(contentHeight(1,false)==24);CHECK(contentHeight(2,false)==46);CHECK(contentHeight(1,true)==32);CHECK(contentHeight(2,true)==62);
 CHECK(packVisibleRows({},800).rows==1);
 std::vector<VisualInterval> spans{{0,0,100},{1,100,200},{2,200,300}};
 CHECK(packVisibleRows(spans,300).rows==1);CHECK(packVisibleRows(spans,0).placements.empty());
 std::vector<VisualInterval> overlap{{0,0,100},{1,10,110},{2,20,120}};
 const auto three=packVisibleRows(overlap,300);CHECK(three.rows==3);valid(overlap,three);
 std::vector<VisualInterval> many;for(int i=0;i<32;++i)many.push_back({static_cast<std::size_t>(i),0,100});
 const auto thirtytwo=packVisibleRows(many,200);CHECK(thirtytwo.rows==32);valid(many,thirtytwo);
 CHECK(packVisibleRows({{0,-100,-1},{1,200,300},{2,-10,10},{3,10,20}},100).placements.size()==2);
 CHECK(packVisibleRows({{0,1,1},{1,4,3},{2,0,std::numeric_limits<double>::infinity()}},100).placements.empty());
 // Changing horizontal scale changes visible collisions, not the lane's present H.
 std::vector<VisualInterval> points{{0,0,30},{1,20,50},{2,120,150}};
 CHECK(packVisibleRows(points,100).rows==2);
 points={{0,0,30},{1,40,70},{2,240,270}};CHECK(packVisibleRows(points,100).rows==1);
 std::mt19937 rng(37);
 for(int round=0;round<100;++round){std::vector<VisualInterval> data;
  for(int i=0;i<50;++i){const auto a=double(rng()%1100)-100;data.push_back({static_cast<std::size_t>(i),a,a+1+double(rng()%100)});}
  const auto layout=packVisibleRows(data,1000);valid(data,layout);
  auto shuffled=data;std::shuffle(shuffled.begin(),shuffled.end(),rng);
  const auto other=packVisibleRows(shuffled,1000);CHECK(other.rows==layout.rows);CHECK(other.placements.size()==layout.placements.size());
  for(std::size_t i=0;i<layout.placements.size();++i){CHECK(layout.placements[i].source==other.placements[i].source);CHECK(layout.placements[i].row==other.placements[i].row);}}
 std::cout<<"PASS "<<count<<" actual event-row layout assertions\n";
}
