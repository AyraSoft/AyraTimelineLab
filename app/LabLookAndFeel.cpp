/** @file LabLookAndFeel.cpp
    Disegno del banco: header per capability; clip unica con N righe, nome sopra
    la waveform, selezione scura e fade dai due angoli inferiori.
    Nessun I/O, documento o lookup di processor in paint. @see LabLookAndFeel.h */
#include "LabLookAndFeel.h"
#include "ThemeData.h"
#include <algorithm>
#include <cmath>

namespace ayra::lab
{
namespace
{
juce::Font font(float height,bool bold=false)
{
    return juce::Font(juce::FontOptions().withHeight(height).withStyle(bold?"Bold":"Regular"));
}
}
Peaks makeSyntheticPeaks(int seed)
{
    Peaks p;std::uint32_t z=static_cast<std::uint32_t>(seed)*5137u+11u;
    auto rnd=[&](){z=z*1664525u+1013904223u;return static_cast<double>(z)/4294967296.0;};
    for(std::size_t i=0;i<p.size();++i)
    {
        const double x=static_cast<double>(i)/(p.size()-1);
        const auto pulse=std::pow(std::max(0.0,std::sin((x*24+seed*.13)*juce::MathConstants<double>::pi)),5.0);
        const auto gap=std::sin(x*21+seed)>.8?.2:1.0;
        const auto env=(.15+.45*std::abs(std::sin(x*9+seed))+.2*pulse)*gap;
        p[i]={static_cast<float>(-(.12+rnd()*.85)*env),static_cast<float>((.1+rnd()*.9)*env)};
    }
    return p;
}
juce::Path makeSyntheticShape(int seed)
{
    const auto peaks=makeSyntheticPeaks(seed);juce::Path path;
    path.preallocateSpace(static_cast<int>(peaks.size()*8+16));
    for(std::size_t i=0;i<peaks.size();++i)
    {
        const float x=1000.0f*static_cast<float>(i)/(peaks.size()-1),y=50-peaks[i].maximum*43;
        if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
    }
    for(std::size_t i=peaks.size();i-->0;)
        path.lineTo(1000.0f*static_cast<float>(i)/(peaks.size()-1),50-peaks[i].minimum*43);
    path.closeSubPath();return path;
}
LabLookAndFeel::LabLookAndFeel(){setDark(true);}
void LabLookAndFeel::setDark(bool d)
{
    dark=d;
    palette=readCanonicalPalette(d);
    setColour(juce::ResizableWindow::backgroundColourId,palette.base);
    setColour(juce::Label::textColourId,palette.text);
    setColour(juce::TextButton::buttonColourId,palette.raised);
    setColour(juce::TextButton::buttonOnColourId,palette.accent);
    setColour(juce::TextButton::textColourOffId,palette.text);
    setColour(juce::TextButton::textColourOnId,ayra::tokens::textOnTint(palette.accent));
    setColour(juce::TextEditor::backgroundColourId,palette.recessed);
    setColour(juce::TextEditor::textColourId,palette.text);
    setColour(juce::TextEditor::outlineColourId,palette.border);
    setColour(juce::ComboBox::backgroundColourId,palette.recessed);
    setColour(juce::ComboBox::textColourId,palette.text);
    setColour(juce::ComboBox::outlineColourId,palette.border);
    setColour(juce::Slider::thumbColourId,palette.accent);
    setColour(juce::Slider::trackColourId,palette.accent);
    setColour(juce::Slider::textBoxTextColourId,palette.value);
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId,palette.recessed);
    setColour(juce::PopupMenu::backgroundColourId,palette.base);
    setColour(juce::PopupMenu::textColourId,palette.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,palette.raised);
    setColour(juce::PopupMenu::highlightedTextColourId,palette.accent);
}
const char* LabLookAndFeel::sourceLabel()const noexcept
{
#if defined(AYRA_LAB_VISUALS_FROM_MULTIVERSE) && AYRA_LAB_VISUALS_FROM_MULTIVERSE
    return "Multiverse: sorgenti visuali diretti / adattatore Lab";
#else
    return "ayra_ui: snapshot locale / non Multiverse live";
#endif
}
void LabLookAndFeel::raised(juce::Graphics& g,juce::Rectangle<float> r,float corner,juce::Colour base)const
{
    canonicalRaised(g,r,corner,base);
}
void LabLookAndFeel::drawHeader(juce::Graphics& g,juce::Rectangle<int> r,const Track& t,bool selected)
{
    const auto tint=juce::Colour(t.colour);
    g.setGradientFill(juce::ColourGradient(palette.top.interpolatedWith(tint,selected?.18f:.07f),0,0,palette.base,static_cast<float>(r.getWidth()),static_cast<float>(r.getHeight()),false));
    g.fillRect(r);g.setColour(tint);g.fillRect(0,0,selected?5:4,r.getHeight());
    g.setColour(selected?tint.withAlpha(.6f):palette.border);g.drawRect(r,1);
    auto icon=juce::Rectangle<float>(48,12,26,26);
    g.setColour(tint.withAlpha(.12f));g.fillRoundedRectangle(icon,t.role==Role::performerMix?13.0f:5.0f);
    g.setColour(tint);g.drawRoundedRectangle(icon.reduced(.5f),t.role==Role::performerMix?13.0f:5.0f,t.role==Role::songMain?2.0f:1.0f);
    g.setFont(font(16,true));
    const auto symbol=t.role==Role::songTrack?"ST":t.role==Role::performerTrack?"T":t.role==Role::performerMix?"M":"S";
    g.drawText(symbol,icon.toNearestInt(),juce::Justification::centred);
    g.setColour(palette.secondary);g.setFont(font(9));
    g.drawText(roleLabel(t.role),80,29,r.getWidth()-110,13,juce::Justification::centredLeft);
    if(t.height>=148)
    {
        auto caps=juce::String("A ")+juce::String(t.audioChannels);
        if(t.midiPins)caps+="  MIDI "+juce::String(t.midiPins);
        if(t.oscPins)caps+="  OSC "+juce::String(t.oscPins);
        if(t.dmxPins)caps+="  DMX "+juce::String(t.dmxPins);
        g.drawText(caps,18,45,r.getWidth()-30,15,juce::Justification::centredLeft);
    }
    g.setColour(palette.secondary.withAlpha(.45f));
    g.fillRect(r.getCentreX()-13,r.getBottom()-4,26,1);
}
void LabLookAndFeel::drawTrackGrid(juce::Graphics& g,juce::Rectangle<int> r,double pps,double originX)const
{
    g.fillAll(palette.recessed);g.setColour(palette.border.withAlpha(.3f));
    const auto step=static_cast<int>(std::max(20.0,pps*2));
    for(int x=static_cast<int>(std::fmod(originX,static_cast<double>(step)))-step;x<r.getWidth();x+=step)g.drawVerticalLine(x,0,static_cast<float>(r.getHeight()));
}
#if !AYRA_LAB_VISUALS_FROM_MULTIVERSE
void LabLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool over,bool down)
{
    auto base=button.getToggleState()?palette.accent.withAlpha(.25f).overlaidWith(palette.base.withAlpha(.5f)):palette.raised;
    if(button.isColourSpecified(juce::TextButton::buttonOnColourId)&&button.getToggleState())base=button.findColour(juce::TextButton::buttonOnColourId);
    if(over)base=base.brighter(.07f);if(down)base=base.darker(.1f);
    raised(g,button.getLocalBounds().toFloat().reduced(.5f),4,base);
    if(button.hasKeyboardFocus(false)){g.setColour(palette.accent);g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1),4,1.5f);}
}
void LabLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool)
{
    auto ink=palette.text;
    if(button.getToggleState()&&button.isColourSpecified(juce::TextButton::buttonOnColourId))
    {
        const auto bg=button.findColour(juce::TextButton::buttonOnColourId);
        ink=bg.getPerceivedBrightness()>.6f?juce::Colour(0xff101820):juce::Colours::white;
    }
    g.setColour(ink.withMultipliedAlpha(button.isEnabled()?1.0f:.35f));g.setFont(font(10,true));
    g.drawText(button.getButtonText(),button.getLocalBounds().reduced(3,0),juce::Justification::centred);
}
#endif
}
