/** @file LabModel.h
    Stato fixture e comandi del banco, indipendenti da JUCE e dal DSP.
    Un solo Store alimenta Timeline e Mixer. Le viste non possiedono i dati.
    Message-thread only; ID sintetici stabili, nessun accesso al documento Ayra.
    @see LabModel.cpp, tests/CoreTests.cpp, fixtures/session.json */
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ayra::lab
{
struct PanelFixtureMetadata; // Immutable, optional metadata for the separate timeline showcase.
enum class Role { songTrack, performerTrack, performerMix, songMain };
enum class Mode { grouped, free };
enum class Selection { replace, toggle, range };
enum class Flag { mute, solo, enabled, bypass, buffer, oversample, expanded, pinned };

struct Clip
{
    Clip() = default;
    Clip(std::string clipId, std::string clipName, double clipStart,
         double clipLength, int clipChannels, int clipSeed,
         double clipFadeIn = 0.0, double clipFadeOut = 0.0)
        : id(std::move(clipId)), name(std::move(clipName)), start(clipStart),
          length(clipLength), channels(clipChannels), seed(clipSeed),
          fadeIn(clipFadeIn), fadeOut(clipFadeOut)
    {
    }
    std::string id, name;
    double start = 0, length = 1;
    int channels = 1, seed = 1;
    double fadeIn = 0, fadeOut = 0;
    /** Distingue semanticamente una EditSource anche quando la finestra
        risultante da uno split non contiene eventi visibili. */
    bool container = false;
    /** Sotto-arrangement relativo all'inizio del container. Un vettore vuoto
        identifica una clip foglia. Nel prodotto questa semantica corrisponde
        a `ayra::timeline::EditSource`; qui resta il solo adapter fixture. */
    std::vector<Clip> children;
    [[nodiscard]] bool isContainer() const noexcept { return container; }
    bool operator==(const Clip&) const = default;
};
struct Track
{
    std::string id, name, group, groupName, route, monitor;
    Role role = Role::songTrack;
    std::uint32_t colour = 0xff16c95c;
    int height = 152, audioChannels = 4, midiPins = 0, oscPins = 0, dmxPins = 0, automations = 0;
    bool expanded = false, pinned = false, mute = false, solo = false;
    bool enabled = true, bypass = false, buffer = false, oversample = false;
    double gain = 0, pan = 0;
    std::vector<int> order;
    std::vector<Clip> clips;
    bool operator==(const Track&) const = default;
};
struct Session
{
    std::string songId;
    Mode mode = Mode::grouped;
    bool dark = true, groupCollapsed = false;
    int headerWidth = 352;
    double zoom = 40;
    std::string active, selectedClip;
    std::vector<std::string> selected, manualOrder;
    std::vector<Track> tracks;
    std::shared_ptr<const PanelFixtureMetadata> panelMetadata; // Not part of legacy Lab JSON v1.
    bool operator==(const Session&) const = default;
};
struct Capabilities
{
    bool mute = true, solo = true, reorder = false, resize = true;
    bool graph = true, route = true, automation = true, monitor = true;
};
struct Audibility
{
    bool program = true, cue = true;
    std::string reason;
};
Session makeFixture(); // generated from fixtures/session.json, never independently edited.
const char* roleKey(Role) noexcept;
const char* roleLabel(Role) noexcept;
bool parseRole(const std::string&, Role&) noexcept;
Capabilities capabilities(const Track&, Mode) noexcept;
const Track* findTrack(const Session&, const std::string&) noexcept;
Track* findTrack(Session&, const std::string&) noexcept;
const Clip* findClip(const Clip&, const std::string&) noexcept;
Clip* findClip(Clip&, const std::string&) noexcept;
const Clip* findClip(const Track&, const std::string&) noexcept;
Clip* findClip(Track&, const std::string&) noexcept;
const Clip* findClip(const Session&, const std::string& trackId,
                     const std::string& clipId) noexcept;
Clip* findClip(Session&, const std::string& trackId,
               const std::string& clipId) noexcept;
struct ClipSiblingScope
{
    std::vector<Clip>* clips = nullptr;
    std::string parentContainerId;
};
/** Trova il vettore canonico che possiede direttamente `clipId`. */
[[nodiscard]] ClipSiblingScope findClipSiblingScope(Track&, const std::string& clipId) noexcept;

struct ClipContainerEditResult
{
    bool ok = false;
    std::string containerId, message;
    explicit operator bool() const noexcept { return ok; }
};

/** Sostituisce clip sorelle con un container. I figli mantengono identita' e
    stato e diventano relativi all'inizio del container. `parentContainerId`
    vuoto indica la lane radice. */
[[nodiscard]] ClipContainerEditResult createClipContainer (
    Track&, const std::string& parentContainerId,
    const std::vector<std::string>& clipIds, std::string containerId);

/** Sostituisce un container con i figli diretti, riportandoli nelle coordinate
    del parent. Funziona a qualunque profondita'. */
[[nodiscard]] bool dissolveClipContainer (Track&, const std::string& containerId);

struct ClipSplitLineage
{
    std::string leftId, rightId;
    double fraction = 0.5;
};

struct ClipSplitEditResult
{
    bool ok = false;
    std::string rightClipId, message;
    std::vector<ClipSplitLineage> lineage;
    explicit operator bool() const noexcept { return ok; }
};

/** Divide una clip e, per una EditSource, partiziona ricorsivamente il
    sotto-arrangement. Nessun figlio intero compare in entrambe le meta'.
    `mintId` deve restituire un'identita' unica per ogni base richiesta. */
[[nodiscard]] ClipSplitEditResult splitClip (
    Track&, const std::string& clipId, double splitTime,
    const std::function<std::string(const std::string&)>& mintId);

/** Copia ricorsivamente tutti gli ID in un set gia' allocato dal chiamante. */
void collectClipIds(const Clip&, std::vector<std::string>& output);
std::vector<std::string> visibleOrder(const Session&);
Audibility audibility(const Session&, const Track&);
int minimumHeight(const Track&) noexcept;
int visualHeight(const Track&) noexcept;
std::vector<int> effectiveChannels(const Track&, const Clip&);
std::string validate(const Session&);

/** Undo bounded a 96 operazioni. Un gesto continuo prepara una Session candidata;
    commit una volta al rilascio. Nessun Undo se nulla e' cambiato; gli errori
    notificano la UI senza creare una transazione.
    callback sincrono sul thread UI: il consumer puo' rinviare il layout.
    @see commit, select */
class Store
{
public:
    explicit Store(Session = makeFixture());
    const Session& state() const noexcept { return current; }
    std::function<void()> changed;
    // Optional fixture-only preflight. May change the private candidate, never the live Store.
    // Return an error to reject atomically; must not call commit/undo/redo or change callbacks.
    std::function<std::string(const Session&, Session&)> prepareCommit;
    bool commit(Session candidate, std::string label);
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return !past.empty(); }
    bool canRedo() const noexcept { return !future.empty(); }
    const std::string& lastAction() const noexcept { return action; }
    const std::string& lastError() const noexcept { return error; }
    bool replace(Session); // import: strict validation, one Undo
    void select(const std::string&, Selection);
    void selectClip(const std::string&);
    bool removeClip(const std::string& trackId, const std::string& clipId);
    bool toggle(const std::string&, Flag);
    bool setGain(const std::string&, double);
    bool setPan(const std::string&, double);
    bool resize(const std::string&, int);
    bool rename(const std::string&, const std::string&);
    bool setMode(Mode);
    bool setOrder(const std::string&, const std::vector<int>&);
    bool move(const std::vector<std::string>& ids, const std::string& anchor, bool after);
private:
    struct Entry { Session state; std::string action; };
    Session current;
    std::vector<Entry> past, future;
    std::string action = "Fixture caricata", error, rangeAnchor;
    void notify();
};
} // namespace ayra::lab
