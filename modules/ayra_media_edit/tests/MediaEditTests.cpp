#include "AudioRouting.h"
#include <functional>
#include <iostream>
#include <limits>

using namespace ayra::media_edit;
namespace {
void expect(bool value) { if (!value) throw std::runtime_error("Expectation failed"); }
template <typename Fn> void rejects(Fn fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    catch (const std::overflow_error&) { rejected = true; }
    expect(rejected);
}
Document fixture() {
    // Small integer ticks intentionally isolate timeline algebra from codecs.
    return {"session", 0,
        {{"voice", "asset", 0, 100, 500, 1000, false}},
        {{"transcript", "asset", 0, {
            {"before", "before", 50, 100}, {"a", "Ciao", 100, 150},
            {"b", "Ayra", 200, 300}, {"c", "oggi", 350, 450},
            {"after", "after", 500, 550}}}},
        {{"captions", "voice", "transcript", "word-highlight", true}}};
}
RoutingPlan routing() {
    return {{{"voice"}, {"music"}, {"master"}},
            {{"voice", "master"}, {"music", "master"}},
            {{"voice", "music", -24.0, 4.0, 10.0, 200.0}}};
}
} // namespace
int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests {
        {"source words project with half-open boundaries", [] {
            auto cues = projectCaptions(fixture()); expect(cues.size() == 3);
            expect(cues[0].start == 1000 && cues[2].end == 1350 && !cues[0].partialWord);
        }},
        {"move follows without modifying transcript", [] {
            CaptionDocument editor(fixture()); auto words = editor.document().transcripts;
            editor.move(editor.version(), "voice", 2000);
            expect(projectCaptions(editor.document())[1].start == 2100);
            expect(editor.document().transcripts == words);
        }},
        {"trim clips partial words and preserves originals", [] {
            CaptionDocument editor(fixture()); editor.trim(editor.version(), "voice", 225, 400, 0);
            auto cues = projectCaptions(editor.document()); expect(cues.size() == 2);
            expect(cues[0].start == 0 && cues[0].end == 75 && cues[0].partialWord);
            expect(cues[1].end == 175 && cues[1].partialWord);
            expect(editor.document().transcripts[0].words.size() == 5);
        }},
        {"split in silence preserves cue intervals", [] {
            CaptionDocument editor(fixture()); auto before = projectCaptions(editor.document());
            editor.split(editor.version(), "voice", 1225, "right"); auto after = projectCaptions(editor.document());
            expect(after.size() == before.size());
            for (std::size_t i = 0; i < after.size(); ++i)
                expect(before[i].start == after[i].start && before[i].end == after[i].end);
            expect(after[2].audioClipId == "right");
        }},
        {"split inside word produces flagged nonoverlapping fragments", [] {
            CaptionDocument editor(fixture()); editor.split(editor.version(), "voice", 1150, "right");
            auto cues = projectCaptions(editor.document()); expect(cues.size() == 4);
            expect(cues[1].end == 1150 && cues[2].start == 1150);
            expect(cues[1].wordId == cues[2].wordId && cues[1].partialWord && cues[2].partialWord);
        }},
        {"moving right split does not move left captions", [] {
            CaptionDocument editor(fixture()); editor.split(editor.version(), "voice", 1225, "right");
            editor.move(editor.version(), "right", 2000); auto cues = projectCaptions(editor.document());
            expect(cues[0].start == 1000 && cues[2].start == 2025);
        }},
        {"deleting audio cascades bindings only", [] {
            CaptionDocument editor(fixture()); editor.eraseAudio(editor.version(), "voice");
            expect(projectCaptions(editor.document()).empty());
            expect(editor.document().captionBindings.empty() && editor.document().transcripts.size() == 1);
        }},
        {"deleting one split preserves the other", [] {
            CaptionDocument editor(fixture()); editor.split(editor.version(), "voice", 1225, "right");
            editor.eraseAudio(editor.version(), "voice"); auto cues = projectCaptions(editor.document());
            expect(cues.size() == 1 && cues[0].audioClipId == "right");
        }},
        {"undo redo restore dependencies with monotonic revisions", [] {
            CaptionDocument editor(fixture()); auto before = projectCaptions(editor.document());
            editor.eraseAudio(editor.version(), "voice"); editor.undo(editor.version());
            expect(projectCaptions(editor.document()) == before && editor.version().revision == 2);
            editor.redo(editor.version()); expect(projectCaptions(editor.document()).empty());
            expect(editor.version().revision == 3);
        }},
        {"stale and cross-session commands reject atomically", [] {
            CaptionDocument editor(fixture()); auto stale = editor.version(); editor.move(stale, "voice", 2000);
            auto before = editor.document(); rejects([&] { editor.move(stale, "voice", 0); });
            rejects([&] { editor.eraseAudio({"other", 1}, "voice"); }); expect(editor.document() == before);
        }},
        {"invalid split IDs roll back source trims and history", [] {
            CaptionDocument editor(fixture()); auto before = editor.document();
            rejects([&] { editor.split(editor.version(), "voice", 1200, "voice"); });
            expect(editor.document() == before && !editor.canUndo());
        }},
        {"endpoint split is rejected", [] {
            CaptionDocument editor(fixture());
            rejects([&] { editor.split(editor.version(), "voice", 1000, "right"); });
            rejects([&] { editor.split(editor.version(), "voice", 1400, "right"); });
        }},
        {"invalid trim and move roll back", [] {
            CaptionDocument editor(fixture()); auto before = editor.document();
            rejects([&] { editor.trim(editor.version(), "voice", 400, 300, 0); });
            rejects([&] { editor.move(editor.version(), "voice", -1); }); expect(before == editor.document());
        }},
        {"source and audio-stream mismatches are rejected", [] {
            auto d = fixture(); d.transcripts[0].sourceId = "different"; rejects([&] { validate(d); });
            d = fixture(); d.transcripts[0].audioStream = 1; rejects([&] { validate(d); });
        }},
        {"dangling or duplicate identities are rejected", [] {
            auto d = fixture(); d.audioClips.clear(); rejects([&] { validate(d); });
            d = fixture(); d.captionBindings.push_back(d.captionBindings[0]); rejects([&] { validate(d); });
        }},
        {"unsorted negative and zero-duration words are rejected", [] {
            auto d = fixture(); std::swap(d.transcripts[0].words[0], d.transcripts[0].words[1]);
            rejects([&] { validate(d); }); d = fixture(); d.transcripts[0].words[0].sourceStart = -1;
            rejects([&] { validate(d); }); d = fixture(); d.transcripts[0].words[0].sourceEnd = 50;
            rejects([&] { validate(d); });
        }},
        {"muting does not delete captions; disable is explicit", [] {
            auto d = fixture(); d.audioClips[0].muted = true; expect(projectCaptions(d).size() == 3);
            d.captionBindings[0].enabled = false; expect(projectCaptions(d).empty());
        }},
        {"time and revision overflow reject without mutation", [] {
            CaptionDocument editor(fixture()); auto before = editor.document();
            rejects([&] { editor.move(editor.version(), "voice", std::numeric_limits<Tick>::max()); });
            expect(editor.document() == before); auto d = fixture(); d.revision = std::numeric_limits<std::uint64_t>::max();
            CaptionDocument full(d); rejects([&] { full.move(full.version(), "voice", 0); });
        }},
        {"bounded history and branching undo", [] {
            CaptionDocument editor(fixture(), 1); editor.move(editor.version(), "voice", 2000);
            editor.move(editor.version(), "voice", 3000); editor.undo(editor.version());
            expect(!editor.canUndo() && editor.canRedo()); editor.move(editor.version(), "voice", 4000);
            expect(!editor.canRedo()); rejects([&] { editor.redo(editor.version()); });
        }},
        {"shared transcript supports independent placements", [] {
            auto d = fixture(); d.audioClips.push_back({"second", "asset", 0, 100, 500, 2000, false});
            d.captionBindings.push_back({"second-captions", "second", "transcript", "basic", true});
            CaptionDocument editor(d); editor.eraseAudio(editor.version(), "voice");
            auto cues = projectCaptions(editor.document()); expect(cues.size() == 3 && cues[0].start == 2000);
        }},
        {"valid bus-to-bus ducking graph", [] { validateRouting(routing()); }},
        {"sidechain self-feedback rejected", [] {
            auto p = routing(); p.ducking[0].targetBus = "voice"; rejects([&] { validateRouting(p); });
        }},
        {"indirect mixed send and sidechain feedback rejected", [] {
            auto p = routing(); p.sends.push_back({"master", "voice"}); rejects([&] { validateRouting(p); });
        }},
        {"opposite sidechains rejected", [] {
            auto p = routing(); p.ducking.push_back({"music", "voice"}); rejects([&] { validateRouting(p); });
        }},
        {"unknown and duplicate bus/send rejected", [] {
            auto p = routing(); p.ducking[0].triggerBus = "missing"; rejects([&] { validateRouting(p); });
            p = routing(); p.buses.push_back({"voice"}); rejects([&] { validateRouting(p); });
            p = routing(); p.sends.push_back(p.sends[0]); rejects([&] { validateRouting(p); });
        }},
        {"invalid ducking values rejected", [] {
            auto p = routing(); p.ducking[0].ratio = std::numeric_limits<double>::quiet_NaN();
            rejects([&] { validateRouting(p); }); p = routing(); p.ducking[0].releaseMs = 0;
            rejects([&] { validateRouting(p); }); p = routing(); p.ducking[0].ratio = 0.5;
            rejects([&] { validateRouting(p); });
        }},
        {"duplicate ducking route rejected", [] {
            auto p = routing(); p.ducking.push_back(p.ducking[0]); rejects([&] { validateRouting(p); });
        }},
        {"clock exactly represents common media sample and frame rates", [] {
            for (const Tick rate : {44100, 48000, 96000, 192000, 24, 25, 30, 50, 60, 24000, 30000, 60000})
                expect(ticksPerSecond % rate == 0);
        }},
        {"deterministic repeated splits and removals stay source-bound", [] {
            auto d = fixture(); d.audioClips[0].sourceIn = 0; d.audioClips[0].sourceOut = 10000;
            d.audioClips[0].timelineStart = 0; d.transcripts[0].words.clear();
            for (int i = 0; i < 100; ++i)
                d.transcripts[0].words.push_back({std::to_string(i), "word", i * 100 + 10, i * 100 + 60});
            CaptionDocument editor(d); auto before = projectCaptions(d);
            for (int i = 99; i > 0; --i) editor.split(editor.version(), "voice", i * 100, "part" + std::to_string(i));
            auto after = projectCaptions(editor.document()); expect(after.size() == before.size());
            for (std::size_t i = 0; i < after.size(); ++i) expect(after[i].start == before[i].start && after[i].end == before[i].end);
            for (int i = 1; i < 100; i += 2) editor.eraseAudio(editor.version(), "part" + std::to_string(i));
            expect(projectCaptions(editor.document()).size() == 50);
        }}
    };
    std::size_t failed = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    std::cout << (tests.size() - failed) << '/' << tests.size() << " cases passed\n";
    return failed == 0 ? 0 : 1;
}
