#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ayra::media_edit {

using Id = std::string;
using Tick = std::int64_t;
// Exact common clock for 44.1/48/96/192 kHz and common integer/NTSC frame rates.
// Importers must retain original media time bases and convert only at boundaries.
inline constexpr Tick ticksPerSecond = 282240000;

struct Word {
    Id id;
    std::string text;
    Tick sourceStart {}, sourceEnd {}; // Half-open source interval, never timeline time.
    bool operator==(const Word&) const = default;
};
struct Transcript {
    Id id, sourceId;
    int audioStream {};
    std::vector<Word> words;
    bool operator==(const Transcript&) const = default;
};
// An audio instance also represents the audio part of a linked video clip.
// Link/compound/sequence ownership is deliberately not implemented in this slice.
struct AudioClip {
    Id id, sourceId;
    int audioStream {};
    Tick sourceIn {}, sourceOut {}, timelineStart {};
    bool muted {};
    bool operator==(const AudioClip&) const = default;
};
struct CaptionBinding {
    Id id, audioClipId, transcriptId;
    std::string stylePresetId;
    bool enabled {true};
    bool operator==(const CaptionBinding&) const = default;
};
struct Document {
    Id sessionId;
    std::uint64_t revision {};
    std::vector<AudioClip> audioClips;
    std::vector<Transcript> transcripts;
    std::vector<CaptionBinding> captionBindings;
    bool operator==(const Document&) const = default;
};
struct Version {
    Id sessionId;
    std::uint64_t revision {};
};
struct CaptionCue {
    Id bindingId, audioClipId, wordId;
    std::string text, stylePresetId;
    Tick start {}, end {};
    bool partialWord {}; // Surface an edit-through-word warning to the future UI.
    bool operator==(const CaptionCue&) const = default;
};

namespace detail {
inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
inline Tick add(Tick a, Tick b) {
    require(a >= 0 && b >= 0, "Negative time");
    if (b > std::numeric_limits<Tick>::max() - a)
        throw std::overflow_error("Timeline time overflow");
    return a + b;
}
template <typename T>
const T& byId(const std::vector<T>& items, const Id& id) {
    const auto it = std::find_if(items.begin(), items.end(), [&](const T& v) { return v.id == id; });
    if (it == items.end()) throw std::invalid_argument("Unknown referenced ID");
    return *it;
}
template <typename T>
T& byId(std::vector<T>& items, const Id& id) {
    const auto it = std::find_if(items.begin(), items.end(), [&](const T& v) { return v.id == id; });
    if (it == items.end()) throw std::invalid_argument("Unknown referenced ID");
    return *it;
}
template <typename T>
void uniqueIds(const std::vector<T>& items) {
    std::unordered_set<Id> ids;
    for (const auto& item : items)
        require(!item.id.empty() && ids.insert(item.id).second, "Empty or duplicate ID");
}
} // namespace detail

inline void validate(const Document& document) {
    using detail::require;
    require(!document.sessionId.empty(), "Empty session ID");
    detail::uniqueIds(document.audioClips);
    detail::uniqueIds(document.transcripts);
    detail::uniqueIds(document.captionBindings);
    for (const auto& clip : document.audioClips) {
        require(!clip.sourceId.empty() && clip.audioStream >= 0, "Invalid audio source");
        require(clip.sourceIn >= 0 && clip.sourceOut > clip.sourceIn, "Invalid source range");
        (void) detail::add(clip.timelineStart, clip.sourceOut - clip.sourceIn);
    }
    for (const auto& transcript : document.transcripts) {
        require(!transcript.sourceId.empty() && transcript.audioStream >= 0, "Invalid transcript source");
        detail::uniqueIds(transcript.words);
        Tick previousStart = 0;
        for (const auto& word : transcript.words) {
            require(!word.text.empty() && word.sourceStart >= previousStart &&
                    word.sourceEnd > word.sourceStart, "Invalid or unsorted word interval");
            previousStart = word.sourceStart;
        }
    }
    for (const auto& binding : document.captionBindings) {
        const auto& clip = detail::byId(document.audioClips, binding.audioClipId);
        const auto& transcript = detail::byId(document.transcripts, binding.transcriptId);
        require(clip.sourceId == transcript.sourceId && clip.audioStream == transcript.audioStream,
                "Caption source/stream mismatch");
        require(!binding.stylePresetId.empty(), "Empty caption style ID");
    }
}

// Unity-rate prototype: no speed ramps, nested compositions or transcription.
// Every caller obtains a fresh projection from canonical source-bound words.
inline std::vector<CaptionCue> projectCaptions(const Document& document) {
    validate(document);
    std::vector<CaptionCue> cues;
    for (const auto& binding : document.captionBindings) {
        if (!binding.enabled) continue;
        const auto& clip = detail::byId(document.audioClips, binding.audioClipId);
        const auto& transcript = detail::byId(document.transcripts, binding.transcriptId);
        for (const auto& word : transcript.words) {
            if (word.sourceStart >= clip.sourceOut) break;
            const auto begin = std::max(word.sourceStart, clip.sourceIn);
            const auto end = std::min(word.sourceEnd, clip.sourceOut);
            if (begin >= end) continue;
            cues.push_back({binding.id, clip.id, word.id, word.text, binding.stylePresetId,
                           detail::add(clip.timelineStart, begin - clip.sourceIn),
                           detail::add(clip.timelineStart, end - clip.sourceIn),
                           begin != word.sourceStart || end != word.sourceEnd});
        }
    }
    std::stable_sort(cues.begin(), cues.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    return cues;
}

// Single-writer, control-thread owner. Snapshot history is bounded by count, not
// bytes; production media remains external, and a delta journal is planned.
class CaptionDocument {
public:
    explicit CaptionDocument(Document initial, std::size_t historyLimit = 100)
        : document_(std::move(initial)), historyLimit_(historyLimit) {
        validate(document_);
        detail::require(historyLimit_ > 0, "History limit must be positive");
    }
    const Document& document() const noexcept { return document_; }
    Version version() const { return {document_.sessionId, document_.revision}; }
    bool canUndo() const noexcept { return !undo_.empty(); }
    bool canRedo() const noexcept { return !redo_.empty(); }

    void move(const Version& expected, const Id& clipId, Tick timelineStart) {
        edit(expected, [&](Document& next) { detail::byId(next.audioClips, clipId).timelineStart = timelineStart; });
    }
    // Explicit placement: the host decides ripple/roll policy, not the caption engine.
    void trim(const Version& expected, const Id& clipId, Tick sourceIn, Tick sourceOut, Tick timelineStart) {
        edit(expected, [&](Document& next) {
            auto& clip = detail::byId(next.audioClips, clipId);
            clip.sourceIn = sourceIn; clip.sourceOut = sourceOut; clip.timelineStart = timelineStart;
        });
    }
    void split(const Version& expected, const Id& clipId, Tick timelineAt, const Id& rightClipId) {
        edit(expected, [&](Document& next) {
            auto& left = detail::byId(next.audioClips, clipId);
            const auto end = detail::add(left.timelineStart, left.sourceOut - left.sourceIn);
            detail::require(timelineAt > left.timelineStart && timelineAt < end, "Split must be inside clip");
            AudioClip right = left;
            right.id = rightClipId;
            right.sourceIn = detail::add(left.sourceIn, timelineAt - left.timelineStart);
            right.timelineStart = timelineAt;
            left.sourceOut = right.sourceIn;
            next.audioClips.push_back(std::move(right));
            std::vector<CaptionBinding> additions;
            for (const auto& binding : next.captionBindings) {
                if (binding.audioClipId != clipId) continue;
                auto copy = binding;
                copy.id += "/split/" + rightClipId;
                copy.audioClipId = rightClipId;
                additions.push_back(std::move(copy));
            }
            next.captionBindings.insert(next.captionBindings.end(), additions.begin(), additions.end());
        });
    }
    void eraseAudio(const Version& expected, const Id& clipId) {
        edit(expected, [&](Document& next) {
            (void) detail::byId(next.audioClips, clipId);
            std::erase_if(next.captionBindings, [&](const auto& b) { return b.audioClipId == clipId; });
            std::erase_if(next.audioClips, [&](const auto& c) { return c.id == clipId; });
            // Retain the shared transcript: other instances and Undo may still need it.
        });
    }
    void undo(const Version& expected) { restore(expected, undo_, redo_); }
    void redo(const Version& expected) { restore(expected, redo_, undo_); }

private:
    Document document_;
    std::vector<Document> undo_, redo_;
    std::size_t historyLimit_;

    void check(const Version& expected) const {
        detail::require(expected.sessionId == document_.sessionId && expected.revision == document_.revision,
                        "Stale revision or wrong session");
        if (document_.revision == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Revision overflow");
    }
    void remember(std::vector<Document>& history) {
        history.push_back(document_); // Allocate before touching the live document.
        if (history.size() > historyLimit_) history.erase(history.begin());
    }
    template <typename Operation>
    void edit(const Version& expected, Operation operation) {
        check(expected);
        Document next = document_;
        operation(next);
        validate(next);
        next.revision = document_.revision + 1;
        remember(undo_);
        document_ = std::move(next);
        redo_.clear();
    }
    void restore(const Version& expected, std::vector<Document>& from, std::vector<Document>& to) {
        check(expected);
        detail::require(!from.empty(), "No history available");
        Document next = from.back();
        next.revision = document_.revision + 1; // Undo never revives a stale revision.
        remember(to);
        document_ = std::move(next);
        from.pop_back();
    }
};

} // namespace ayra::media_edit
