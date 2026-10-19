#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
namespace ayra::lab
{
/** Native showcase; --legacy keeps the original single-track Lab and JSON v1. */
std::unique_ptr<juce::Component> makeTimelinePanelDemo();
}
