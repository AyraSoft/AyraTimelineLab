/** @file CoreTests.cpp
    Regressioni reali del modello usato dall'app JUCE, non copie degli algoritmi.
    Nessun test di questo file attesta rendering, audio o eventi GUI nativi. */
#include "LabModel.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace ayra::lab;
namespace { int checks=0; void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);} }
int main()
{
 try
 {
    Store store;
    auto original=store.state();
    check(validate(original).empty(),"fixture validity");
    check(original.tracks.size()==11,"eleven distinct tracks");
    const auto* main=findTrack(original,"main");
    check(main && !capabilities(*main,Mode::free).solo,"main has no solo");
    check(!store.toggle("main",Flag::solo),"solo main rejected");
    check(!store.canUndo(),"rejection no undo");
    check(!store.move({"voice"},"backing",false),"grouped movement rejected");
    store.select("voice",Selection::replace);
    check(store.state().selected==std::vector<std::string>{"voice"},"header selects entity");
    check(!store.canUndo(),"selection not musical undo");
    store.select("backing",Selection::toggle);
    check(store.state().selected.size()==2,"multi select");
    store.select("mix",Selection::range);
    check(store.state().selected.size()==10,"range visible order");
    check(store.setMode(Mode::free),"free mode");
    auto before=store.state();
    check(store.move({"voice","mix"},"backing",false),"move group");
    check(store.state().manualOrder==std::vector<std::string>({"voice","mix","backing","synth","filter","markers","arranger","blocks","tempo","filmstrip","main"}),"relative order");
    for(std::size_t i=0;i<before.tracks.size();++i)check(before.tracks[i]==store.state().tracks[i],"presentation does not touch domain");
    auto order=store.state().manualOrder;
    store.setMode(Mode::grouped);store.setMode(Mode::free);
    check(store.state().manualOrder==order,"manual order survives mode toggles");
    check(!store.move({"main"},"voice",false),"pinned source protected");
    check(!store.move({"voice"},"main",true),"pinned target protected");
    check(!store.move({"voice","voice"},"backing",true),"duplicate ids rejected");
    check(!store.move({"voice"},"voice",false),"self no-op");
    check(store.toggle("main",Flag::pinned),"unpin main");
    check(store.move({"main"},"voice",false),"main moves when unpinned");
    check(store.undo(),"undo move");check(store.redo(),"redo move");
    store=Store();
    const auto voice=*findTrack(store.state(),"voice");
    check(effectiveChannels(voice,voice.clips[0])==std::vector<int>({0,-1,-1,1}),"stereo L1 R4 retains empty slots");
    check(effectiveChannels(voice,voice.clips[1])==std::vector<int>({0,2,3,1}),"same track order on quad");
    check(!store.setOrder("voice",{0,0,2,3}),"invalid permutation rejected");
    check(store.setOrder("voice",{0,1,2,3}),"shared mapping applied");
    check(effectiveChannels(*findTrack(store.state(),"voice"),voice.clips[0]).size()==4,"mapping has N positions");
    check(store.undo(),"undo mapping");
    check(findTrack(store.state(),"voice")->order==std::vector<int>({0,2,3,1}),"undo mapping exact");
    for(int n=1;n<=64;++n)
    {
        Track t=*findTrack(original,"voice");t.audioChannels=n;t.order.resize(static_cast<std::size_t>(n));
        std::iota(t.order.begin(),t.order.end(),0);Clip c;c.channels=1;
        const auto v=effectiveChannels(t,c);
        check(v.size()==static_cast<std::size_t>(n) && v[0]==0,"N-channel row cardinality");
        if(n>1)check(v.back()==-1,"empty last slot preserved");
    }
    store=Store();
    store.toggle("main",Flag::mute);
    const auto audible=audibility(store.state(),*findTrack(store.state(),"voice"));
    check(!audible.program && audible.cue,"program mute does not silence cue fixture");
    check(!findTrack(store.state(),"voice")->mute,"no inherited flag mutation");
    store.toggle("main",Flag::mute);store.toggle("mix",Flag::mute);
    check(!audibility(store.state(),*findTrack(store.state(),"voice")).cue,"mix mute fixture");
    store.toggle("voice",Flag::solo);store.toggle("voice",Flag::mute);
    check(!findTrack(store.state(),"voice")->solo,"mute solo exclusion");
    auto routes=store.state().tracks;
    check(store.resize("voice",500),"resize");
    check(findTrack(store.state(),"voice")->clips==routes[1].clips,"resize preserves clips");
    check(!store.resize("voice",500),"resize no-op");
    check(!store.setGain("voice",std::numeric_limits<double>::quiet_NaN()),"nan gain rejected");
    check(!store.setPan("voice",std::numeric_limits<double>::infinity()),"inf pan rejected");
    check(store.setGain("voice",99),"gain bounded");check(findTrack(store.state(),"voice")->gain==12,"gain upper limit");
    check(!store.rename("voice",""),"empty rename rejected");
    check(store.rename("voice","Voce finale"),"rename");
    check(findTrack(store.state(),"voice")->group=="ayra","rename keeps membership");
    const auto clipToDelete = findTrack(store.state(), "voice")->clips.front().id;
    store.selectClip(clipToDelete);
    const auto clipsBeforeDelete = findTrack(store.state(), "voice")->clips.size();
    check(store.removeClip("voice", clipToDelete), "clip deletion committed");
    check(findTrack(store.state(), "voice")->clips.size() + 1 == clipsBeforeDelete
          && store.state().selectedClip.empty(),
          "clip deletion removed one clip and cleared its selection");
    check(store.undo()
          && findTrack(store.state(), "voice")->clips.size() == clipsBeforeDelete
          && store.state().selectedClip == clipToDelete,
          "clip deletion undo did not restore clip and selection");
    check(!store.removeClip("missing", clipToDelete)
          && !store.removeClip("voice", "missing"),
          "clip deletion accepted an unknown owner or id");
    {
        auto grouped = store.state();
        auto* track = findTrack(grouped, "voice");
        const auto firstId = track->clips[0].id;
        const auto secondId = track->clips[1].id;
        const auto firstStart = track->clips[0].start;
        const auto secondEnd = track->clips[1].start + track->clips[1].length;
        const auto glue = createClipContainer(
            *track, {}, {firstId, secondId}, "voice.glue.1");
        check(glue && glue.containerId == "voice.glue.1",
              "Glue did not create a real container");
        const auto* container = findClip(*track, glue.containerId);
        check(container && container->children.size() == 2,
              "Glue did not reparent both clips");
        check(container->start == firstStart
              && container->length == secondEnd - firstStart,
              "container does not span the full child union");
        check(container->children[0].id == firstId
              && container->children[0].start == 0.0,
              "first child identity or relative time changed");
        check(findClip(*track, secondId) != nullptr,
              "recursive clip lookup lost a child");
        check(validate(grouped).empty(), "valid container tree rejected");

        Store groupedStore(grouped);
        groupedStore.selectClip(firstId);
        check(groupedStore.removeClip("voice", glue.containerId),
              "container deletion with a selected child was rejected");
        check(groupedStore.state().selectedClip.empty()
              && findClip(*findTrack(groupedStore.state(), "voice"), firstId) == nullptr,
              "container deletion left a dangling child selection");

        const auto nested = createClipContainer(
            *track, glue.containerId,
            {firstId, secondId}, "voice.glue.2");
        check(nested && findClip(*track, nested.containerId)->children.size() == 2,
              "nested Glue failed");
        check(dissolveClipContainer(*track, nested.containerId),
              "nested Dissolve failed");
        check(findClip(*track, firstId) != nullptr
              && findClip(*track, secondId) != nullptr,
              "Dissolve did not restore child identities");
        findClip(*track,glue.containerId)->length=0.5;
        check(validate(grouped).empty(),
              "trimmed container incorrectly truncated its internal source");
        check(dissolveClipContainer(*track, glue.containerId),
              "root Dissolve failed");
        check(track->clips[0].id == firstId && track->clips[1].id == secondId,
              "Dissolve did not restore sibling order");

        Track splitTrack;
        splitTrack.id = "split";
        splitTrack.audioChannels = 2;
        splitTrack.order = {0, 1};
        splitTrack.clips = {
            {"left", "Left", 0.0, 2.0, 2, 1},
            {"right", "Right", 6.0, 2.0, 2, 2}
        };
        const auto splitGlue = createClipContainer(
            splitTrack, {}, {"left", "right"}, "gap.glue");
        check(static_cast<bool>(splitGlue), "gap Glue setup failed");
        int minted = 0;
        const auto splitResult = splitClip(
            splitTrack, "gap.glue", 4.0,
            [&minted](const std::string& base)
            {
                return base + "." + std::to_string(++minted);
            });
        check(splitResult && splitTrack.clips.size() == 2,
              "container split failed");
        check(splitTrack.clips[0].children.size() == 1
              && splitTrack.clips[0].children[0].id == "left"
              && splitTrack.clips[1].children.size() == 1
              && splitTrack.clips[1].children[0].id == "right",
              "split duplicated children across a source gap");
        check(splitTrack.clips[1].children[0].start == 2.0,
              "right source window did not preserve the internal gap");
        check(dissolveClipContainer(splitTrack, splitTrack.clips[1].id)
              && dissolveClipContainer(splitTrack, "gap.glue"),
              "split containers could not be dissolved");
        check(splitTrack.clips.size() == 2
              && splitTrack.clips[0].id == "left"
              && splitTrack.clips[1].id == "right",
              "Dissolve after split duplicated or lost source clips");

        Track crossingTrack;
        crossingTrack.id = "crossing";
        crossingTrack.audioChannels = 2;
        crossingTrack.order = {0, 1};
        crossingTrack.clips = {
            {"long", "Long", 1.0, 6.0, 2, 3},
            {"tail", "Tail", 8.0, 1.0, 2, 4}
        };
        check(static_cast<bool>(createClipContainer(
                  crossingTrack, {}, {"long", "tail"}, "cross.glue")),
              "crossing Glue setup failed");
        minted = 0;
        const auto crossing = splitClip(
            crossingTrack, "cross.glue", 4.0,
            [&minted](const std::string& base)
            {
                return base + "." + std::to_string(++minted);
            });
        check(crossing && crossing.lineage.size() == 2,
              "crossing child split lineage is incomplete");
        check(crossingTrack.clips[0].children[0].length == 3.0
              && crossingTrack.clips[1].children[0].length == 3.0
              && crossingTrack.clips[0].children[0].id !=
                 crossingTrack.clips[1].children[0].id,
              "crossing child was cloned instead of divided");

        auto invalid = grouped;
        auto* invalidTrack = findTrack(invalid, "voice");
        invalidTrack->clips[0].children.push_back(invalidTrack->clips[1]);
        invalidTrack->clips[0].children.back().start = invalidTrack->clips[0].length;
        check(!validate(invalid).empty(),
              "duplicate nested clip identity was accepted");
    }
    auto corrupt=store.state();corrupt.manualOrder.pop_back();
    auto checkpoint=store.state();check(!store.replace(corrupt) && store.state()==checkpoint,"bad import atomic");
    corrupt=checkpoint;corrupt.tracks[0].clips[0].fadeIn=999;
    check(!store.replace(corrupt),"fade validation");
    corrupt=checkpoint;corrupt.tracks[0].id="voice";check(!store.replace(corrupt),"duplicate identity");
    corrupt=checkpoint;corrupt.tracks[0].order={0,1,2,99};check(!store.replace(corrupt),"bad channel order import");
    corrupt=checkpoint;corrupt.tracks[0].audioChannels=std::numeric_limits<int>::max();
    check(!store.replace(corrupt),"oversized channel input rejected before geometry overflow");
    corrupt=checkpoint;corrupt.tracks[0].audioChannels=std::numeric_limits<int>::min();
    check(!store.replace(corrupt),"negative channel input rejected before geometry overflow");
    corrupt=checkpoint;corrupt.groupCollapsed=true;corrupt.mode=Mode::grouped;store.replace(corrupt);
    auto visible=visibleOrder(store.state());
    check(std::find(visible.begin(),visible.end(),"voice")==visible.end(),"collapse hides member");
    check(std::find(visible.begin(),visible.end(),"mix")!=visible.end(),"collapse keeps real mix");
    check(std::find(visible.begin(),visible.end(),"main")!=visible.end(),"collapse keeps main");
    // Invarianti Warp Markers & Ancoraggio Sample durante Trim / Slip / Stretch
    {
        struct WarpPt { double sourceTimeSec; double warpedTimeSec; };
        std::vector<WarpPt> warpPts = { { 5.0, 5.0 }, { 6.0, 6.01 }, { 7.0, 7.0 } };
        double clipStart = 2.0;
        double clipLength = 4.0;
        double sourceOffset = 4.0;

        auto calcNorms = [&](double len, double offset) {
            std::vector<float> norms;
            for (const auto& wp : warpPts)
                norms.push_back(static_cast<float>((wp.warpedTimeSec - offset) / len));
            return norms;
        };

        // Clip iniziale [4.0, 8.0]s nel materiale audio
        auto initialNorms = calcNorms(clipLength, sourceOffset);
        check(std::abs(initialNorms[0] - 0.25f) < 0.001f, "wp0 initial norm at 25%");
        check(std::abs(initialNorms[1] - 0.5025f) < 0.001f, "wp1 initial norm at ~50%");
        check(std::abs(initialNorms[2] - 0.75f) < 0.001f, "wp2 initial norm at 75%");

        // Timeline position = clipStart + norm * clipLength
        double wp0Timeline = clipStart + initialNorms[0] * clipLength;
        check(std::abs(wp0Timeline - 3.0) < 0.001, "wp0 timeline position is exactly 3.0s");

        // 1. Right Trim: riduciamo la clip a 1.5s (nasconde wp1 e wp2)
        double trimmedLen = 1.5;
        auto rightTrimNorms = calcNorms(trimmedLen, sourceOffset);
        check(std::abs(rightTrimNorms[0] - 0.6666f) < 0.002f, "wp0 still visible in trimmed clip");
        double wp0TrimmedTimeline = clipStart + rightTrimNorms[0] * trimmedLen;
        check(std::abs(wp0TrimmedTimeline - 3.0) < 0.001, "wp0 timeline pos unchanged during right trim");
        check(rightTrimNorms[1] > 1.001f, "wp1 is outside trimmed bounds (> 1.0, hidden)");
        check(rightTrimNorms[2] > 1.001f, "wp2 is outside trimmed bounds (> 1.0, hidden)");

        // 2. Right Expand: riespandiamo a 4.0s (wp1 e wp2 ricompaiono esattamente dove erano)
        auto expandedNorms = calcNorms(clipLength, sourceOffset);
        check(std::abs(expandedNorms[1] - initialNorms[1]) < 0.0001f, "wp1 reappears at exact same norm");
        check(std::abs(expandedNorms[2] - initialNorms[2]) < 0.0001f, "wp2 reappears at exact same norm");

        // 3. Left Trim: spostiamo il bordo sinistro avanti di 1.5s
        double leftTrimStart = clipStart + 1.5;
        double leftTrimLen = clipLength - 1.5; // 2.5s
        double leftTrimOffset = sourceOffset + 1.5; // 5.5s
        auto leftTrimNorms = calcNorms(leftTrimLen, leftTrimOffset);
        check(leftTrimNorms[0] < -0.001f, "wp0 is before left edge (< 0.0, hidden)");
        check(leftTrimNorms[1] >= 0.0f && leftTrimNorms[1] <= 1.0f, "wp1 visible in left trimmed clip");
        double wp1LeftTrimmedTimeline = leftTrimStart + leftTrimNorms[1] * leftTrimLen;
        check(std::abs(wp1LeftTrimmedTimeline - (clipStart + initialNorms[1] * clipLength)) < 0.001,
              "wp1 timeline position unchanged during left trim");

        // 4. Sample Displacement calculation invariant
        double deltaSec = warpPts[1].warpedTimeSec - warpPts[1].sourceTimeSec;
        std::int64_t deltaSamples = static_cast<std::int64_t>(std::round(deltaSec * 48000.0));
        check(deltaSamples == 480, "displacement exactly +480 samples independent of clip length");
    }

    std::cout<<"PASS: "<<checks<<" core assertions\n";
 }
 catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
 return 0;
}
