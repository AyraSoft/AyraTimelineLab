/** The production MIDI overview geometry, without JUCE or external providers. */
#include "../app/clips/midi/ayra_MidiPreviewGeometry.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int checks=0;
void check(bool value,int line) { ++checks; if(!value) {std::cerr<<"FAIL "<<line<<'\n';std::exit(1);} }
#define CHECK(x) check((x),__LINE__)
}
int main()
{
    using namespace ayra::lab::clips::midi_preview;
    CHECK(headerHeight(18)==0);CHECK(headerHeight(31)==0);CHECK(headerHeight(32)==16);
    CHECK(headerHeight(230)==16);CHECK(headerHeight(374)==16);
    for(float width:{1.0f,2.0f,4.0f,8.0f,32.0f,300.0f,1000.0f})
        for(float height:{1.0f,2.0f,4.0f,16.0f,18.0f,40.0f,64.0f})
            for(bool drum:{false,true})
                for(int pitch=0;pitch<128;++pitch) {
                    const auto n=project(width,height,0.1f,0.10001f,pitch,0,127,drum);
                    CHECK(!n.empty());CHECK(n.x>=0&&n.y>=0);
                    CHECK(n.x+n.width<=width+0.0001f);
                    CHECK(n.y+n.height<=height+0.0001f);
                    if(width>=2)CHECK(n.width>=1);
                    if(height>=2)CHECK(n.height>=2);
                    const auto left=project(width,height,-0.5f,0.05f,pitch,0,127,drum);
                    const auto right=project(width,height,0.999f,1.5f,pitch,0,127,drum);
                    CHECK(!left.empty()&&!right.empty());
                    CHECK(left.x>=0&&right.x+right.width<=width+0.0001f);
                }
    const auto high=project(100,16,.1f,.2f,127,0,127,false);
    const auto low=project(100,16,.1f,.2f,0,0,127,false);
    CHECK(high.y==0);CHECK(low.y+low.height==16);
    const auto onePitch=project(100,16,.1f,.2f,60,60,60,false);
    CHECK(onePitch.y+onePitch.height/2==8);
    CHECK(project(100,16,-1,0,60,0,127,false).empty());
    CHECK(project(100,16,1,2,60,0,127,false).empty());
    CHECK(project(100,16,.5f,.2f,60,0,127,false).empty());
    CHECK(project(100,0,0,1,60,0,127,false).empty());
    CHECK(project(0,16,0,1,60,0,127,false).empty());
    CHECK(project(100,16,0,1,60,72,48,false).empty());
    CHECK(project(100,16,0,1,20,48,72,false).empty());
    CHECK(project(100,16,std::numeric_limits<float>::quiet_NaN(),1,60,0,127,false).empty());
    std::cout<<"PASS "<<checks<<" production MIDI preview geometry assertions\n";
}
