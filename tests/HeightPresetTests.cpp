#include "../modules/ayra_timeline_panel/TimelineHeightPresets.h"
#include <cassert>
#include <iostream>
#include <limits>
using ayra::timeline_panel::height_presets::next;
static_assert(next(24.0f,false)==400);
static_assert(next(400.0f,false)==24);
static_assert(next(24.0f,true)==256);
static_assert(next(256.0f,true)==400);
static_assert(next(400.0f,true)==24);
int main()
{
    assert(next(-1.0f,false)==24);
    assert(next(std::numeric_limits<float>::quiet_NaN(),false)==24);
    assert(next(96.0f,false)==400);
    assert(next(256.0f,false)==24);
    assert(next(96.0f,true)==256);
    assert(next(176.0f,true)==400);
    std::cout << "PASS shared presentation-height levels\n";
}
