#pragma once
// Isolated executable contract, not the Lab's canonical document or media engine.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ayra::media_document_prototype {
using Id = std::string;
using Tick = std::int64_t;
inline constexpr Tick maxTick = std::numeric_limits<Tick>::max() / 4;
struct Range {
    Tick begin = 0, end = 0; // Half-open [begin, end), in the document tick domain.
    bool operator==(const Range&) const = default;
};
struct Word {
    Id id;
    std::string text;
    Range source;
    bool operator==(const Word&) const = default;
};
struct Transcript {
    Id id, asset;
    std::vector<Word> words;
    bool operator==(const Transcript&) const = default;
};
struct Clip {
    Id id, asset;
    Range source;
    Tick start = 0; // Prototype supports forward 1x only, not retiming.
    bool operator==(const Clip&) const = default;
};
struct Binding {
    Id id, clip, transcript, style;
    bool enabled = true;
    bool operator==(const Binding&) const = default;
};
struct Document {
    Id id;
    std::uint64_t revision = 0;
    std::int64_t ticksPerSecond = 48000;
    std::vector<Transcript> transcripts;
    std::vector<Clip> clips;
    std::vector<Binding> bindings;
    bool operator==(const Document&) const = default;
};
struct Result {
    bool ok = true;
    std::string error;
    explicit operator bool() const noexcept { return ok; }
    static Result fail(std::string error) { return {false, std::move(error)}; }
};
template<class T> const T* find(const std::vector<T>& values, const Id& id) {
    const auto i = std::find_if(values.begin(), values.end(), [&](const auto& v) { return v.id == id; });
    return i == values.end() ? nullptr : &*i;
}
inline bool validRange(Range r) { return 0 <= r.begin && r.begin < r.end && r.end <= maxTick; }
inline Result validate(const Document& d) {
    if (d.id.empty() || d.ticksPerSecond <= 0 || d.ticksPerSecond > 1000000000)
        return Result::fail("Invalid document identity or tick domain");
    std::unordered_set<Id> transcripts, clips, bindings, boundClips;
    for (const auto& t : d.transcripts) {
        if (t.id.empty() || t.asset.empty() || !transcripts.insert(t.id).second)
            return Result::fail("Invalid transcript identity");
        std::unordered_set<Id> words;
        for (const auto& w : t.words)
            if (w.id.empty() || w.text.empty() || !words.insert(w.id).second || !validRange(w.source))
                return Result::fail("Invalid source word");
    }
    for (const auto& c : d.clips) {
        if (c.id.empty() || c.asset.empty() || !clips.insert(c.id).second || !validRange(c.source))
            return Result::fail("Invalid clip identity or source range");
        if (c.start < 0 || c.start > maxTick - (c.source.end - c.source.begin))
            return Result::fail("Timeline range overflow");
    }
    for (const auto& b : d.bindings) {
        const auto* c = find(d.clips, b.clip);
        const auto* t = find(d.transcripts, b.transcript);
        if (b.id.empty() || b.style.empty() || !bindings.insert(b.id).second || !boundClips.insert(b.clip).second
            || !c || !t || c->asset != t->asset)
            return Result::fail("Invalid caption ownership, transcript or asset");
    }
    return {};
}
struct Cue {
    Id binding, clip, word;
    std::string text;
    Range timeline;
    bool clippedAtEdit = false;
    bool operator==(const Cue&) const = default;
};
// Read-only derived view. Invalid external documents are rejected, never repaired silently.
inline std::vector<Cue> project(const Document& d) {
    if (const auto r = validate(d); !r) throw std::invalid_argument(r.error);
    std::vector<Cue> cues;
    for (const auto& b : d.bindings) {
        if (!b.enabled) continue;
        const auto& c = *find(d.clips, b.clip);
        const auto& t = *find(d.transcripts, b.transcript);
        for (const auto& w : t.words) {
            const Range kept{std::max(c.source.begin, w.source.begin), std::min(c.source.end, w.source.end)};
            if (kept.begin >= kept.end) continue;
            cues.push_back({b.id, c.id, w.id, w.text,
                {c.start + kept.begin - c.source.begin, c.start + kept.end - c.source.begin},
                kept != w.source});
        }
    }
    std::stable_sort(cues.begin(), cues.end(), [](const auto& a, const auto& b) {
        if (a.timeline.begin != b.timeline.begin) return a.timeline.begin < b.timeline.begin;
        if (a.clip != b.clip) return a.clip < b.clip;
        return a.word < b.word;
    });
    return cues;
}
struct Move { Id clip; Tick start; };
struct Trim { Id clip; Range source; Tick start; }; // Shrink only; source handles require an asset catalogue.
struct Split { Id clip; Tick at; Id rightClip, rightBinding; }; // at is a timeline tick.
struct Erase { Id clip; };
struct Duplicate { Id clip, newClip, newBinding; Tick start; };
using Command = std::variant<Move, Trim, Split, Erase, Duplicate>;
namespace detail {
// Called only on a validated working copy; callers must not invoke directly.
inline Result applyOne(Document& d, const Command& command) {
    return std::visit([&](const auto& op) -> Result {
        using T = std::decay_t<decltype(op)>;
        auto it = std::find_if(d.clips.begin(), d.clips.end(), [&](const auto& c) { return c.id == op.clip; });
        if (it == d.clips.end()) return Result::fail("Unknown clip");
        const Clip before = *it;
        if constexpr (std::is_same_v<T, Move>) {
            it->start = op.start;
        } else if constexpr (std::is_same_v<T, Trim>) {
            if (!validRange(op.source) || op.source.begin < before.source.begin || op.source.end > before.source.end)
                return Result::fail("Trim must stay within the current source range");
            it->source = op.source;
            it->start = op.start;
        } else if constexpr (std::is_same_v<T, Erase>) {
            d.clips.erase(it);
            std::erase_if(d.bindings, [&](const auto& b) { return b.clip == op.clip; });
        } else {
            Clip added = before;
            Id bindingId;
            if constexpr (std::is_same_v<T, Split>) {
                const Tick end = before.start + before.source.end - before.source.begin;
                if (op.at <= before.start || op.at >= end) return Result::fail("Split must be strictly inside the clip");
                const Tick sourceAt = before.source.begin + (op.at - before.start);
                it->source.end = sourceAt;
                added.id = op.rightClip;
                added.source.begin = sourceAt;
                added.start = op.at;
                bindingId = op.rightBinding;
            } else {
                added.id = op.newClip;
                added.start = op.start;
                bindingId = op.newBinding;
            }
            auto b = std::find_if(d.bindings.begin(), d.bindings.end(), [&](const auto& value) { return value.clip == op.clip; });
            if (b != d.bindings.end()) {
                Binding copy = *b;
                copy.id = bindingId;
                copy.clip = added.id;
                d.bindings.push_back(std::move(copy));
            }
            d.clips.push_back(std::move(added));
        }
        return {};
    }, command);
}
} // namespace detail
// One successful batch publishes one revision. Any failure leaves the input unchanged.
inline Result apply(Document& d, std::uint64_t expectedRevision, const std::vector<Command>& commands) {
    if (const auto r = validate(d); !r) return r;
    if (d.revision != expectedRevision) return Result::fail("Stale document revision");
    if (commands.empty()) return {};
    if (d.revision == std::numeric_limits<std::uint64_t>::max()) return Result::fail("Revision overflow");
    auto next = d;
    for (const auto& command : commands) {
        if (const auto r = detail::applyOne(next, command); !r) return r;
        if (const auto r = validate(next); !r) return r;
    }
    ++next.revision;
    d = std::move(next);
    return {};
}
} // namespace ayra::media_document_prototype
