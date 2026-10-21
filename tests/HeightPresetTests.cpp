#include "../modules/ayra_timeline_panel/TimelineHeightPresets.h"
#include <cstdlib>
#define REQUIRE(x) do {if(!(x)){std::cerr << "FAIL " << #x << "\n";return 1;}}while(false)
#include <iostream>
#include <limits>
using ayra::timeline_panel::height_presets::next;
static_assert(next(32.0f,false)==400);
static_assert(next(400.0f,false)==32);
static_assert(next(32.0f,true)==256);
static_assert(next(256.0f,true)==400);
static_assert(next(400.0f,true)==32);
int main()
{
    REQUIRE(next(-1.0f,false)==32);
    REQUIRE(next(std::numeric_limits<float>::quiet_NaN(),false)==32);
    REQUIRE(next(96.0f,false)==400);
    REQUIRE(next(256.0f,false)==32);
    REQUIRE(next(96.0f,true)==256);
    REQUIRE(next(176.0f,true)==400);
    std::cout << "PASS shared presentation-height levels\n";
}
