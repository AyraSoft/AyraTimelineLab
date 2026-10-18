/** @file TrackHeader.cpp
    Geometria unica, controlli JUCE e macchina del resize cancellabile.
    ID bloccato durante il gesto, commit unico. @see TrackHeader.h */
#include "TrackHeader.h"
#include <algorithm>

namespace ayra::lab
{
HeaderLayout HeaderLayout::calculate(int w,int h,bool compact,bool hasSolo)
{
    HeaderLayout r;
    if(w<320 || h<100)return r;
    r.grip={12,13,15,23};r.disclosure={28,13,18,23};
    r.name={80,8,std::max(25,w-124),23};r.pin={w-37,11,30,24};
    int y=compact?47:63,x=14;
    const std::array<int,7> widths{hasSolo?26:68,hasSolo?26:0,30,34,0,48,54};
    for(std::size_t i=0;i<widths.size();++i)
    {
        int width=widths[i];
        if(i==4)width=std::max(2,w-x-118);
        r.controls[i]={x,y,width,24};if(width>0)x+=width+5;
    }
    y+=28;
    const int gainWidth=(w-32)*3/5;
    r.gain={12,y,gainWidth,25};r.pan={18+gainWidth,y,w-gainWidth-31,25};y+=27;
    r.monitor={14,y,91,23};r.automations={111,y,94,23};r.settings={w-36,y,27,23};
    r.advanced[0]={14,y+30,66,23};r.advanced[1]={85,y+30,48,23};
    r.resize={0,h-7,w,7};return r;
}
TrackHeader::TrackHeader(LabLookAndFeel& l):laf(l)
{
    setWantsKeyboardFocus(true);setTitle("Header traccia");
    name.setFont(juce::Font(juce::FontOptions().withHeight(14).withStyle("Bold")));
    name.setEditable(false,false,false);name.setInterceptsMouseClicks(false,false);addAndMakeVisible(name);
    const std::array<std::pair<juce::TextButton*,HeaderAction>,13> bindings{{
        {&disclosure,HeaderAction::expanded},{&mute,HeaderAction::mute},{&solo,HeaderAction::solo},{&power,HeaderAction::enabled},
        {&bypass,HeaderAction::bypass},{&graph,HeaderAction::graph},{&routing,HeaderAction::routing},
        {&automations,HeaderAction::automations},{&settings,HeaderAction::settings},{&pin,HeaderAction::pinned},
        {&buffer,HeaderAction::buffer},{&oversample,HeaderAction::oversample},{&grip,HeaderAction::select}}};
    for(auto [button,action]:bindings)
    {
        addAndMakeVisible(*button);
        button->onClick=[this,action]{send(action);};
    }
    grip.onClick={};grip.addMouseListener(this,false);
    mute.setTooltip("Mute diretto; Song Main silenzia soltanto il Program");
    solo.setTooltip("Solo diretto della traccia");power.setTooltip("Enable del Tensor fixture");bypass.setTooltip("Bypass della catena fixture");
    graph.setTooltip("Apri il Graph fixture nell'Inspector");routing.setTooltip("Percorso logico: solo lettura nel banco");
    settings.setTooltip("Nome, colore, buffer, oversampling e pin");disclosure.setTooltip("Espandi/ripiega lane e controlli");
    for(auto* slider:{&gain,&pan})
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,43,21);
        addAndMakeVisible(*slider);
    }
    gain.setName("Main gain fixture");gain.setRange(-60,12,.1);gain.setTextValueSuffix(" dB");
    pan.setName("Main pan fixture");pan.setRange(-1,1,.01);
    gain.setDoubleClickReturnValue(true,0);pan.setDoubleClickReturnValue(true,0);
    gain.onDragEnd=[this]{if(!syncing)send(HeaderAction::gain,gain.getValue());};
    pan.onDragEnd=[this]{if(!syncing)send(HeaderAction::pan,pan.getValue());};
    gain.onValueChange=[this]{if(!syncing&&!gain.isMouseButtonDown())send(HeaderAction::gain,gain.getValue());};
    pan.onValueChange=[this]{if(!syncing&&!pan.isMouseButtonDown())send(HeaderAction::pan,pan.getValue());};
    int i=1;for(const auto* value:{"Auto","Live","Playback","Merge"})monitor.addItem(value,i++);
    monitor.setTooltip("Monitoring del bus audio fixture");addAndMakeVisible(monitor);
    monitor.onChange=[this]{if(!syncing)send(HeaderAction::monitor,0,monitor.getText().toStdString());};
}
TrackHeader::~TrackHeader(){grip.removeMouseListener(this);}
void TrackHeader::send(HeaderAction a,double value,std::string text,Selection selection)
{
    auto callback=onIntent;
    const auto id=model.id;
    if(callback)callback({a,id,std::move(text),value,selection});
}
void TrackHeader::setModel(const Track& t,Mode m,bool sel)
{
    if(model.id!=t.id && resizing)cancelResize();
    model=t;mode=m;selected=sel;syncing=true;
    name.setText(juce::String::fromUTF8(model.name.c_str()),juce::dontSendNotification);
    setTitle(juce::String::fromUTF8(model.name.c_str()));setDescription(roleLabel(model.role));
    const auto cap=capabilities(t,m);
    solo.setVisible(cap.solo);mute.setButtonText(t.role==Role::songMain?"M program":"M");
    grip.setEnabled(cap.reorder);grip.setTooltip(cap.reorder?"Trascina per riordinare":"Riordino disponibile in Libera su tracce non pinnate");
    mute.setToggleState(t.mute,juce::dontSendNotification);solo.setToggleState(t.solo,juce::dontSendNotification);
    mute.setColour(juce::TextButton::buttonOnColourId,laf.colours().mute);solo.setColour(juce::TextButton::buttonOnColourId,laf.colours().solo);
    power.setToggleState(t.enabled,juce::dontSendNotification);power.setButtonText(t.enabled?"ON":"OFF");
    bypass.setToggleState(t.bypass,juce::dontSendNotification);buffer.setToggleState(t.buffer,juce::dontSendNotification);oversample.setToggleState(t.oversample,juce::dontSendNotification);
    pin.setVisible(t.role==Role::songMain);pin.setToggleState(t.pinned,juce::dontSendNotification);pin.setButtonText(t.pinned?"PIN":"FREE");
    disclosure.setButtonText(t.expanded?"v":">");
    automations.setButtonText("Autom. "+juce::String(t.automations));
    buffer.setVisible(t.expanded);oversample.setVisible(t.expanded);
    if(!gain.isMouseButtonDown())gain.setValue(t.gain,juce::dontSendNotification);
    if(!pan.isMouseButtonDown())pan.setValue(t.pan,juce::dontSendNotification);
    gain.setColour(juce::Slider::trackColourId,juce::Colour(t.colour));gain.setColour(juce::Slider::thumbColourId,juce::Colour(t.colour));
    pan.setColour(juce::Slider::trackColourId,juce::Colour(t.colour));pan.setColour(juce::Slider::thumbColourId,juce::Colour(t.colour));
    monitor.setText(juce::String(t.monitor),juce::dontSendNotification);
    syncing=false;resized();repaint();
}
void TrackHeader::paint(juce::Graphics& g){laf.drawHeader(g,getLocalBounds(),model,selected);}
void TrackHeader::resized()
{
    const auto l=HeaderLayout::calculate(getWidth(),getHeight(),model.height<148,model.role!=Role::songMain);
    grip.setBounds(l.grip);disclosure.setBounds(l.disclosure);name.setBounds(l.name);pin.setBounds(l.pin);
    mute.setBounds(l.controls[0]);solo.setBounds(l.controls[1]);power.setBounds(l.controls[2]);bypass.setBounds(l.controls[3]);graph.setBounds(l.controls[5]);routing.setBounds(l.controls[6]);
    gain.setBounds(l.gain);pan.setBounds(l.pan);monitor.setBounds(l.monitor);automations.setBounds(l.automations);settings.setBounds(l.settings);
    buffer.setBounds(l.advanced[0]);oversample.setBounds(l.advanced[1]);
}
void TrackHeader::mouseDown(const juce::MouseEvent& e)
{
    if(e.mods.isPopupMenu()){cancelResize();send(HeaderAction::settings);return;}
    if(!e.mods.isLeftButtonDown())return;
    grabKeyboardFocus();
    if(e.eventComponent==&grip)return;
    const auto l=HeaderLayout::calculate(getWidth(),getHeight(),model.height<148,model.role!=Role::songMain);
    if(l.resize.contains(e.getPosition()))
    {
        resizing=true;startY=e.getScreenY();startHeight=model.height;previewHeight=startHeight;return;
    }
    const auto selection=e.mods.isShiftDown()?Selection::range:(e.mods.isCommandDown()||e.mods.isCtrlDown())?Selection::toggle:Selection::replace;
    send(HeaderAction::select,0,{},selection);
}
void TrackHeader::mouseMove(const juce::MouseEvent& e)
{
    const auto l=HeaderLayout::calculate(getWidth(),getHeight(),model.height<148,model.role!=Role::songMain);
    setMouseCursor(l.resize.contains(e.getPosition())?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);
}
void TrackHeader::mouseDrag(const juce::MouseEvent& e)
{
    if(e.eventComponent==&grip && e.mods.isLeftButtonDown() && capabilities(model,mode).reorder && e.getDistanceFromDragStart()>4)
    {
        if(auto* container=juce::DragAndDropContainer::findParentDragContainerFor(this);container && !container->isDragAndDropActive())
            container->startDragging(juce::String(model.id),&grip);
        return;
    }
    if(!resizing)return;
    previewHeight=std::clamp(startHeight+e.getScreenY()-startY,minimumHeight(model),1200);
    send(HeaderAction::resizePreview,previewHeight);
}
void TrackHeader::mouseUp(const juce::MouseEvent&)
{
    if(!resizing)return;resizing=false;send(HeaderAction::resizeCommit,previewHeight);
}
void TrackHeader::cancelResize(){if(!resizing)return;resizing=false;send(HeaderAction::resizeCancel);}
bool TrackHeader::isEditing()const noexcept{return resizing||gain.isMouseButtonDown()||pan.isMouseButtonDown();}
void TrackHeader::focusLost(FocusChangeType){cancelResize();}
bool TrackHeader::keyPressed(const juce::KeyPress& k)
{
    if(k==juce::KeyPress::escapeKey&&resizing){cancelResize();return true;}
    if(!hasKeyboardFocus(false))return false;
    if(k.getKeyCode()==juce::KeyPress::F2Key){send(HeaderAction::rename);return true;}
    if(k.getKeyCode()==juce::KeyPress::returnKey){send(HeaderAction::graph);return true;}
    if(k.getModifiers().isAltDown()&&k.getKeyCode()==juce::KeyPress::upKey){send(HeaderAction::stepUp);return true;}
    if(k.getModifiers().isAltDown()&&k.getKeyCode()==juce::KeyPress::downKey){send(HeaderAction::stepDown);return true;}
    return false;
}
}
