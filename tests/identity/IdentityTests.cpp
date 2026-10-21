#include "ayra_ui.h"
#include "SharedChromeLookAndFeel.h"
#include "../../demo/TimelineIdentityArtwork.h"
#include "../../demo/TimelineLaneControls.h"
#include <cstdlib>
#include <iostream>
#include <array>
using namespace ayra::timeline_panel;
using ayra::lab::TimelineIdentityArtwork;
namespace
{
int checks=0;
#define CHECK(x) do {++checks; if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
void writePng(const juce::File& file,const juce::Image& image)
{
    file.deleteFile();juce::FileOutputStream output(file);CHECK(output.openedOk());
    CHECK(juce::PNGImageFormat{}.writeImageToStream(image,output));
}
void lane(juce::Graphics& g,juce::Rectangle<int> bounds,bool dark,juce::Colour colour,int kind)
{
    g.setColour(dark?juce::Colour(0xff101520):juce::Colour(0xffdde4ef));g.fillRoundedRectangle(bounds.toFloat(),6);
    juce::ColourGradient aura(colour.withAlpha(0.28f),{bounds.getX()+75.0f,bounds.getY()+32.0f},
                              colour.withAlpha(0.0f),{bounds.getX()+165.0f,bounds.getY()+32.0f},true);
    g.setGradientFill(aura);g.fillRoundedRectangle(bounds.toFloat(),6);
    if(kind<0)return;
    const auto b=bounds.reduced(28,8).toFloat();
    g.setColour(colour.withAlpha(0.34f));g.fillRoundedRectangle(b,4);
    g.setColour(colour.withAlpha(0.75f));
    if(kind==0){
        for(int x=0;x<b.getWidth();++x){const float h=3+16*std::abs(std::sin(x*0.071f)*std::sin(x*0.023f));
            g.drawVerticalLine(juce::roundToInt(b.getX()+x),b.getCentreY()-h,b.getCentreY()+h);}
    }else{
        for(int i=0;i<48;++i)g.fillRoundedRectangle(b.getX()+i*14,b.getY()+5+(i*11)%35,11,3,1);
    }
}
juce::Image showcase(bool dark,float scale,bool left)
{
    ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
    constexpr int width=960,height=414;
    juce::Image image(juce::Image::ARGB,juce::roundToInt(width*scale),juce::roundToInt(height*scale),true);
    juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(scale));
    g.fillAll(dark?juce::Colour(0xff090c12):juce::Colour(0xfff4f6fa));
    g.setColour(dark?juce::Colour(0xfff1f5f9):juce::Colour(0xff182033));
    g.setFont(juce::Font(juce::FontOptions().withHeight(17).withStyle("Bold")));
    g.drawText("Frosted lens / riferimento HTML",18,12,900,25,juce::Justification::centredLeft);
    const std::array<juce::String,4> performers{"Ayra","Ayra","Noah","Performer with an intentionally very long name"};
    const std::array<juce::String,4> tracks{juce::String::fromUTF8("Rhythm Guitars \xe2\x80\x94 Left Quad"),"Lead Vocals","Keys","Very long track label that must not increase the lens width"};
    const std::array<juce::Colour,4> types{juce::Colour(0xff38bdf8),juce::Colour(0xff16c95c),juce::Colour(0xfff03c3c),juce::Colour(0xffa84fd6)};
    for(int i=0;i<4;++i){
        const juce::Rectangle<int> bounds(16,48+i*76,928,64);
        lane(g,bounds,dark,types[i],i==0?-1:i%2);
        TimelineIdentityContent c; c.performer=performers[i];c.track=tracks[i];
        c.type=i==2?"MIDI":i==3?"DMX":"Audio";c.typeColour=types[i];c.onLeft=left;
        const auto layout=TimelineIdentityLayout::make(bounds,c,TimelineIdentityArtwork::typography());
        // Sample the real fixture underlay before compositing the lens. Explicit
        // rendering fixture, not decoded media or a screenshot of the whole app.
        const auto sample=layout.backdrop.expanded(15).getIntersection(bounds);
        const auto pixels=image.getClippedImage((sample.toFloat() * scale).toNearestInt()).createCopy();
        const auto blurred=TimelineIdentityArtwork::prepareUnderlay(pixels,scale,layout,sample);
        TimelineIdentityArtwork::drawUnderlay(g,layout,blurred,sample);
        TimelineIdentityArtwork::draw(g,c,layout,TimelineIdentityArtwork::prepare(c,layout));
    }
    const juce::Rectangle<int> bounds(16,358,928,24);lane(g,bounds,dark,juce::Colours::grey,-1);
    TimelineIdentityContent global;global.track="Markers";global.compact=true;global.onLeft=left;
    const auto layout=TimelineIdentityLayout::make(bounds,global,TimelineIdentityArtwork::typography());
    TimelineIdentityArtwork::draw(g,global,layout,TimelineIdentityArtwork::prepare(global,layout));
    g.setColour(dark?juce::Colour(0xff94a3b8):juce::Colour(0xff58677c));g.setFont(juce::Font(juce::FontOptions().withHeight(10)));
    g.drawText("Fixture JUCE: nome protetto e rinominabile / nessun header / palette di tipo indipendente",18,387,900,18,juce::Justification::centredLeft);
    return image;
}
juce::Image groupedShowcase(bool dark, bool namesOnLeft)
{
    namespace tl=ayra::timeline;
    ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
    juce::Image image(juce::Image::ARGB,1100,640,true);juce::Graphics g(image);
    g.fillAll(ayra::tokens::surfaceRecessed());
    g.setColour(ayra::tokens::textPrimary());g.setFont(ayra::tokens::type::body().boldened());
    g.drawText("Card centrata / controlli raggruppati / accento traccia",16,8,1050,24,juce::Justification::centredLeft);
    const std::array<int,4> heights{64,176,272,56};int top=44;
    for(int i=0;i<4;++i){
        juce::Graphics::ScopedSaveState save(g);g.setOrigin(12,top);
        const juce::Rectangle<int> row(0,0,1076,heights[i]);
        const juce::Colour owner(0xff5bb6bd);const juce::Colour type(i==2?0xfff03c3c:0xff16c95c);
        lane(g,row,dark,type,i==2?1:0);
        tl::TimelineChromeVisibility show;show.nameSide=namesOnLeft?tl::TimelineNameSide::left:tl::TimelineNameSide::right;
        tl::TimelineChromeCapabilities caps;
        const auto geometry=tl::layoutTimelineLaneChrome(row.getWidth(),row.getHeight(),caps,show);
        // Real common well and real glyph-only children, not a mock renderer.
        g.setColour(owner);g.fillRect(rectangle(geometry.leftAccent));
        for(auto well:{geometry.leftGroup,geometry.rightGroup})
            if(!well.empty())ayra::lab::TimelineLaneControls::drawGroup(g,rectangle(well).toFloat());
        for(int b=0;b<6;++b){
            auto button=ayra::lab::TimelineLaneControls::makeButton(static_cast<tl::TimelineLaneButton>(b));
            button->setBounds(rectangle(geometry.buttons[static_cast<std::size_t>(b)]));
            button->setToggleState((i==1&&b==3),juce::dontSendNotification);
            const auto art=button->createComponentSnapshot(button->getLocalBounds());
            g.drawImageAt(art,button->getX(),button->getY());
        }
        TimelineIdentityContent c;c.performer="Ayra";c.track=i==0?"Backing":i==1?"Voice":i==2?"Keys":"Minimum height";
        c.type=i==2?"MIDI":"Audio";c.typeColour=type;c.onLeft=namesOnLeft;
        const auto layout=TimelineIdentityLayout::make(rectangle(geometry.content),c,TimelineIdentityArtwork::typography());
        auto sample=layout.backdrop.expanded(15).getIntersection(rectangle(geometry.content));
        const auto pixels=image.getClippedImage(sample.translated(12,top)).createCopy();
        const auto underlay=TimelineIdentityArtwork::prepareUnderlay(pixels,1,layout,sample);
        TimelineIdentityArtwork::drawUnderlay(g,layout,underlay,sample);
        TimelineIdentityArtwork::draw(g,c,layout,TimelineIdentityArtwork::prepare(c,layout));
        top+=heights[i]+6;
    }
    return image;
}
juce::Image statusShowcase(bool dark,bool left,float scale)
{
    using namespace ayra::lab;
    ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
    const int width=1080,height=506;
    juce::Image image(juce::Image::ARGB,juce::roundToInt(width*scale),juce::roundToInt(height*scale),true);
    juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(scale));
    g.fillAll(dark?juce::Colour(0xff090c12):juce::Colour(0xfff4f6fa));
    g.setColour(ayra::tokens::textPrimary());g.setFont(ayra::tokens::type::body().boldened());
    g.drawText("Navigazione nella card / stato traccia passivo / controlli laterali nascosti",16,8,1040,26,juce::Justification::centredLeft);
    const std::array<juce::String,5> titles{"Backing","Lead Vocals","Keys","Guitar","Track with an intentionally long name"};
    for(int i=0;i<5;++i){
        const juce::Rectangle<int> row(12,42+i*82,1056,76);
        const auto colour=i==2?ayra::tokens::streamMidi():ayra::tokens::streamAudio();
        lane(g,row,dark,colour,i==2?1:0);
        g.setColour(juce::Colour(0xff5bb6bd));g.fillRoundedRectangle(row.withWidth(6).toFloat(),2);
        TimelineIdentityContent c;c.performer=i<3?"Ayra":"Noah";c.track=titles[i];c.type=i==2?"MIDI":"Audio";
        c.typeColour=colour;c.navigation=true;c.onLeft=left;
        c.muted=i==0||i==4;c.soloed=i==2||i==4;
        if(i==1){c.status.canvasActive=true;c.status.monitoringActive=true;}
        if(i==2){c.status.bufferized=true;c.status.oversampled=true;c.status.latencyCompensated=true;}
        if(i==3){c.status.shutdown=true;c.status.bypassed=true;}
        if(i==4)c.status={true,true,true,true,true,true,true};
        const auto layout=TimelineIdentityLayout::make(row.withTrimmedLeft(8),c,TimelineIdentityArtwork::typography());
        const auto sample=layout.backdrop.expanded(15).getIntersection(row);
        const auto pixels=image.getClippedImage((sample.toFloat()*scale).toNearestInt()).createCopy();
        const auto blur=TimelineIdentityArtwork::prepareUnderlay(pixels,scale,layout,sample);
        TimelineIdentityArtwork::drawUnderlay(g,layout,blur,sample);
        TimelineIdentityArtwork::draw(g,c,layout,TimelineIdentityArtwork::prepare(c,layout));
        for(int n=0;n<2;++n){
            auto button=TimelineLaneControls::makeButton(n==0?LaneButton::previous:LaneButton::next);
            button->setBounds(n==0?layout.previous:layout.next);
            auto art=button->createComponentSnapshot(button->getLocalBounds(),true,scale);
            g.drawImage(art,button->getBounds().toFloat());
        }
    }
    g.setColour(ayra::tokens::textSecondary());g.setFont(ayra::tokens::type::caption());
    g.drawText("Fixture nativa JUCE con stati dimostrativi / glifi Ayra originali selezionati / nessun motore audio",16,466,1040,24,juce::Justification::centredLeft);
    return image;
}

// The same material and typography at the exact event-widget heights. No
// image mockup renderer: these call the live identity painter and real buttons.
juce::Image adaptiveShowcase(bool dark,bool left)
{
    using namespace ayra::lab;
    ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
    juce::Image image(juce::Image::ARGB,1160,420,true);juce::Graphics g(image);
    g.fillAll(dark?juce::Colour(0xff090c12):juce::Colour(0xfff4f6fa));
    g.setColour(ayra::tokens::textPrimary());g.setFont(ayra::tokens::type::body().boldened());
    g.drawText("Altezza dal contenuto / card adattiva / stati sotto le frecce",16,10,1120,24,juce::Justification::centredLeft);
    int y=45;const std::array<int,5> heights{24,32,46,96,64};
    for(int i=0;i<5;++i){
        const juce::Rectangle<int> row(16,y,1128,heights[i]);
        const auto tint=i==0?ayra::tokens::streamOsc():i==1?ayra::tokens::streamDmx():ayra::tokens::streamAudio();
        g.setColour(dark?juce::Colour(0xff101520):juce::Colour(0xffdde4ef));g.fillRect(row);
        g.setColour(juce::Colour(0xff5bb6bd));g.fillRect(row.withWidth(6));
        if(i<=2){
            const int count=i==1?3:5;
            for(int n=0;n<count;++n){
                const int x=row.getX()+8+(i==1?n*360:n*205);
                const int py=row.getY()+3+((i==2&&n%2)?22:0);
                const auto pill=juce::Rectangle<int>(x,py,i==1?358:85,18);
                g.setColour(tint.withAlpha(.60f));g.fillRoundedRectangle(pill.toFloat(),4);
                g.setColour(ayra::tokens::textPrimary());g.setFont(juce::Font(juce::FontOptions().withHeight(11)));
                g.drawText(i==1?"Verse "+juce::String(n+1):"PC "+juce::String(n+1),pill,juce::Justification::centred);
                if(i==1){g.setColour(tint.withAlpha(.3f));g.fillRect(x,py+20,358,4);}
            }
        }
        TimelineIdentityContent c;c.performer="Noah";c.track=i<3?"Lights":"Backing vocals with a complete, intentionally long track name";
        c.type=i==0?"OSC":i==1?"DMX":i==2?"Text":"Audio";c.typeColour=tint;c.navigation=true;c.onLeft=left;
        c.muted=i==0||i>=3;c.soloed=i==4;if(i==0||i==3)c.status={true,true,true,true,true,true,true};
        c.expanded=i==4;
        const auto l=TimelineIdentityLayout::make(row.withTrimmedLeft(8),c,TimelineIdentityArtwork::typography());
        const auto sample=l.backdrop.expanded(15).getIntersection(row);
        const auto pix=image.getClippedImage(sample).createCopy();
        const auto blur=TimelineIdentityArtwork::prepareUnderlay(pix,1,l,sample);
        TimelineIdentityArtwork::drawUnderlay(g,l,blur,sample);
        TimelineIdentityArtwork::draw(g,c,l,TimelineIdentityArtwork::prepare(c,l));
        for(int n=0;n<2;++n){
            auto b=TimelineLaneControls::makeButton(n==0?LaneButton::previous:LaneButton::next);
            b->setBounds(n==0?l.previous:l.next);g.drawImageAt(b->createComponentSnapshot(b->getLocalBounds()),b->getX(),b->getY());
        }
        y+=heights[i]+6;
    }
    g.setColour(ayra::tokens::textSecondary());g.setFont(ayra::tokens::type::caption());
    g.drawText("Fixture nativa JUCE / 24, 32, 46, 96 e 64 px / ultima card espansa solo in orizzontale",16,375,1120,22,juce::Justification::centredLeft);
    return image;
}
void adaptiveGeometryContracts()
{
    const auto fonts=TimelineIdentityArtwork::typography();
    for(bool left:{false,true})for(int height:{24,32,39,40,46,64,400}) {
        TimelineIdentityContent c;c.performer="Noah";c.track="Lights";c.type="OSC";c.onLeft=left;c.navigation=true;c.muted=c.soloed=true;
        c.status={true,true,true,true,true,true,true};
        auto l=TimelineIdentityLayout::make({0,0,800,height},c,fonts);
        CHECK(l.twoLines==(height>=40));CHECK(l.backdrop.getHeight()==(height<40?22:40));
        CHECK(l.trackFont== (height<40?fonts.type:fonts.track));
        CHECK(l.mute.getRight()<l.previous.getX());CHECK(l.solo.getRight()<l.previous.getX());
        for(auto r:l.indicators){CHECK(!r.isEmpty());CHECK(r.getY()>=l.navigationGroup.getBottom());CHECK(l.backdrop.contains(r));}
        for(auto r:{l.performer,l.type,l.typeMark,l.mute,l.solo,l.previous,l.next})CHECK(!l.titleHit.intersects(r));
        CHECK(l.track.contains(l.titleHit));
        c.track="Full original track title which must be readable rather than truncated when hovered";
        const auto compact=TimelineIdentityLayout::make({0,0,1100,height},c,fonts);CHECK(compact.titleTruncated);
        c.expanded=true;const auto wide=TimelineIdentityLayout::make({0,0,1100,height},c,fonts);
        CHECK(wide.backdrop.getWidth()>compact.backdrop.getWidth());CHECK(!wide.titleTruncated);
        CHECK(wide.backdrop.getHeight()==compact.backdrop.getHeight());
        CHECK(wide.backdrop.getCentreY()==compact.backdrop.getCentreY());
        CHECK(left?wide.backdrop.getX()==compact.backdrop.getX():wide.backdrop.getRight()==compact.backdrop.getRight());
        const auto narrow=TimelineIdentityLayout::make({0,0,140,height},c,fonts);
        CHECK(narrow.backdrop.getWidth()<=140);CHECK(narrow.titleTruncated);
    }
}

}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto output=argc>1?juce::File(juce::String(argv[1])):juce::File::getCurrentWorkingDirectory();
    CHECK(output.createDirectory().wasOk());
    TimelineIdentityHoverState hover;
    CHECK(!hover.hidden());CHECK(!hover.update(true,false,false,0));CHECK(!hover.deadline());
    CHECK(!hover.update(true,true,false,100));CHECK(hover.deadline()==600);
    CHECK(!hover.update(true,true,false,400));CHECK(hover.deadline()==600);
    CHECK(!hover.update(true,true,true,590));CHECK(!hover.deadline());
    CHECK(!hover.update(true,true,true,4000));CHECK(!hover.hidden());
    CHECK(!hover.update(true,true,false,4010));CHECK(hover.deadline()==4510);
    CHECK(!hover.update(true,true,false,4509));CHECK(hover.update(true,true,false,4510));CHECK(hover.hidden());
    CHECK(!hover.update(true,false,false,5000));CHECK(hover.hidden());
    CHECK(!hover.update(true,true,true,5100));CHECK(hover.hidden()); // invisible arrows cannot re-open
    CHECK(hover.update(false,true,false,5200));CHECK(!hover.hidden());CHECK(!hover.deadline());
    CHECK(!hover.update(true,true,false,5300));CHECK(!hover.update(true,false,false,5400));CHECK(!hover.deadline());
    CHECK(hover.update(true,true,false,5500,true));CHECK(hover.hidden());hover.reset();CHECK(!hover.hidden());
    for(bool dark:{true,false}){
        ayra::tokens::setThemeMode(dark?ayra::tokens::Mode::Dark:ayra::tokens::Mode::Light);
        const auto fonts=TimelineIdentityArtwork::typography();
        for(int width:{0,12,44,120,240,540,1200})for(int height:{16,24,32,40,48,64,96,400})for(bool left:{false,true}){
            TimelineIdentityContent c;c.performer="Performer with an intentionally long name";c.track="Track with a very long name";
            c.type="Audio";c.typeColour=juce::Colour(0xff38bdf8);c.onLeft=left;
            const juce::Rectangle<int> area(13,17,width,height);
            const auto l=TimelineIdentityLayout::make(area,c,fonts);
            for(auto rect:{l.backdrop,l.track,l.performer,l.type,l.typeMark})CHECK(rect.isEmpty()||area.contains(rect));
            CHECK(l.backdrop.getWidth()<=TimelineIdentityLayout::maxWidth);
            CHECK(l.backdrop.getHeight()<=TimelineIdentityLayout::normalHeight);
            CHECK(l.trackFont.getHorizontalScale()==1.0f);
            CHECK(!l.twoLines||l.performer.isEmpty()||l.type.isEmpty()||!l.performer.intersects(l.type));
            CHECK(!l.twoLines||l.track.getY()>=l.performer.getBottom());
            CHECK(l.typeMark.isEmpty()||l.typeMark.getWidth()==5);
            const auto image=TimelineIdentityArtwork::prepare(c,l);CHECK(l.backdrop.isEmpty()||image.isValid());
            CHECK(l.backdrop.isEmpty()||std::abs(l.backdrop.getCentreY()-area.getCentreY())<=1);
            if(l.twoLines&&!l.type.isEmpty())CHECK(l.track.getRight()==l.type.getRight());
            if(image.isValid())for(int x=0;x<juce::jmin(18,image.getWidth());++x)
                for(int y=0;y<image.getHeight();++y)CHECK(image.getPixelAt(x,y).getAlpha()==0);
            if(width>=540&&height>=64){CHECK(l.twoLines);CHECK(std::abs(l.backdrop.getCentreY()-area.getCentreY())<=1);CHECK(l.trackFont.getHeightInPoints()==12.5f);}
        }
        for(bool left:{false,true})for(float scale:{1.0f,1.25f,1.5f,2.0f})
            writePng(output.getChildFile(juce::String(dark?"lens-dark-":"lens-light-")+(left?"left-":"right-")+juce::String(scale,2)+".png"),showcase(dark,scale,left));
    }
    for(int flags=0;flags<128;++flags)for(int ms=0;ms<4;++ms)for(bool left:{false,true}) {
        TimelineIdentityContent c;c.performer="Performer";c.track="Backing";c.type="Audio";c.navigation=true;c.onLeft=left;
        c.muted=(ms&1)!=0;c.soloed=(ms&2)!=0;
        c.status={bool(flags&1),bool(flags&2),bool(flags&4),bool(flags&8),bool(flags&16),bool(flags&32),bool(flags&64)};
        const auto l=TimelineIdentityLayout::make({0,0,800,96},c,TimelineIdentityArtwork::typography());
        CHECK(l.previous.getWidth()==16&&l.next.getHeight()==18);CHECK(l.previous.getRight()+2==l.next.getX());
        CHECK(l.type.getRight()<l.previous.getX());CHECK(l.backdrop.getWidth()<=255);CHECK(l.backdrop.getHeight()==40);
        CHECK(l.mute.isEmpty()==!c.muted);CHECK(l.solo.isEmpty()==!c.soloed);
        CHECK(!c.muted||l.mute.getX()>l.track.getRight());CHECK(!c.soloed||l.solo.getX()>l.track.getRight());
        const auto active=activeTrackIndicators(c.status);
        for(std::size_t i=0;i<active.size();++i){CHECK(l.indicators[i].isEmpty()==!active[i]);
            if(active[i]){CHECK(l.indicators[i].getWidth()==6);CHECK(l.indicators[i].getY()>=l.navigationGroup.getBottom() && !l.indicators[i].intersects(l.track));}}
    }
    for(bool dark:{true,false})for(bool left:{false,true})
        writePng(output.getChildFile(juce::String("grouped-")+(dark?"dark-":"light-")+(left?"left":"right")+".png"),groupedShowcase(dark,left));
    juce::Image stripes(juce::Image::ARGB,80,40,true);
    {juce::Graphics g(stripes);g.fillAll(juce::Colours::black);g.setColour(juce::Colours::white);
     for(int x=0;x<80;x+=2)g.fillRect(x,0,1,40);}
    const auto blur=TimelineIdentityArtwork::blurUnderlay(stripes,1);
    CHECK(blur.isValid());CHECK(std::abs(blur.getPixelAt(40,20).getFloatRed()-blur.getPixelAt(41,20).getFloatRed())<0.2f);
    // The blur itself must dissolve too, otherwise its crop reveals left corners.
    TimelineIdentityContent card;card.track="Backing";card.performer="Ayra";card.type="Audio";
    const auto layout=TimelineIdentityLayout::make({0,0,320,100},card,TimelineIdentityArtwork::typography());
    juce::Image flat(juce::Image::ARGB,320,100,true);{juce::Graphics g(flat);g.fillAll(juce::Colours::white);}
    const auto masked=TimelineIdentityArtwork::prepareUnderlay(flat,1,layout,{0,0,320,100});
    for(int y=layout.backdrop.getY();y<layout.backdrop.getBottom();++y)
        for(int x=layout.backdrop.getX();x<layout.backdrop.getX()+9;++x)CHECK(masked.getPixelAt(x,y).getAlpha()==0);
    CHECK(masked.getPixelAt(layout.backdrop.getRight()-12,layout.backdrop.getCentreY()).getAlpha()>240);
    card.onLeft=true;
    const auto mirrored=TimelineIdentityLayout::make({0,0,320,100},card,TimelineIdentityArtwork::typography());
    const auto inverted=TimelineIdentityArtwork::prepareUnderlay(flat,1,mirrored,{0,0,320,100});
    CHECK(mirrored.onLeft);
    CHECK(inverted.getPixelAt(mirrored.backdrop.getX()+12,mirrored.backdrop.getCentreY()).getAlpha()>240);
    for(int y=mirrored.backdrop.getY();y<mirrored.backdrop.getBottom();++y)
        for(int x=mirrored.backdrop.getRight()-9;x<mirrored.backdrop.getRight();++x)
            CHECK(inverted.getPixelAt(x,y).getAlpha()==0);
    // A single open V, never a closed transport triangle/bar.
    for(bool right:{false,true}) {
        auto path=ayra::lab::TimelineLaneControls::navigationPath({0,0,22,22},right);
        juce::Path::Iterator it(path);int moves=0,lines=0;
        while(it.next()){moves+=it.elementType==juce::Path::Iterator::startNewSubPath;lines+=it.elementType==juce::Path::Iterator::lineTo;CHECK(it.elementType!=juce::Path::Iterator::closePath);}
        CHECK(moves==1&&lines==2);
    }
    for(bool dark:{true,false})for(bool left:{false,true})for(float scale:{1.0f,2.0f})
        writePng(output.getChildFile(juce::String("status-")+(dark?"dark-":"light-")+(left?"left-":"right-")+juce::String(scale,1)+".png"),statusShowcase(dark,left,scale));
    adaptiveGeometryContracts();
    for(bool dark:{true,false})for(bool left:{false,true})
        writePng(output.getChildFile(juce::String("adaptive-")+(dark?"dark-":"light-")+(left?"left":"right")+".png"),adaptiveShowcase(dark,left));
    ayra::shadowCache().clear();juce::ImageCache::releaseUnusedImages();
    std::cout<<"PASS "<<checks<<" frosted lens layout/render/hover assertions; native JUCE painter fixtures\n";
}
