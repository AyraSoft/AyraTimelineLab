#include "CaptionDocument.h"
#include <functional>
#include <iostream>
#include <utility>
using namespace ayra::media_document_prototype;
#define REQUIRE(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(#__VA_ARGS__); } while (false)
static Document fixture() {
    return {"project", 0, 48000,
        {{"transcript", "voice", {{"w1", "Ciao", {100,120}}, {"w2", "Ayra", {120,150}}, {"w3", "oggi", {160,190}}}}},
        {{"clip", "voice", {100,200}, 1000}}, {{"caption", "clip", "transcript", "highlight", true}}};
}
static std::vector<Id> active(const std::vector<Cue>& cues, Tick at) {
    std::vector<Id> ids;
    for (const auto& c : cues) if (c.timeline.begin <= at && at < c.timeline.end) ids.push_back(c.word);
    std::sort(ids.begin(), ids.end());
    return ids;
}
int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"source-relative projection", [] {
            auto d=fixture(); const auto c=project(d);
            REQUIRE(c.size()==3); REQUIRE(c[0].timeline==Range{1000,1020}); REQUIRE(c[2].timeline==Range{1060,1090});
        }},
        {"move carries captions", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Move{"clip",2000}}));
            REQUIRE(project(d)[0].timeline==Range{2000,2020}); REQUIRE(d.revision==1);
        }},
        {"trim intersects source words", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Trim{"clip",{130,180},1030}})); const auto c=project(d);
            REQUIRE(c.size()==2); REQUIRE(c[0].timeline==Range{1030,1050}); REQUIRE(c[1].timeline==Range{1060,1080});
            REQUIRE(c[0].clippedAtEdit && c[1].clippedAtEdit);
        }},
        {"split at word boundary", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Split{"clip",1020,"right","right-caption"}})); const auto c=project(d);
            REQUIRE(d.clips.size()==2 && d.bindings.size()==2 && c.size()==3);
            REQUIRE(c[1].clip=="right" && c[1].timeline==Range{1020,1050});
        }},
        {"split inside word uses half-open fragments", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Split{"clip",1030,"right","right-caption"}})); const auto c=project(d);
            REQUIRE(c.size()==4); REQUIRE(c[1].word==c[2].word);
            REQUIRE(c[1].timeline==Range{1020,1030}); REQUIRE(c[2].timeline==Range{1030,1050});
            REQUIRE(active(c,1030)==std::vector<Id>{"w2"});
        }},
        {"delete cascades bindings, retains transcript", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Erase{"clip"}}));
            REQUIRE(d.bindings.empty() && project(d).empty()); REQUIRE(d.transcripts.size()==1);
        }},
        {"duplicate creates independent caption ownership", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Duplicate{"clip","copy","copy-caption",2000},Move{"clip",3000}}));
            REQUIRE(project(d).size()==6); REQUIRE(project(d)[0].clip=="copy");
            REQUIRE(project(d)[0].timeline.begin==2000); REQUIRE(d.transcripts.size()==1 && d.revision==1);
        }},
        {"delete one instance does not delete another", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Duplicate{"clip","copy","copy-caption",2000},Erase{"clip"}}));
            REQUIRE(d.clips.size()==1 && d.bindings.size()==1 && project(d).size()==3);
            REQUIRE(project(d)[0].clip=="copy");
        }},
        {"failed batch rolls back all edits", [] {
            auto d=fixture(); const auto old=d;
            REQUIRE(!apply(d,0,{Move{"clip",2000},Erase{"missing"}})); REQUIRE(d==old);
        }},
        {"stale revision rejected", [] {
            auto d=fixture(); const auto old=d; REQUIRE(!apply(d,8,{Erase{"clip"}})); REQUIRE(d==old);
        }},
        {"invalid bindings and word identities rejected", [] {
            auto d=fixture(); d.transcripts[0].asset="other"; REQUIRE(!validate(d));
            d=fixture(); d.bindings.push_back({"duplicate","clip","transcript","basic",true}); REQUIRE(!validate(d));
            d=fixture(); d.transcripts[0].words[1].id="w1"; REQUIRE(!validate(d));
            d=fixture(); d.bindings[0].clip="missing"; REQUIRE(!validate(d));
        }},
        {"overflow and negative ranges rejected without mutation", [] {
            auto d=fixture(); const auto old=d;
            REQUIRE(!apply(d,0,{Move{"clip",maxTick}})); REQUIRE(d==old);
            REQUIRE(!apply(d,0,{Move{"clip",-1}})); REQUIRE(d==old);
            REQUIRE(!apply(d,0,{Trim{"clip",{-1,180},1000}})); REQUIRE(d==old);
        }},
        {"invalid split and duplicate identifiers are atomic", [] {
            auto d=fixture(); const auto old=d;
            REQUIRE(!apply(d,0,{Split{"clip",1000,"right","binding"}})); REQUIRE(d==old);
            REQUIRE(!apply(d,0,{Split{"clip",1100,"right","binding"}})); REQUIRE(d==old);
            REQUIRE(!apply(d,0,{Split{"clip",1030,"clip","binding"}})); REQUIRE(d==old);
            REQUIRE(!apply(d,0,{Duplicate{"clip","copy","caption",2000}})); REQUIRE(d==old);
        }},
        {"disabled captions retain ownership", [] {
            auto d=fixture(); d.bindings[0].enabled=false; REQUIRE(project(d).empty());
            REQUIRE(apply(d,0,{Duplicate{"clip","copy","copy-caption",2000},Erase{"clip"}}));
            REQUIRE(d.bindings.size()==1 && !d.bindings[0].enabled && project(d).empty());
        }},
        {"uncaptioned clips can split", [] {
            auto d=fixture(); d.bindings.clear(); REQUIRE(apply(d,0,{Split{"clip",1030,"right",""}}));
            REQUIRE(d.clips.size()==2 && d.bindings.empty());
        }},
        {"every split preserves exact active-word coverage", [] {
            const auto original=fixture(); const auto baseline=project(original);
            for (Tick cut=1001;cut<1100;++cut) {
                auto d=original; REQUIRE(apply(d,0,{Split{"clip",cut,"right","right-caption"}}));
                const auto projected=project(d);
                for(Tick tick=999;tick<=1101;++tick) REQUIRE(active(projected,tick)==active(baseline,tick));
            }
        }},
        {"successful batch increments revision once", [] {
            auto d=fixture(); REQUIRE(apply(d,0,{Move{"clip",2000},Trim{"clip",{120,190},2020}}));
            REQUIRE(d.revision==1 && project(d).size()==2); REQUIRE(apply(d,1,{})); REQUIRE(d.revision==1);
        }},
        {"invalid input rejected by projection", [] {
            auto d=fixture(); d.bindings[0].transcript="missing"; bool caught=false;
            try { (void)project(d); } catch(const std::invalid_argument&) { caught=true; }
            REQUIRE(caught);
        }},
        {"revision exhaustion does not wrap", [] {
            auto d=fixture(); d.revision=std::numeric_limits<std::uint64_t>::max(); const auto old=d;
            REQUIRE(!apply(d,d.revision,{Erase{"clip"}})); REQUIRE(d==old);
        }},
        {"source trim cannot invent unavailable handles", [] {
            auto d=fixture(); const auto old=d;
            REQUIRE(!apply(d,0,{Trim{"clip",{90,200},1000}})); REQUIRE(d==old);
        }}
    };
    std::size_t failed=0;
    for(const auto& [name,run] : tests) {
        try { run(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    }
    std::cout<<tests.size()-failed<<'/'<<tests.size()<<" contract cases passed\n";
    return failed ? 1 : 0;
}
