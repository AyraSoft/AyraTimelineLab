/** Application adapter for the original Multiverse standard-button recipes.
    Its generated implementation changes only the declaring class and include.
    It is NOT ApplicationLookAndFeel and imports no Tensor/graph/runtime modules.
    The ordinary ayra_ui snapshot profile deliberately does not claim this path.
*/
#pragma once
#include "ayra_ui.h"
namespace ayra::lab
{
class SharedChromeLookAndFeel : public juce::LookAndFeel_V4,
                               public ayra::CommonDefaultLookAndFeel
{
public:
#if AYRA_LAB_VISUALS_FROM_MULTIVERSE
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    juce::Font getTextButtonFont(juce::TextButton&,int) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void drawTickBox(juce::Graphics&,juce::Component&,float,float,float,float,bool,bool,bool,bool) override;
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar&,juce::Graphics&) override;
    void drawTabButton(juce::TabBarButton&,juce::Graphics&,bool,bool) override;
#endif
};
}
