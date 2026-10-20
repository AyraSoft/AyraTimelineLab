#include "ayra_ui.h"
#include "SharedChromeLookAndFeel.h"
#include "../../demo/TimelineIdentityArtwork.h"
#include <cstdlib>
#include <iostream>
#include <array>
using namespace ayra::timeline_panel;
using ayra::lab::TimelineIdentityArtwork;
namespace
{
int checks = 0;
#define CHECK(x) do {++checks; if(!(x)) {std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; std::exit(1);}} while(false)
void writePng(const juce::File& file, const juce::Image& image)
{
    file.deleteFile(); juce::FileOutputStream output(file);
    CHECK(output.openedOk()); CHECK(juce::PNGImageFormat{}.writeImageToStream(image,output));
}
void background(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour, int kind)
{
    // An explicit graphics fixture, NOT a waveform decoded from audio.
    g.setColour(ayra::tokens::surfaceRecessed()); g.fillRect(bounds);
    g.setColour(ayra::tokens::borderHairline().withAlpha(0.35f));
    for(int x=bounds.getX();x<bounds.getRight();x+=36)g.drawVerticalLine(x,static_cast<float>(bounds.getY()),static_cast<float>(bounds.getBottom()));
    auto body=bounds.reduced(24,18).toFloat();
    g.setColour(colour.withAlpha(0.72f));g.fillRoundedRectangle(body,3);
    g.setColour(juce::Colours::black.withAlpha(0.32f));
    if(kind==0){
        juce::Path wave;
        for(int x=0;x<static_cast<int>(body.getWidth());++x){
            float size=5.0f+14.0f*std::abs(std::sin(x*0.061f)*std::sin(x*0.017f));
            wave.startNewSubPath(body.getX()+x,body.getCentreY()-size);
            wave.lineTo(body.getX()+x,body.getCentreY()+size);
        }
        g.strokePath(wave,juce::PathStrokeType(1.0f));
    }else{
        for(int i=0;i<32;++i)g.fillRoundedRectangle(body.getX()+i*15.0f,body.getY()+12+(i*19)%juce::jmax(1,static_cast<int>(body.getHeight()-25)),13,3,1);
    }
}
void oldName(juce::Graphics& g, juce::Rectangle<int> row, juce::Colour owner, juce::String title)
{
    const auto area=row.removeFromRight(240).toFloat();
    const auto shade=ayra::tokens::surfaceBase().interpolatedWith(owner,0.20f);
    juce::ColourGradient gradient(shade.withAlpha(0.97f),area.getRight(),0,shade.withAlpha(0.0f),area.getX(),0,false);
    gradient.addColour(0.55,shade.withAlpha(0.80f));g.setGradientFill(gradient);g.fillRect(area);
    g.setColour(ayra::tokens::textPrimary());g.setFont(juce::Font(juce::FontOptions().withHeight(14).withStyle("Bold")));
    g.drawText(title,area.toNearestInt().reduced(12,0),juce::Justification::centredRight,true);
}
juce::Image comparison(bool dark, float scale, bool left=false)
{
    namespace tk=ayra::tokens;
    tk::setThemeMode(dark?tk::Mode::Dark:tk::Mode::Light);
    juce::Image image(juce::Image::ARGB,juce::roundToInt(1160*scale),juce::roundToInt(584*scale),true);
    juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(scale));g.fillAll(tk::surfaceBase());
    g.setFont(tk::type::title());g.setColour(tk::textPrimary());g.drawText("Identita' della lane",20,12,1000,26,juce::Justification::centredLeft);
    g.setFont(tk::type::caption());g.setColour(tk::textSecondary());
    g.drawText("Confronto nativo JUCE del painter  /  fixture grafica, non playback",20,40,1000,20,juce::Justification::centredLeft);
    g.drawText("PRIMA  /  gradiente a tutta altezza",20,72,540,20,juce::Justification::centredLeft);
    g.drawText("DOPO  /  identita' flottante",600,72,540,20,juce::Justification::centredLeft);
    const std::array<juce::String,3> primary {"Voice", "Keys", "Voce principale / armonizzazioni"};
    const std::array<juce::String,3> secondary {"Audio I", "MIDI I", "Audio II"};
    const std::array<juce::Colour,3> owners {juce::Colour(0xff39b9b6),juce::Colour(0xffdfae68),juce::Colour(0xff39b9b6)};
    for(int i=0;i<3;++i){
        const auto type=i==1?tk::nodeColour(tk::StreamType::midi):tk::nodeColour(tk::StreamType::audio);
        const juce::Rectangle<int> before(20,100+i*136,540,128),after(600,100+i*136,540,128);
        background(g,before,type,i%2);background(g,after,type,i%2);
        oldName(g,before,owners[i],primary[i]+" / "+secondary[i]);
        TimelineIdentityContent c{primary[i],secondary[i],owners[i],type,false,left,false,true};
        const auto layout=TimelineIdentityLayout::make(after,c,TimelineIdentityArtwork::typography());
        TimelineIdentityArtwork::draw(g,c,layout,TimelineIdentityArtwork::prepare(c,layout));
    }
    TimelineIdentityContent compact {"Markers",{},juce::Colour(0xff888888),tk::textSecondary(),true,false,false,false};
    const juce::Rectangle<int> row(600,516,540,24);background(g,row,tk::surfaceRaised(),1);
    const auto layout=TimelineIdentityLayout::make(row,compact,TimelineIdentityArtwork::typography());
    TimelineIdentityArtwork::draw(g,compact,layout,TimelineIdentityArtwork::prepare(compact,layout));
    g.setFont(tk::type::caption());g.setColour(tk::textSecondary());
    g.drawText("Stessa area clip. Nessun header aggiunto. Nome della traccia e nome della lane hanno destinazioni di rinomina distinte.",20,552,1120,20,juce::Justification::centredLeft);
    return image;
}
juce::Image widgets(bool dark)
{
    namespace tk=ayra::tokens;
    tk::setThemeMode(dark?tk::Mode::Dark:tk::Mode::Light);
    ayra::lab::SharedChromeLookAndFeel laf;
    laf.setColour(juce::TextButton::buttonColourId,tk::surfaceRaised());
    laf.setColour(juce::TextButton::textColourOffId,tk::textPrimary());
    laf.setColour(juce::TextButton::textColourOnId,tk::textOnTint(tk::accent()));
    juce::Image image(juce::Image::ARGB,860,190,true);juce::Graphics g(image);
    g.fillAll(tk::surfaceBase());g.setFont(tk::type::title());g.setColour(tk::textPrimary());
    g.drawText("Widget / sorgente visuale selezionata",20,12,820,26,juce::Justification::centredLeft);
    const std::array<const char*,5> names{"Normale","Hover","Premuto","Attivo","Disabilitato"};
    for(int i=0;i<5;++i){
        juce::Graphics::ScopedSaveState guard(g);g.setOrigin(20+i*148,64);
        juce::TextButton button(names[i]);button.setLookAndFeel(&laf);button.setSize(138,36);
        if(i==3)button.setToggleState(true,juce::dontSendNotification);
        if(i==4)button.setEnabled(false);
        laf.drawButtonBackground(g,button,tk::surfaceRaised(),i==1,i==2);
        laf.drawButtonText(g,button,i==1,i==2);button.setLookAndFeel(nullptr);
    }
    for(int i=0;i<5;++i)laf.drawVMeter(g,{775.0f+i*10,55,6,100},0.15f+i*0.18f);
    g.setFont(tk::type::caption());g.setColour(tk::textSecondary());
#if AYRA_LAB_VISUALS_FROM_MULTIVERSE
    g.drawText("Buttons.cpp originale adattato solo nel nome della classe; meter e token inclusi dal checkout.",20,130,735,24,juce::Justification::centredLeft);
#else
    g.drawText("Profilo snapshot: questa immagine NON usa ApplicationLookAndFeel di Multiverse.",20,130,735,24,juce::Justification::centredLeft);
#endif
    return image;
}
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto output=argc>1?juce::File(juce::String(argv[1])):juce::File::getCurrentWorkingDirectory();
    CHECK(output.createDirectory().wasOk());
    for(bool dark:{true,false}){
        ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
        for(int width:{0,12,44,120,540,1200})for(int height:{16,24,48,96,400})for(bool left:{false,true}){
            TimelineIdentityContent c{"A very long owner name that must not be squeezed", "Audio I", juce::Colour(0xff38bdb5), juce::Colour(0xff16c95c),height<48,left,false,true};
            const juce::Rectangle<int> area(0,0,width,height);
            auto l=TimelineIdentityLayout::make(area,c,TimelineIdentityArtwork::typography());
            CHECK(l.backdrop.isEmpty()||area.contains(l.backdrop));CHECK(l.primary.isEmpty()||area.contains(l.primary));
            CHECK(l.secondary.isEmpty()||area.contains(l.secondary));
            CHECK(height<96||l.backdrop.getHeight()<height);
            CHECK(l.primaryFont.getHorizontalScale()==1.0f);
            auto image=TimelineIdentityArtwork::prepare(c,l);CHECK(l.backdrop.isEmpty()||image.isValid());
        }
        writePng(output.getChildFile(dark?"widgets-dark.png":"widgets-light.png"),widgets(dark));
        writePng(output.getChildFile(dark?"identity-left-dark.png":"identity-left-light.png"),comparison(dark,1,true));
        for(float scale:{1.0f,1.25f,1.5f,2.0f})writePng(output.getChildFile((dark?"identity-dark-":"identity-light-")+juce::String(scale,2)+".png"),comparison(dark,scale));
    }
    ayra::shadowCache().clear();juce::ImageCache::releaseUnusedImages();
    std::cout<<"PASS "<<checks<<" identity layout/render assertions; screenshots are native painter fixtures, not full application frames\n";
}
