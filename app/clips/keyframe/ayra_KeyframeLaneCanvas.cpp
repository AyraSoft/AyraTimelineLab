/** @file ayra_KeyframeLaneCanvas.cpp
    Adapter del Lab per lane di eventi `pointKeyframe` (Marker/Counter),
    `contiguousSpans` (Program Change / Arranger contiguo da 0 a fine timeline)
    e `disjointSpans` (Sezioni/Blocchi a intervalli finiti con buchi, senza overlap). */

namespace ayra::lab
{
namespace
{

using clips::KeyframeEventVisual;
using clips::KeyframeLaneComponent;
using clips::KeyframeLaneType;
using clips::KeyframeLaneVisual;
using clips::TimelineClipZone;
namespace clip_midi_law = clips::clip_midi_law;
namespace clip_height_law = clips::clip_height_law;

struct KeyframeSourceEvent
{
    int id = 0;
    double startSec = 0.0;
    double lengthSec = 4.0;
    std::uint8_t programNumber = 0; // Usato per Program Change (0..127 -> mostrato PC 1..128)
    juce::String label;
    juce::Colour colour { juce::Colour (0xff38bdf8) };
    bool selected = false;
};

class KeyframeLaneCanvas final : public juce::Component,
                                 public juce::TooltipClient,
                                 public clips::KeyframeLanePresentationProvider
{
public:
    enum class HitZone
    {
        none,
        leftEdge,
        rightEdge,
        bottomRail,
        body
    };

    struct HitInfo
    {
        HitZone zone = HitZone::none;
        int index = -1;
    };

    [[nodiscard]] static constexpr TimelineClipZone timelineZoneForHit (
        const HitZone zone) noexcept
    {
        switch (zone)
        {
            case HitZone::leftEdge:  return TimelineClipZone::keyframeLeftEdge;
            case HitZone::rightEdge: return TimelineClipZone::keyframeRightEdge;
            case HitZone::bottomRail:return TimelineClipZone::keyframeCutRail;
            case HitZone::body:      return TimelineClipZone::keyframeBody;
            default:                 return TimelineClipZone::laneBackground;
        }
    }

    explicit KeyframeLaneCanvas (LabComponent& h, KeyframeLaneType type = KeyframeLaneType::contiguousSpans)
        : host (h), laneType (type)
    {
        setWantsKeyboardFocus (true);
        marqueeSelector.setFillColour (juce::Colour (0x3000d084));
        marqueeSelector.setOutlineColour (juce::Colour (0xe000d084));

        marqueeSelector.onPerformMarqueeSelection = [this] (const juce::Rectangle<int>& area,
                                                            ayra::MarqueeSelector::ModifierMode mode)
        {
            const auto laneBounds = keyframeLaneBoundsInCanvas();
            const bool isSpan = (laneType != KeyframeLaneType::points);
            bool anyChanged = false;

            for (size_t i = 0; i < cachedVisualEvents.size() && i < events.size(); ++i)
            {
                const auto pill = clips::keyframe_lane_layout::eventBounds (laneBounds, cachedVisualEvents[i], isSpan);
                const bool inside = area.intersects (pill.toNearestInt());

                if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
                    events[i].selected = inside;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Add && inside)
                    events[i].selected = true;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Subtract && inside)
                    events[i].selected = false;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Toggle && inside)
                    events[i].selected = !events[i].selected;

                anyChanged = true;
            }

            if (anyChanged)
            {
                rebuildVisualCache();
                repaint();
            }
        };

        marqueeSelector.onPerformClickSelection = [this] (juce::Point<int>,
                                                           ayra::MarqueeSelector::ModifierMode mode)
        {
            if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
            {
                for (auto& ev : events)
                    ev.selected = false;
                rebuildVisualCache();
                repaint();
            }
        };

        initDefaultEvents();
    }

    void setLaneType (KeyframeLaneType newType)
    {
        if (laneType == newType) return;
        laneType = newType;
        if (laneComponent)
            laneComponent->setLaneType (laneType);
        initDefaultEvents();
        rebuildVisualCache();
        repaint();
    }

    [[nodiscard]] KeyframeLaneType getLaneType() const noexcept { return laneType; }

    juce::String getTooltip() override
    {
        if (hoveredBottomRailIdx >= 0)
            return "Barra di Taglio Span / Split Tool\nClicca per tagliare questo blocco in due parti alla posizione del cursore";

        if (hoveredEventIdx >= 0)
        {
            if (laneType == KeyframeLaneType::contiguousSpans)
                return "Span Contiguo (Arranger / Program Change)\nTrascina il corpo per spostare il blocco; trascina i bordi per riposizionare le transizioni; doppio click per rinominare; Alt+Click per eliminare";
            if (laneType == KeyframeLaneType::disjointSpans)
                return "Span a Blocchi Disgiunti\nTrascina il corpo per spostare il blocco (riducendo i vicini se spinto a contatto); trascina i bordi per estendere/ridurre; barra sotto per tagliare; doppio click per rinominare; Alt+Click per eliminare";
            return "Marker Puntuale (Cue / Marker)\nTrascina per spostare il marker temporale; doppio click per rinominare; Alt+Click per eliminare";
        }
        return "Lane Keyframe\nDoppio click sul vuoto per aggiungere uno span o marker; trascina sul vuoto per selezione marquee";
    }

    void setModel (const Track& t, int laneHeight)
    {
        data = t;
        height = laneHeight;
        programAudible = audibility (host.store().state(), data).program;

        if (!laneComponent)
        {
            auto& provider = static_cast<clips::KeyframeLanePresentationProvider&> (*this);
            laneComponent = std::make_unique<KeyframeLaneComponent> (data.id, provider,
                                                                     laneType != KeyframeLaneType::points,
                                                                     laneType);
            addAndMakeVisible (*laneComponent);
        }
        laneComponent->setLaneType (laneType);

        if (events.empty())
            initDefaultEvents();

        rebuildVisualCache();
        layoutLaneComponent();
        repaint();
    }

    void resized() override
    {
        rebuildVisualCache();
        layoutLaneComponent();
    }

    void paint (juce::Graphics& g) override
    {
        const auto pps = getWidth() / 32.0;
        host.look().drawTrackGrid (g, getLocalBounds(), pps);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        marqueeSelector.drawMarquee (g);
    }

private:
    void initDefaultEvents()
    {
        events.clear();
        if (laneType == KeyframeLaneType::contiguousSpans)
        {
            events = {
                { 1, 0.0,  8.0,  0,  "Intro",   juce::Colour (0xff38bdf8), false },
                { 2, 8.0,  10.0, 4,  "Verse 1", juce::Colour (0xffa78bfa), false },
                { 3, 18.0, 14.0, 11, "Chorus",  juce::Colour (0xff34d399), false }
            };
            nextEventId = 4;
        }
        else if (laneType == KeyframeLaneType::disjointSpans)
        {
            events = {
                { 1, 2.0,  6.0, 0,  "Block A", juce::Colour (0xffec4899), false },
                { 2, 11.0, 6.0, 4,  "Block B", juce::Colour (0xff8b5cf6), false },
                { 3, 20.0, 8.0, 11, "Block C", juce::Colour (0xfff59e0b), false }
            };
            nextEventId = 4;
        }
        else
        {
            events = {
                { 1, 2.0,  0.0, 0,  "PC 1",  juce::Colour (0xff38bdf8), false },
                { 2, 10.0, 0.0, 4,  "PC 5",  juce::Colour (0xffa78bfa), false },
                { 3, 20.0, 0.0, 11, "PC 12", juce::Colour (0xff34d399), false }
            };
            nextEventId = 4;
        }
    }

    void rebuildVisualCache()
    {
        constexpr double totalTimelineSec = 32.0;

        std::sort (events.begin(), events.end(),
                   [] (const KeyframeSourceEvent& a, const KeyframeSourceEvent& b)
                   {
                       return a.startSec < b.startSec;
                   });

        // In contiguousSpans mode, invarianti fondamentali:
        // 1) Il primo keyframe parte SEMPRE da 0.0s e non può modificare il suo inizio.
        // 2) Gli span sono contigui senza buchi: event[i].lengthSec = event[i+1].startSec - event[i].startSec.
        // 3) L'ultimo procede fino a fine timeline (totalTimelineSec).
        if (laneType == KeyframeLaneType::contiguousSpans && !events.empty())
        {
            events[0].startSec = 0.0;
            for (size_t i = 0; i + 1 < events.size(); ++i)
            {
                if (events[i + 1].startSec < events[i].startSec + 0.5)
                    events[i + 1].startSec = events[i].startSec + 0.5;
                events[i].lengthSec = events[i + 1].startSec - events[i].startSec;
            }
            events.back().lengthSec = std::max (0.5, totalTimelineSec - events.back().startSec);
        }

        cachedVisualEvents.clear();
        cachedVisualEvents.reserve (events.size());

        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& ev = events[i];
            const float startNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> (ev.startSec / totalTimelineSec));

            float endNorm = startNorm;
            if (laneType == KeyframeLaneType::contiguousSpans)
            {
                if (i + 1 < events.size())
                    endNorm = juce::jlimit (startNorm, 1.0f, static_cast<float> (events[i + 1].startSec / totalTimelineSec));
                else
                    endNorm = 1.0f;
            }
            else if (laneType == KeyframeLaneType::disjointSpans)
            {
                const double endSec = std::min (totalTimelineSec, ev.startSec + ev.lengthSec);
                endNorm = juce::jlimit (startNorm, 1.0f, static_cast<float> (endSec / totalTimelineSec));
            }

            KeyframeEventVisual vev;
            vev.sourceIndex = static_cast<int> (i);
            vev.startNormX  = startNorm;
            vev.endNormX    = endNorm;
            vev.label       = ev.label;
            vev.colour      = ev.colour;
            vev.selected    = ev.selected;
            vev.showBottomRemovalRail = (laneType != KeyframeLaneType::points);
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (juce::Font (juce::FontOptions().withHeight (12.0f)),
                                  ev.label, 0.0f, 0.0f);
            vev.pillWidth = juce::jmax (22.0f,
                                        glyphs.getBoundingBox (0, -1, true).getWidth() + 14.0f);
            cachedVisualEvents.push_back (vev);
        }

        if (laneType == KeyframeLaneType::points)
        {
            float lastRight[clips::keyframe_lane_layout::maxRows];
            for (auto& right : lastRight) right = -1.0e9f;

            const auto laneBounds = keyframeLaneBoundsInCanvas();
            for (auto& event : cachedVisualEvents)
            {
                const float x = laneBounds.getX() + event.startNormX * laneBounds.getWidth();
                int row = clips::keyframe_lane_layout::maxRows - 1;
                for (int candidate = 0; candidate < clips::keyframe_lane_layout::maxRows; ++candidate)
                {
                    if (x >= lastRight[candidate] + 2.0f)
                    {
                        row = candidate;
                        break;
                    }
                }
                event.visualRow = row;
                lastRight[row] = juce::jmax (lastRight[row], x + event.pillWidth);
            }
        }
    }

    [[nodiscard]] KeyframeLaneVisual makeKeyframeLaneVisual (
        const std::string& /*laneId*/,
        const juce::Rectangle<float> localBodyBounds,
        const juce::Point<float> /*parentOrigin*/) override
    {
        KeyframeLaneVisual visual;
        visual.bounds               = localBodyBounds;
        visual.tint                 = juce::Colour (data.colour);
        visual.suspended            = !programAudible;
        visual.spanToNextEvent      = (laneType != KeyframeLaneType::points);
        visual.laneType             = laneType;
        visual.events               = cachedVisualEvents.data();
        visual.numEvents            = static_cast<int> (cachedVisualEvents.size());
        visual.hoveredEventIdx      = hoveredEventIdx;
        visual.hoveredBottomRailIdx = hoveredBottomRailIdx;
        visual.hoverCutX            = hoverCutX;
        return visual;
    }

    void layoutLaneComponent()
    {
        if (laneComponent && getWidth() > 0 && getHeight() > 0)
        {
            laneComponent->setBodyBoundsInParent (
                juce::Rectangle<float> (4.0f, 4.0f,
                                         static_cast<float> (getWidth()) - 8.0f,
                                         std::max (14.0f, static_cast<float> (height) - 8.0f)));
        }
    }

    [[nodiscard]] juce::Rectangle<float> keyframeLaneBoundsInCanvas() const noexcept
    {
        return { 4.0f, 4.0f,
                 juce::jmax (1.0f, static_cast<float> (getWidth()) - 8.0f),
                 juce::jmax (14.0f, static_cast<float> (height) - 8.0f) };
    }

    HitInfo hitTestEvent (juce::Point<float> p) const
    {
        if (getWidth() <= 0) return {};
        const auto laneBounds = keyframeLaneBoundsInCanvas();
        const bool isSpan = (laneType != KeyframeLaneType::points);
        const bool isDisjoint = (laneType == KeyframeLaneType::disjointSpans);

        for (int i = static_cast<int> (cachedVisualEvents.size()) - 1; i >= 0; --i)
        {
            const auto& ev = cachedVisualEvents[static_cast<size_t> (i)];
            const auto pill = clips::keyframe_lane_layout::eventBounds (laneBounds, ev, isSpan);

            // 1. Barra inferiore di taglio (Split tool per entrambe le tipologie di span)
            if (isSpan)
            {
                const auto bottomBar = juce::Rectangle<float> (pill.getX() - 1.0f, pill.getBottom() - 1.0f,
                                                               juce::jmax (2.0f, pill.getWidth() + 2.0f), 8.5f);
                if (bottomBar.contains (p))
                    return { HitZone::bottomRail, i };
            }

            if (!isSpan)
            {
                const auto hitBox = pill.expanded (4.0f, 2.0f);
                if (hitBox.contains (p))
                    return { HitZone::body, i };
                continue;
            }

            // Per gli span:
            // 2. Bordo sinistro (estremo sinistro)
            const bool canLeftEdgeMove = (isDisjoint || i > 0);
            if (canLeftEdgeMove && std::abs (p.x - pill.getX()) <= 6.0f
                && p.y >= pill.getY() - 2.0f && p.y <= pill.getBottom() - 1.0f)
            {
                return { HitZone::leftEdge, i };
            }

            // 3. Bordo destro (estremo destro)
            const bool canRightEdgeMove = (isDisjoint || i < static_cast<int> (cachedVisualEvents.size()) - 1);
            if (canRightEdgeMove && std::abs (p.x - pill.getRight()) <= 6.0f
                && p.y >= pill.getY() - 2.0f && p.y <= pill.getBottom() - 1.0f)
            {
                return { HitZone::rightEdge, i };
            }

            // 4. Corpo dello span
            if (pill.contains (p))
            {
                return { HitZone::body, i };
            }
        }
        return {};
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto hit = hitTestEvent (e.position);
        hoveredHitZone = hit.zone;
        hoveredEventIdx = hit.index;
        hoveredBottomRailIdx = (hit.zone == HitZone::bottomRail ? hit.index : -1);
        hoverCutX = e.position.x;

        const auto zone = timelineZoneForHit (hit.zone);
        const bool canErase = hit.index >= 0
            && (laneType != KeyframeLaneType::contiguousSpans || events.size() > 1);
        const auto role = hit.zone == HitZone::none
            && e.position.y >= getHeight() - 6.0f
            ? clips::TimelineCursorRole::verticalResize
            : clips::resolveTimelineCursorRole ({ zone, e.mods.isAltDown(),
                e.mods.isShiftDown(), true, canErase });
        setMouseCursor (clips::timelineMouseCursor (role));

        repaint();
    }

    void modifierKeysChanged (const juce::ModifierKeys& modifiers) override
    {
        const bool canErase = hoveredEventIdx >= 0
            && (laneType != KeyframeLaneType::contiguousSpans || events.size() > 1);
        setMouseCursor (clips::timelineMouseCursor (
            clips::resolveTimelineCursorRole ({ timelineZoneForHit (hoveredHitZone),
                modifiers.isAltDown(), modifiers.isShiftDown(), true, canErase })));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredEventIdx = -1;
        hoveredBottomRailIdx = -1;
        hoveredHitZone = HitZone::none;
        hoverCutX = -1.0f;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();

        if (e.mods.isLeftButtonDown()
            && e.position.y >= getHeight() - 6.0f)
        {
            isResizingLaneHeight = true;
            resizeLaneStartY = e.getScreenY();
            resizeStartLaneHeight = height;
            return;
        }

        const auto hit = hitTestEvent (e.position);
        if (e.mods.isPopupMenu())
        {
            TimelineClipZone zone = TimelineClipZone::laneBackground;
            switch (hit.zone)
            {
                case HitZone::leftEdge:  zone = TimelineClipZone::keyframeLeftEdge; break;
                case HitZone::rightEdge: zone = TimelineClipZone::keyframeRightEdge; break;
                case HitZone::bottomRail: zone = TimelineClipZone::keyframeCutRail; break;
                case HitZone::body: zone = TimelineClipZone::keyframeBody; break;
                case HitZone::none: break;
            }

            const bool hasEvent = hit.index >= 0
                && hit.index < static_cast<int> (events.size());
            const auto eventId = hasEvent
                ? events[static_cast<size_t> (hit.index)].id : -1;
            auto context = clips::makeTimelinePointerContext (
                data.id, hasEvent ? std::to_string (eventId) : std::string {},
                hasEvent ? clips::TimelinePointerTarget::keyframeEvent
                         : clips::TimelinePointerTarget::keyframeLane,
                laneType == KeyframeLaneType::points
                    ? clips::ClipKind::pointKeyframe
                    : clips::ClipKind::spanKeyframe,
                zone, e.position, clips::timelineSecondsFromCanvasX (
                    e.position.x, getWidth()), e.mods);
            context.eventIndex = hit.index;
            context.eventId = eventId;
            context.keyframeLaneType = laneType;
            host.requestTimelineContextMenu(context);
            return;
        }

        if (hit.zone == HitZone::none)
        {
            isMarqueeActive = marqueeSelector.handleMouseDown (e);
            return;
        }

        // Alt + Click su qualunque elemento: elimina l'evento (come specificato dall'utente)
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && hit.index >= 0 && hit.index < static_cast<int> (events.size()))
        {
            // Per contiguousSpans, non rimuovere lo span se ne e' rimasto solo uno
            if (laneType != KeyframeLaneType::contiguousSpans || events.size() > 1)
            {
                events.erase (events.begin() + hit.index);
                hoveredEventIdx = -1;
                hoveredBottomRailIdx = -1;
                rebuildVisualCache();
                repaint();
            }
            return;
        }

        // Clic sulla barra inferiore: Split / Taglio dello span in 2 blocchi alla posizione del cursore
        if (hit.zone == HitZone::bottomRail && hit.index >= 0 && hit.index < static_cast<int> (events.size()))
        {
            constexpr double totalTimelineSec = 32.0;
            const double usableW = std::max (1.0, static_cast<double> (getWidth()) - 8.0);
            const double splitSec = std::clamp (((e.position.x - 4.0) / usableW) * totalTimelineSec, 0.0, totalTimelineSec);
            const size_t i = static_cast<size_t> (hit.index);

            if (laneType == KeyframeLaneType::disjointSpans)
            {
                const double start = events[i].startSec;
                const double end   = start + events[i].lengthSec;
                if (end - start >= 0.4 && splitSec >= start + 0.15 && splitSec <= end - 0.15)
                {
                    events[i].lengthSec = splitSec - start;
                    KeyframeSourceEvent newEv;
                    newEv.id = nextEventId++;
                    newEv.startSec = splitSec;
                    newEv.lengthSec = end - splitSec;
                    newEv.programNumber = static_cast<std::uint8_t> ((nextEventId * 7) % 128);
                    newEv.label = events[i].label + " (b)";
                    newEv.colour = events[i].colour;
                    newEv.selected = true;
                    events[i].selected = false;
                    events.insert (events.begin() + i + 1, newEv);
                    hoveredEventIdx = -1;
                    hoveredBottomRailIdx = -1;
                    rebuildVisualCache();
                    repaint();
                    return;
                }
            }
            else if (laneType == KeyframeLaneType::contiguousSpans)
            {
                const double start = events[i].startSec;
                const double nextStart = (i + 1 < events.size() ? events[i + 1].startSec : totalTimelineSec);
                if (nextStart - start >= 0.4 && splitSec >= start + 0.15 && splitSec <= nextStart - 0.15)
                {
                    KeyframeSourceEvent newEv;
                    newEv.id = nextEventId++;
                    newEv.startSec = splitSec;
                    newEv.lengthSec = nextStart - splitSec;
                    newEv.programNumber = static_cast<std::uint8_t> ((nextEventId * 7) % 128);
                    newEv.label = events[i].label + " (b)";
                    newEv.colour = events[i].colour;
                    newEv.selected = true;
                    events[i].selected = false;
                    events.insert (events.begin() + i + 1, newEv);
                    hoveredEventIdx = -1;
                    hoveredBottomRailIdx = -1;
                    rebuildVisualCache();
                    repaint();
                    return;
                }
            }
            return;
        }

        if (!e.mods.isLeftButtonDown())
            return;

        const size_t idx = static_cast<size_t> (hit.index);
        if (e.mods.isShiftDown())
            events[idx].selected = true;
        else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            events[idx].selected = !events[idx].selected;
        else if (!events[idx].selected)
        {
            for (auto& ev : events) ev.selected = false;
            events[idx].selected = true;
        }

        activeDragZone   = hit.zone;
        draggedEventIdx  = hit.index;
        draggedEventId   = events[idx].id;
        dragStartPos     = e.position;
        snapshotEvents   = events;
        hasDragSnapshot  = true;

        rebuildVisualCache();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isResizingLaneHeight)
        {
            const int deltaY = e.getScreenY() - resizeLaneStartY;
            const int newH = std::clamp (resizeStartLaneHeight + deltaY, minimumHeight (data), 1200);
            if (e.mods.isShiftDown() || e.mods.isAltDown())
            {
                const float factor = static_cast<float> (newH) / static_cast<float> (juce::jmax (1, resizeStartLaneHeight));
                host.adjustVerticalZoom (factor - 1.0f);
            }
            else
            {
                host.store().resize (data.id, newH);
            }
            return;
        }

        if (isMarqueeActive)
        {
            marqueeSelector.handleMouseDrag (e);
            return;
        }

        if (draggedEventId == 0 || !hasDragSnapshot || getWidth() <= 0)
            return;

        constexpr double totalTimelineSec = 32.0;
        const double usableW = std::max (1.0, static_cast<double> (getWidth()) - 8.0);
        const double deltaSec = ((e.position.x - dragStartPos.x) / usableW) * totalTimelineSec;

        // Trova l'evento trascinato nello snapshot in base all'ID stabile
        int dragSnapIdx = -1;
        for (int k = 0; k < static_cast<int> (snapshotEvents.size()); ++k)
        {
            if (snapshotEvents[static_cast<size_t> (k)].id == draggedEventId)
            {
                dragSnapIdx = k;
                break;
            }
        }
        if (dragSnapIdx < 0) return;
        const size_t i = static_cast<size_t> (dragSnapIdx);

        // Ripristina la base dallo snapshot per calcoli deterministici continui
        events = snapshotEvents;

        // 1. Point Keyframes
        if (laneType == KeyframeLaneType::points)
        {
            for (size_t k = 0; k < events.size(); ++k)
            {
                if (snapshotEvents[k].selected)
                    events[k].startSec = std::clamp (snapshotEvents[k].startSec + deltaSec, 0.0, totalTimelineSec - 0.1);
            }
            rebuildVisualCache();
            repaint();
            return;
        }

        // 2. contiguousSpans (Program / Sections)
        if (laneType == KeyframeLaneType::contiguousSpans)
        {
            if (activeDragZone == HitZone::leftEdge && i > 0)
            {
                // Muove il confine tra event i-1 ed event i
                const double orig = snapshotEvents[i].startSec;
                const double minT = snapshotEvents[i - 1].startSec + 0.5;
                const double maxT = (i + 1 < snapshotEvents.size() ? snapshotEvents[i + 1].startSec - 0.5 : totalTimelineSec - 0.5);
                events[i].startSec = std::clamp (orig + deltaSec, minT, maxT);
            }
            else if (activeDragZone == HitZone::rightEdge && i + 1 < events.size())
            {
                // Muove il confine tra event i ed event i+1
                const size_t nextI = i + 1;
                const double orig = snapshotEvents[nextI].startSec;
                const double minT = snapshotEvents[i].startSec + 0.5;
                const double maxT = (nextI + 1 < snapshotEvents.size() ? snapshotEvents[nextI + 1].startSec - 0.5 : totalTimelineSec - 0.5);
                events[nextI].startSec = std::clamp (orig + deltaSec, minT, maxT);
            }
            else if (activeDragZone == HitZone::body)
            {
                // Multi-selezione vs singolo drag
                std::vector<size_t> selIndices;
                for (size_t k = 0; k < snapshotEvents.size(); ++k)
                    if (snapshotEvents[k].selected) selIndices.push_back (k);

                if (selIndices.size() > 1 && std::find (selIndices.begin(), selIndices.end(), i) != selIndices.end())
                {
                    // Draggare in blocco tutti i selezionati
                    const size_t firstSel = selIndices.front();
                    const size_t lastSel  = selIndices.back();

                    const double minStart = (firstSel > 0 ? snapshotEvents[firstSel - 1].startSec + 0.5 : 0.0);
                    const double maxEnd   = (lastSel + 1 < snapshotEvents.size() ? snapshotEvents[lastSel + 1].startSec - 0.5 : totalTimelineSec);
                    const double minDelta = minStart - snapshotEvents[firstSel].startSec;
                    const double maxDelta = maxEnd - (lastSel + 1 < snapshotEvents.size() ? snapshotEvents[lastSel + 1].startSec : totalTimelineSec);
                    const double clampedDelta = std::clamp (deltaSec, minDelta, maxDelta);

                    for (size_t idx : selIndices)
                    {
                        if (idx > 0)
                            events[idx].startSec = snapshotEvents[idx].startSec + clampedDelta;
                    }
                }
                else
                {
                    // Drag singolo blocco ("draggare in blocco"): sposta il range preservando la durata
                    if (i > 0 && i + 1 < snapshotEvents.size())
                    {
                        const double origStart = snapshotEvents[i].startSec;
                        const double origEnd   = snapshotEvents[i + 1].startSec;
                        const double minStart  = snapshotEvents[i - 1].startSec + 0.5;
                        const double maxEnd    = (i + 2 < snapshotEvents.size() ? snapshotEvents[i + 2].startSec - 0.5 : totalTimelineSec - 0.5);
                        const double clampedDelta = std::clamp (deltaSec, minStart - origStart, maxEnd - origEnd);
                        events[i].startSec     = origStart + clampedDelta;
                        events[i + 1].startSec = origEnd + clampedDelta;
                    }
                    else if (i == 0 && snapshotEvents.size() > 1)
                    {
                        // Span iniziale ancorato a 0: il drag sul corpo sposta il limite destro
                        const double maxEnd = (snapshotEvents.size() > 2 ? snapshotEvents[2].startSec - 0.5 : totalTimelineSec - 0.5);
                        events[1].startSec = std::clamp (snapshotEvents[1].startSec + deltaSec, 0.5, maxEnd);
                    }
                    else if (i == snapshotEvents.size() - 1 && i > 0)
                    {
                        // Span finale fino a fine timeline: il drag sul corpo sposta il limite sinistro
                        events[i].startSec = std::clamp (snapshotEvents[i].startSec + deltaSec,
                                                         snapshotEvents[i - 1].startSec + 0.5, totalTimelineSec - 0.5);
                    }
                }
            }
        }
        // 3. disjointSpans (Arranger / Blocks)
        else if (laneType == KeyframeLaneType::disjointSpans)
        {
            if (activeDragZone == HitZone::leftEdge)
            {
                // Spostando l'inizio indietro contro la fine del blocco precedente,
                // il blocco precedente SI RIDUCE (push) permettendo a questo blocco di estendersi indietro dove necessario!
                const double origStart = snapshotEvents[i].startSec;
                const double fixedEnd  = origStart + snapshotEvents[i].lengthSec;
                const double maxStart  = fixedEnd - 0.2; // durata minima 0.2s per blocco i
                const double rawStart  = std::clamp (origStart + deltaSec, 0.0, maxStart);

                events[i].startSec  = rawStart;
                events[i].lengthSec = fixedEnd - rawStart;

                // Push a cascata indietro sui blocchi precedenti se rawStart si estende dentro di essi
                double barrier = rawStart;
                for (int k = static_cast<int> (i) - 1; k >= 0; --k)
                {
                    const double prevOrigStart = snapshotEvents[static_cast<size_t> (k)].startSec;
                    const double prevOrigEnd   = prevOrigStart + snapshotEvents[static_cast<size_t> (k)].lengthSec;
                    if (barrier < prevOrigEnd)
                    {
                        const double newPrevEnd = barrier;
                        if (newPrevEnd >= prevOrigStart + 0.2)
                        {
                            events[static_cast<size_t> (k)].startSec  = prevOrigStart;
                            events[static_cast<size_t> (k)].lengthSec = newPrevEnd - prevOrigStart;
                            break;
                        }
                        else
                        {
                            const double newPrevStart = std::max (0.0, newPrevEnd - 0.2);
                            events[static_cast<size_t> (k)].startSec  = newPrevStart;
                            events[static_cast<size_t> (k)].lengthSec = newPrevEnd - newPrevStart;
                            barrier = newPrevStart;
                        }
                    }
                    else
                    {
                        events[static_cast<size_t> (k)] = snapshotEvents[static_cast<size_t> (k)];
                    }
                }
            }
            else if (activeDragZone == HitZone::rightEdge)
            {
                // Spostando la fine avanti contro l'inizio del blocco successivo,
                // il blocco successivo si riduce (push)
                const double fixedStart = snapshotEvents[i].startSec;
                const double origEnd    = fixedStart + snapshotEvents[i].lengthSec;
                const double minEnd     = fixedStart + 0.2;
                const double rawEnd     = std::clamp (origEnd + deltaSec, minEnd, totalTimelineSec);

                events[i].startSec  = fixedStart;
                events[i].lengthSec = rawEnd - fixedStart;

                // Push a cascata in avanti sui blocchi successivi se rawEnd si estende dentro di essi
                double barrier = rawEnd;
                for (size_t k = i + 1; k < events.size(); ++k)
                {
                    const double nextOrigStart = snapshotEvents[k].startSec;
                    const double nextOrigEnd   = nextOrigStart + snapshotEvents[k].lengthSec;
                    if (barrier > nextOrigStart)
                    {
                        const double newNextStart = barrier;
                        if (newNextStart <= nextOrigEnd - 0.2)
                        {
                            events[k].startSec  = newNextStart;
                            events[k].lengthSec = nextOrigEnd - newNextStart;
                            break;
                        }
                        else
                        {
                            const double newNextEnd = std::min (totalTimelineSec, newNextStart + 0.2);
                            events[k].startSec  = newNextStart;
                            events[k].lengthSec = newNextEnd - newNextStart;
                            barrier = newNextEnd;
                        }
                    }
                    else
                    {
                        events[k] = snapshotEvents[k];
                    }
                }
            }
            else if (activeDragZone == HitZone::body)
            {
                // Multi-selezione vs singolo drag
                std::vector<size_t> selIndices;
                for (size_t k = 0; k < snapshotEvents.size(); ++k)
                    if (snapshotEvents[k].selected) selIndices.push_back (k);

                if (std::find (selIndices.begin(), selIndices.end(), i) == selIndices.end())
                {
                    selIndices.clear();
                    selIndices.push_back (i);
                }

                // Calcolo confini del gruppo selezionato
                double groupMinStart = totalTimelineSec;
                double groupMaxEnd   = 0.0;
                for (size_t idx : selIndices)
                {
                    groupMinStart = std::min (groupMinStart, snapshotEvents[idx].startSec);
                    groupMaxEnd   = std::max (groupMaxEnd,   snapshotEvents[idx].startSec + snapshotEvents[idx].lengthSec);
                }

                const double clampedDelta = std::clamp (deltaSec, -groupMinStart, totalTimelineSec - groupMaxEnd);

                // Sposta tutti i blocchi selezionati preservando le durate
                for (size_t idx : selIndices)
                {
                    events[idx].startSec  = snapshotEvents[idx].startSec + clampedDelta;
                    events[idx].lengthSec = snapshotEvents[idx].lengthSec;
                }

                // Se trascinato indietro, riduce/spinge i blocchi non selezionati precedenti
                const size_t firstSel = selIndices.front();
                double leftBarrier = events[firstSel].startSec;
                for (int k = static_cast<int> (firstSel) - 1; k >= 0; --k)
                {
                    const size_t uk = static_cast<size_t> (k);
                    if (snapshotEvents[uk].selected)
                    {
                        leftBarrier = events[uk].startSec;
                        continue;
                    }
                    const double origEnd = snapshotEvents[uk].startSec + snapshotEvents[uk].lengthSec;
                    if (leftBarrier < origEnd)
                    {
                        const double newEnd = leftBarrier;
                        if (newEnd >= snapshotEvents[uk].startSec + 0.2)
                        {
                            events[uk].startSec  = snapshotEvents[uk].startSec;
                            events[uk].lengthSec = newEnd - snapshotEvents[uk].startSec;
                            break;
                        }
                        else
                        {
                            const double newStart = std::max (0.0, newEnd - 0.2);
                            events[uk].startSec  = newStart;
                            events[uk].lengthSec = newEnd - newStart;
                            leftBarrier = newStart;
                        }
                    }
                }

                // Se trascinato in avanti, riduce/spinge i blocchi non selezionati successivi
                const size_t lastSel = selIndices.back();
                double rightBarrier = events[lastSel].startSec + events[lastSel].lengthSec;
                for (size_t k = lastSel + 1; k < events.size(); ++k)
                {
                    if (snapshotEvents[k].selected)
                    {
                        rightBarrier = events[k].startSec + events[k].lengthSec;
                        continue;
                    }
                    const double origStart = snapshotEvents[k].startSec;
                    const double origEnd   = origStart + snapshotEvents[k].lengthSec;
                    if (rightBarrier > origStart)
                    {
                        const double newStart = rightBarrier;
                        if (newStart <= origEnd - 0.2)
                        {
                            events[k].startSec  = newStart;
                            events[k].lengthSec = origEnd - newStart;
                            break;
                        }
                        else
                        {
                            const double newEnd = std::min (totalTimelineSec, newStart + 0.2);
                            events[k].startSec  = newStart;
                            events[k].lengthSec = newEnd - newStart;
                            rightBarrier = newEnd;
                        }
                    }
                }
            }
        }

        rebuildVisualCache();
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isResizingLaneHeight)
        {
            isResizingLaneHeight = false;
            return;
        }

        if (isMarqueeActive)
        {
            marqueeSelector.handleMouseUp (e);
            isMarqueeActive = false;
        }
        activeDragZone  = HitZone::none;
        draggedEventIdx = -1;
        draggedEventId  = 0;
        hasDragSnapshot = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;

        const auto hit = hitTestEvent (e.position);
        // Doppio click sul corpo dello span: editing inline del testo ("poterli selezionare ed editare")
        if (hit.zone == HitZone::body && hit.index >= 0 && hit.index < static_cast<int> (events.size()))
        {
            startEditingEvent (static_cast<size_t> (hit.index));
            return;
        }

        if (hit.zone != HitZone::none)
            return; // click su estremi o barra di taglio

        // Se doppio click sul bordo/divider: cicla altezze canoniche
        if (e.position.y >= getHeight() - 6.0f)
        {
            const int modelHeight = clip_height_law::nextModelHeight (static_cast<float> (height), host.getVerticalZoom());
            host.store().resize (data.id, modelHeight);
            return;
        }

        // Doppio click sul vuoto della lane: inserisce un nuovo evento
        constexpr double totalTimelineSec = 32.0;
        const double usableW = std::max (1.0, static_cast<double> (getWidth()) - 8.0);
        const double clickedSec = std::clamp (((e.position.x - 4.0) / usableW) * totalTimelineSec,
                                              0.0, totalTimelineSec - 0.5);

        for (auto& ev : events) ev.selected = false;

        if (laneType == KeyframeLaneType::points)
        {
            const auto prog = static_cast<std::uint8_t> ((nextEventId * 3) % 128);
            events.push_back ({
                nextEventId++,
                clickedSec,
                0.0,
                prog,
                "M " + juce::String (nextEventId - 1),
                juce::Colour (0xff38bdf8),
                true
            });
        }
        else if (laneType == KeyframeLaneType::contiguousSpans)
        {
            const auto prog = static_cast<std::uint8_t> ((nextEventId * 3) % 128);
            events.push_back ({
                nextEventId++,
                clickedSec,
                4.0,
                prog,
                clip_midi_law::programChangeLabel (prog),
                juce::Colour (0xff38bdf8),
                true
            });
        }
        else if (laneType == KeyframeLaneType::disjointSpans)
        {
            // In disjoint mode: durata fino a 5.0s, garantendo no overlap con lo span successivo
            double maxDur = 5.0;
            for (const auto& ev : events)
            {
                if (ev.startSec > clickedSec)
                    maxDur = std::min (maxDur, std::max (0.5, ev.startSec - clickedSec - 0.05));
            }

            const auto prog = static_cast<std::uint8_t> ((nextEventId * 3) % 128);
            events.push_back ({
                nextEventId++,
                clickedSec,
                maxDur,
                prog,
                "Block " + juce::String (nextEventId - 1),
                juce::Colour (0xffec4899),
                true
            });
        }

        rebuildVisualCache();
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isAltDown() || e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown())
        {
            const float delta = (std::abs (wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX);
            if (std::abs (delta) > 0.001f)
            {
                host.adjustVerticalZoom (delta > 0.0f ? 0.08f : -0.08f);
                return;
            }
        }
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            vp->mouseWheelMove (e, wheel);
        else
            Component::mouseWheelMove (e, wheel);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.isKeyCode (juce::KeyPress::escapeKey) && hasDragSnapshot)
        {
            events = snapshotEvents;
            activeDragZone = HitZone::none;
            draggedEventIdx = -1;
            hasDragSnapshot = false;
            rebuildVisualCache();
            repaint();
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::backspaceKey || key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            bool removed = false;
            for (auto it = events.begin(); it != events.end();)
            {
                if (it->selected) { it = events.erase (it); removed = true; }
                else ++it;
            }
            if (removed)
            {
                hoveredEventIdx = -1;
                hoveredBottomRailIdx = -1;
                rebuildVisualCache();
                repaint();
                return true;
            }
        }
        return false;
    }

    void startEditingEvent (size_t idx)
    {
        if (idx >= events.size() || idx >= cachedVisualEvents.size()) return;
        editingEventIdx = static_cast<int> (idx);

        inlineEditor = std::make_unique<juce::TextEditor>();
        inlineEditor->setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
        inlineEditor->setText (events[idx].label, false);
        inlineEditor->selectAll();
        inlineEditor->setEscapeAndReturnKeysConsumed (true);
        inlineEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f172a));
        inlineEditor->setColour (juce::TextEditor::textColourId, juce::Colours::white);
        inlineEditor->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff38bdf8));
        inlineEditor->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff38bdf8));
        inlineEditor->setJustification (juce::Justification::centred);
        inlineEditor->onReturnKey = [this] { commitInlineEdit(); };
        inlineEditor->onEscapeKey = [this] { cancelInlineEdit(); };
        inlineEditor->onFocusLost = [this] { commitInlineEdit(); };

        const auto laneBounds = keyframeLaneBoundsInCanvas();
        const auto pill = clips::keyframe_lane_layout::eventBounds (laneBounds, cachedVisualEvents[idx], laneType != KeyframeLaneType::points);
        const int edW = juce::jmax (72, juce::roundToInt (pill.getWidth()));
        const int edX = juce::jlimit (4, juce::jmax (4, getWidth() - edW - 4), juce::roundToInt (pill.getX()));
        inlineEditor->setBounds (edX, juce::roundToInt (pill.getY()), edW, juce::roundToInt (pill.getHeight()));
        addAndMakeVisible (*inlineEditor);
        inlineEditor->grabKeyboardFocus();
    }

    void commitInlineEdit()
    {
        if (inlineEditor && editingEventIdx >= 0 && static_cast<size_t> (editingEventIdx) < events.size())
        {
            const auto text = inlineEditor->getText().trim();
            if (text.isNotEmpty())
                events[static_cast<size_t> (editingEventIdx)].label = text;
        }
        cancelInlineEdit();
    }

    void cancelInlineEdit()
    {
        if (inlineEditor)
        {
            inlineEditor->setVisible (false);
            removeChildComponent (inlineEditor.get());
            inlineEditor.reset();
        }
        editingEventIdx = -1;
        rebuildVisualCache();
        repaint();
    }

    LabComponent& host;
    Track data;
    int height = 44;
    bool programAudible = true;
    KeyframeLaneType laneType = KeyframeLaneType::contiguousSpans;

    int nextEventId = 1;
    std::vector<KeyframeSourceEvent> events;
    std::vector<KeyframeSourceEvent> snapshotEvents;
    std::vector<KeyframeEventVisual> cachedVisualEvents;

    std::unique_ptr<KeyframeLaneComponent> laneComponent;
    std::unique_ptr<juce::TextEditor> inlineEditor;
    int editingEventIdx = -1;
    ayra::MarqueeSelector marqueeSelector { *this };
    bool isMarqueeActive = false;

    HitZone activeDragZone = HitZone::none;
    int hoveredEventIdx = -1;
    int hoveredBottomRailIdx = -1;
    HitZone hoveredHitZone = HitZone::none;
    float hoverCutX = -1.0f;
    int draggedEventIdx = -1;
    int draggedEventId = 0;
    bool hasDragSnapshot = false;
    juce::Point<float> dragStartPos;

    bool isResizingLaneHeight = false;
    int resizeLaneStartY = 0;
    int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
