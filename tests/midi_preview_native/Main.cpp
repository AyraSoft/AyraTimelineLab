#include <juce_graphics/juce_graphics.h>
#include "../../app/clips/midi/ayra_MidiPreviewGeometry.h"
#include "../../app/clips/midi/ayra_MidiPreviewPainter.h"
#include <iostream>
#include <stdexcept>
namespace {
int count=0;
void check(bool v,int line){++count;if(!v)throw std::runtime_error("line "+std::to_string(line));}
#define CHECK(v) check((v),__LINE__)
}
int main(int argc,char** argv)
{
    using namespace ayra::lab::clips;
    try {
        const juce::File out(argc>1?argv[1]:"/tmp/ayra-midi-preview");out.createDirectory();
        for(float width:{4.0f,8.0f,32.0f,160.0f,300.0f}) for(float height:{2.0f,16.0f,18.0f,40.0f})
            for(float scale:{1.0f,1.25f,1.5f,2.0f}) for(bool drum:{false,true})
                for(int pitch:{0,42,84,127}) {
                    juce::Image image(juce::Image::ARGB,int(std::ceil((width+8)*scale)),int(std::ceil((height+8)*scale)),true);
                    juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(scale));
                    const auto n=midi_preview::project(width,height,.4f,.40001f,pitch,0,127,drum);
                    CHECK(!n.empty());
                    paintMidiPreviewNote(g,{4+n.x,4+n.y,n.width,n.height},n.radius,juce::Colours::white.withAlpha(.95f));
                    int visible=0;
                    for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)
                        visible+=image.getPixelAt(x,y).getAlpha()>32;
                    CHECK(visible>=1);
                }
        for(bool drum:{false,true}) {
            juce::Image image(juce::Image::ARGB,800,100,true);juce::Graphics g(image);
            g.fillAll(juce::Colour(0xff101520));g.addTransform(juce::AffineTransform::scale(2.0f));
            g.setColour(juce::Colour(0xffb23e48));g.fillRoundedRectangle({4,3,390,18},4);
            g.setColour(juce::Colour(0xffdd747b));g.fillRoundedRectangle({4,23,390,4},1.5f);
            const int pitches[]{50,60,67,62,55,72,64,60,48,65,76,57};
            for(int i=0;i<12;++i){
                const float start=i/12.0f+.01f,end=start+.045f;
                const auto n=midi_preview::project(390,16,start,end,pitches[i],48,76,drum);
                paintMidiPreviewNote(g,{4+n.x,4+n.y,n.width,n.height},n.radius,juce::Colour(0xffffe8e8));
            }
            const auto file=out.getChildFile(drum?"midi-preview-drums.png":"midi-preview-notes.png");
            file.deleteFile();juce::FileOutputStream stream(file);CHECK(stream.openedOk());
            CHECK(juce::PNGImageFormat{}.writeImageToStream(image,stream));
        }
        std::cout<<"PASS "<<count<<" production MIDI preview raster assertions (JUCE Graphics)\n";
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
