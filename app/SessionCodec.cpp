/** @file SessionCodec.cpp
    Parser rigoroso e serializer del solo stato dimostrativo. @see SessionCodec.h */
#include "SessionCodec.h"
#include <stdexcept>
#include <cmath>

namespace ayra::lab
{
namespace
{
juce::var object(){return juce::var(new juce::DynamicObject());}
void put(juce::var& v,const char* name,juce::var value){v.getDynamicObject()->setProperty(name,std::move(value));}
juce::String utf8(const std::string& s){return juce::String::fromUTF8(s.c_str());}
juce::var strings(const std::vector<std::string>& v){juce::Array<juce::var> a;for(const auto& s:v)a.add(utf8(s));return a;}
const juce::var& get(const juce::var& v,const char* name)
{
    const auto* d=v.getDynamicObject();if(!d||!d->hasProperty(name))throw std::runtime_error(std::string("Campo mancante: ")+name);
    return d->getProperty(name);
}
std::string text(const juce::var& v,const char* name)
{
    const auto& value=get(v,name);if(!value.isString())throw std::runtime_error(std::string("Stringa richiesta: ")+name);
    return value.toString().toStdString();
}
bool flag(const juce::var& v,const char* name)
{
    const auto& value=get(v,name);if(!value.isBool())throw std::runtime_error(std::string("Boolean richiesto: ")+name);return static_cast<bool>(value);
}
double number(const juce::var& v,const char* name)
{
    const auto& value=get(v,name);if(!value.isInt()&&!value.isInt64()&&!value.isDouble())throw std::runtime_error(std::string("Numero richiesto: ")+name);return static_cast<double>(value);
}
int integer(const juce::var& v,const char* name)
{
    const auto n=number(v,name);if(!std::isfinite(n)||std::trunc(n)!=n||n<-1000000||n>1000000)throw std::runtime_error(std::string("Intero invalido: ")+name);return static_cast<int>(n);
}
const juce::Array<juce::var>& array(const juce::var& v,const char* name,int maximum)
{
    const auto* a=get(v,name).getArray();if(!a||a->size()>maximum)throw std::runtime_error(std::string("Array non valido: ")+name);return *a;
}
std::vector<std::string> readStrings(const juce::var& v,const char* key)
{
    std::vector<std::string> out;
    for(const auto& value:array(v,key,256)){if(!value.isString())throw std::runtime_error("ID non testuale");out.push_back(value.toString().toStdString());}return out;
}
}
juce::String encodeSession(const Session& s)
{
    auto root=object();put(root,"schema","ayra-timeline-lab/1");put(root,"songId",utf8(s.songId));put(root,"mode",s.mode==Mode::grouped?"grouped":"free");
    put(root,"dark",s.dark);put(root,"headerWidth",s.headerWidth);put(root,"groupCollapsed",s.groupCollapsed);put(root,"zoom",s.zoom);
    put(root,"selected",strings(s.selected));put(root,"active",utf8(s.active));put(root,"selectedClip",utf8(s.selectedClip));put(root,"manualOrder",strings(s.manualOrder));
    juce::Array<juce::var> tracks;
    for(const auto& t:s.tracks)
    {
        auto v=object();put(v,"id",utf8(t.id));put(v,"role",roleKey(t.role));put(v,"name",utf8(t.name));put(v,"group",utf8(t.group));put(v,"groupName",utf8(t.groupName));put(v,"route",utf8(t.route));put(v,"monitor",utf8(t.monitor));
        put(v,"colour","#"+juce::String::toHexString(static_cast<int>(t.colour&0xffffff)).paddedLeft('0',6));
        put(v,"height",t.height);put(v,"audioChannels",t.audioChannels);put(v,"midiPins",t.midiPins);put(v,"oscPins",t.oscPins);put(v,"dmxPins",t.dmxPins);put(v,"automations",t.automations);
        put(v,"expanded",t.expanded);put(v,"pinned",t.pinned);put(v,"mute",t.mute);put(v,"solo",t.solo);put(v,"enabled",t.enabled);put(v,"bypass",t.bypass);put(v,"buffer",t.buffer);put(v,"oversample",t.oversample);put(v,"gain",t.gain);put(v,"pan",t.pan);
        juce::Array<juce::var> order;for(int x:t.order)order.add(x);put(v,"order",order);
        juce::Array<juce::var> clips;
        for(const auto& c:t.clips)
        {
            auto cv=object();put(cv,"id",utf8(c.id));put(cv,"name",utf8(c.name));put(cv,"start",c.start);put(cv,"length",c.length);put(cv,"channels",c.channels);put(cv,"seed",c.seed);put(cv,"fadeIn",c.fadeIn);put(cv,"fadeOut",c.fadeOut);clips.add(cv);
        }
        put(v,"clips",clips);tracks.add(v);
    }
    put(root,"tracks",tracks);return juce::JSON::toString(root,false);
}
juce::Result decodeSession(const juce::String& input,Session& destination)
{
    if(input.getNumBytesAsUTF8()>2*1024*1024)return juce::Result::fail("File oltre 2 MiB");
    juce::var root;auto parsed=juce::JSON::parse(input,root);if(parsed.failed())return parsed;
    try
    {
        if(text(root,"schema")!="ayra-timeline-lab/1")return juce::Result::fail("Formato Lab non supportato");
        Session s;s.songId=text(root,"songId");const auto mode=text(root,"mode");
        if(mode!="grouped"&&mode!="free")return juce::Result::fail("Modalita' sconosciuta");
        s.mode=mode=="grouped"?Mode::grouped:Mode::free;s.dark=flag(root,"dark");s.groupCollapsed=flag(root,"groupCollapsed");s.headerWidth=integer(root,"headerWidth");s.zoom=number(root,"zoom");
        s.selected=readStrings(root,"selected");s.manualOrder=readStrings(root,"manualOrder");s.active=text(root,"active");s.selectedClip=text(root,"selectedClip");
        for(const auto& v:array(root,"tracks",256))
        {
            Track t;t.id=text(v,"id");if(!parseRole(text(v,"role"),t.role))return juce::Result::fail("Ruolo sconosciuto");
            t.name=text(v,"name");t.group=text(v,"group");t.groupName=text(v,"groupName");t.route=text(v,"route");t.monitor=text(v,"monitor");
            const auto colour=juce::String(text(v,"colour"));
            if(colour.length()!=7||!colour.startsWithChar('#')||!colour.substring(1).containsOnly("0123456789abcdefABCDEF"))return juce::Result::fail("Colore non valido");
            t.colour=0xff000000u|static_cast<std::uint32_t>(colour.substring(1).getHexValue32());
            t.height=integer(v,"height");t.audioChannels=integer(v,"audioChannels");t.midiPins=integer(v,"midiPins");t.oscPins=integer(v,"oscPins");t.dmxPins=integer(v,"dmxPins");t.automations=integer(v,"automations");
            t.expanded=flag(v,"expanded");t.pinned=flag(v,"pinned");t.mute=flag(v,"mute");t.solo=flag(v,"solo");t.enabled=flag(v,"enabled");t.bypass=flag(v,"bypass");t.buffer=flag(v,"buffer");t.oversample=flag(v,"oversample");t.gain=number(v,"gain");t.pan=number(v,"pan");
            for(const auto& x:array(v,"order",64)){if(!x.isInt())return juce::Result::fail("Indice canale non intero");t.order.push_back(static_cast<int>(x));}
            for(const auto& cv:array(v,"clips",64))
            {
                Clip c;c.id=text(cv,"id");c.name=text(cv,"name");c.start=number(cv,"start");c.length=number(cv,"length");c.channels=integer(cv,"channels");c.seed=integer(cv,"seed");c.fadeIn=number(cv,"fadeIn");c.fadeOut=number(cv,"fadeOut");t.clips.push_back(std::move(c));
            }
            s.tracks.push_back(std::move(t));
        }
        const auto error=validate(s);if(!error.empty())return juce::Result::fail(utf8(error));
        destination=std::move(s);return juce::Result::ok();
    }
    catch(const std::exception& e){return juce::Result::fail(juce::String::fromUTF8(e.what()));}
}
}
