/** @file ayra_ContinuousLineCanvas.cpp
    Adapter del Lab per una lane a linea continua (`ContinuousLineComponent`),
    come la traccia TEMPO o un'automazione globale su tutta la timeline. */

namespace ayra::lab
{
namespace
{

using clips::ContinuousLineComponent;
using clips::ContinuousLineVisual;
using clips::MidiEnvelopePoint;
using clips::TimelineClipZone;
namespace clip_height_law = clips::clip_height_law;

class ContinuousLineCanvas final : public juce::Component,
                                   public juce::TooltipClient,
                                   public clips::ContinuousLinePresentationProvider
{
public:
    explicit ContinuousLineCanvas (
        LabComponent& h,
        const timeline_visual_style::CurveLineColourVariant colourVariant
            = timeline_visual_style::CurveLineColourVariant::light)
        : host (h),
          curveColour (timeline_visual_style::curveLineColour (colourVariant))
    {
        setWantsKeyboardFocus (true);
        marqueeSelector.setFillColour (juce::Colour (0x3000d084));
        marqueeSelector.setOutlineColour (juce::Colour (0xe000d084));

        // Punti iniziali in stile traccia TEMPO dello screenshot (120 BPM -> 67.4 BPM)
        points = {
            { 0.0f,  valueToNorm (120.0f), 0.5f, false },
            { 0.45f, valueToNorm (67.4f),  0.5f, true  },
            { 1.0f,  valueToNorm (67.4f),  0.5f, false }
        };

        marqueeSelector.onPerformMarqueeSelection = [this] (const juce::Rectangle<int>& area,
                                                            ayra::MarqueeSelector::ModifierMode mode)
        {
            const auto b = clips::computeContinuousLineLayout (
                getLocalBounds().toFloat(), true).graphBounds;
            bool anyChanged = false;

            for (int pointIndex = 0;
                 pointIndex < static_cast<int> (points.size());
                 ++pointIndex)
            {
                auto& pt = points[static_cast<size_t> (pointIndex)];
                const float px = clips::automationPointRenderX (
                    b, pointIndex, static_cast<int> (points.size()), pt.normX);
                const float py = b.getBottom() - juce::jlimit (0.0f, 1.0f, pt.normValue) * b.getHeight();
                const bool inside = area.contains (juce::roundToInt (px), juce::roundToInt (py));

                if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
                    pt.selected = inside;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Add && inside)
                    pt.selected = true;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Subtract && inside)
                    pt.selected = false;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Toggle && inside)
                    pt.selected = !pt.selected;

                anyChanged = true;
            }

            if (anyChanged)
                repaint();
        };

        marqueeSelector.onPerformClickSelection = [this] (juce::Point<int>,
                                                           ayra::MarqueeSelector::ModifierMode mode)
        {
            if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
            {
                for (auto& pt : points)
                    pt.selected = false;
                repaint();
            }
        };
    }

    juce::String getTooltip() override
    {
        switch (activeZone)
        {
            case TimelineClipZone::gainEnvelopePoint:
                return "Nodo Linea Continua (es. Tempo BPM)\nIl primo resta a inizio timeline e non si elimina; Alt+Click elimina solo i nodi interni";
            case TimelineClipZone::gainSegmentCurveHandle:
                return "Curvatura Segmento\nTrascina in orizzontale e verticale per modellare la rampa (doppio click per lineare)";
            case TimelineClipZone::gainEnvelopeLine:
                return "Linea Continua\nDoppio click per aggiungere un nuovo punto; trascina verticalmente per alzare/abbassare il segmento";
            default:
                return "Traccia Linea Continua\nDoppio click sulla linea per creare un punto; trascina per selezione marquee";
        }
    }

    void setRange (float minVal, float maxVal, float defaultVal, int decimals = 1) noexcept
    {
        minDisplayValue = minVal;
        maxDisplayValue = std::max (minVal + 0.001f, maxVal);
        defaultDisplayValue = std::clamp (defaultVal, minDisplayValue, maxDisplayValue);
        valueDecimals = decimals;
        repaint();
    }

    [[nodiscard]] float valueToNorm (float val) const noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (val - minDisplayValue) / (maxDisplayValue - minDisplayValue));
    }

    [[nodiscard]] float normToValue (float norm) const noexcept
    {
        return minDisplayValue + juce::jlimit (0.0f, 1.0f, norm) * (maxDisplayValue - minDisplayValue);
    }

    void setModel (const Track& t, int laneHeight)
    {
        data = t;
        height = laneHeight;
        programAudible = audibility (host.store().state(), data).program;

        if (!lineComponent)
        {
            auto& provider = static_cast<clips::ContinuousLinePresentationProvider&> (*this);
            lineComponent = std::make_unique<ContinuousLineComponent> (data.id, provider);
            addAndMakeVisible (*lineComponent);
        }

        layoutLineComponent();
        repaint();
    }

    void resized() override
    {
        layoutLineComponent();
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
    [[nodiscard]] ContinuousLineVisual makeContinuousLineVisual (
        const std::string& /*laneId*/,
        const juce::Rectangle<float> localBodyBounds,
        const juce::Point<float> /*parentOrigin*/) override
    {
        ContinuousLineVisual visual;
        visual.bounds              = localBodyBounds;
        visual.tint                = juce::Colour (data.colour != 0 ? data.colour : 0xff2dd4bf);
        visual.curveColour         = curveColour;
        visual.activeNodeColour    = juce::Colour (0xfff97316); // Arancio attivo stile screenshot TEMPO
        visual.suspended           = !programAudible;
        visual.minDisplayValue     = minDisplayValue;
        visual.maxDisplayValue     = maxDisplayValue;
        visual.defaultDisplayValue = defaultDisplayValue;
        visual.valueDecimals       = valueDecimals;
        visual.showLeftScale       = true;
        visual.points              = points.data();
        visual.numPoints           = static_cast<int> (points.size());
        visual.hoveredZone         = (dragZone != TimelineClipZone::none) ? dragZone : activeZone;
        visual.hoveredPointIdx     = (dragZone != TimelineClipZone::none) ? draggedPointIdx : hoveredPointIdx;
        visual.hoveredSegmentIdx   = (dragZone != TimelineClipZone::none) ? draggedSegmentIdx : hoveredSegmentIdx;
        return visual;
    }

    void layoutLineComponent()
    {
        if (lineComponent && getWidth() > 0 && getHeight() > 0)
        {
            lineComponent->setBodyBoundsInParent (
                juce::Rectangle<float> (0.0f, 0.0f,
                                        static_cast<float> (getWidth()),
                                        static_cast<float> (height)));
        }
    }

    void hitTestLine (juce::Point<float> p,
                      TimelineClipZone& outZone,
                      int& outPointIdx,
                      int& outSegIdx) const
    {
        outZone = TimelineClipZone::none;
        outPointIdx = -1;
        outSegIdx = -1;

        const auto b = clips::computeContinuousLineLayout (
            juce::Rectangle<float> (0.0f, 0.0f,
                                    static_cast<float> (getWidth()),
                                    static_cast<float> (height)),
            true).graphBounds;
        if (b.getWidth() <= 4.0f || b.getHeight() <= 8.0f)
            return;

        auto normToY = [&] (float normVal) -> float
        {
            return b.getBottom() - juce::jlimit (0.0f, 1.0f, normVal) * b.getHeight();
        };

        // 1. Nodi
        for (int i = 0; i < static_cast<int> (points.size()); ++i)
        {
            const auto& pt = points[static_cast<size_t> (i)];
            const auto point = clips::automationPointPixelPosition (
                b, pt, i, static_cast<int> (points.size()));
            if (point.getDistanceFrom (p) <= 9.0f)
            {
                outZone = TimelineClipZone::gainEnvelopePoint;
                outPointIdx = i;
                return;
            }
        }

        // 1b. Linee tratteggiate soltanto per i punti mobili in X.
        for (int ep = 0; ep < static_cast<int> (points.size()); ++ep)
        {
            if (! clips::canAutomationPointMoveHorizontally (
                    clips::AutomationBoundaryPolicy::pinnedEndpoints,
                    ep, static_cast<int> (points.size())))
                continue;

            const auto& pt = points[static_cast<size_t> (ep)];
            const float px = clips::automationPointRenderX (
                b, ep, static_cast<int> (points.size()), pt.normX);
            const float py = normToY (pt.normValue);
            if (p.y >= py + 6.0f && p.y <= b.getBottom() - 2.0f && std::abs (p.x - px) <= 7.0f)
            {
                outZone = TimelineClipZone::gainPointDashedLine;
                outPointIdx = ep;
                return;
            }
        }

        // 2. Handle di curvatura e segmenti
        if (points.size() >= 2)
        {
            for (size_t s = 0; s + 1 < points.size(); ++s)
            {
                const auto& n0 = points[s];
                const auto& n1 = points[s + 1];
                const float x0 = clips::automationPointRenderX (
                    b, static_cast<int> (s), static_cast<int> (points.size()), n0.normX);
                const float x1 = clips::automationPointRenderX (
                    b, static_cast<int> (s + 1), static_cast<int> (points.size()), n1.normX);
                const auto handle = clips::automationControlPixelPosition (
                    b, n0, n1, static_cast<int> (s),
                    static_cast<int> (points.size()));

                if (handle.getDistanceFrom (p) <= 8.0f)
                {
                    outZone = TimelineClipZone::gainSegmentCurveHandle;
                    outSegIdx = static_cast<int> (s);
                    return;
                }

                if (p.x >= x0 && p.x <= x1)
                {
                    const float span = std::max (0.001f, x1 - x0);
                    const float u = juce::jlimit (0.0f, 1.0f, (p.x - x0) / span);
                    const float expectedY = normToY (
                        clips::evaluateAutomationSegment (n0, n1, n0.normX + u * (n1.normX - n0.normX)));
                    if (std::abs (p.y - expectedY) <= 7.5f)
                    {
                        outZone = TimelineClipZone::gainEnvelopeLine;
                        outSegIdx = static_cast<int> (s);
                        return;
                    }
                }
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hitTestLine (e.position, activeZone, hoveredPointIdx, hoveredSegmentIdx);
        const bool canMoveX = clips::canAutomationPointMoveHorizontally (
            clips::AutomationBoundaryPolicy::pinnedEndpoints,
            hoveredPointIdx, static_cast<int> (points.size()));
        const bool canErase = activeZone == TimelineClipZone::gainEnvelopePoint
            && clips::canAutomationPointBeRemoved (
                hoveredPointIdx, static_cast<int> (points.size()));
        const auto role = activeZone == TimelineClipZone::none
            && e.position.y >= getHeight() - 6.0f
            ? clips::TimelineCursorRole::verticalResize
            : clips::resolveTimelineCursorRole ({ activeZone,
                e.mods.isAltDown(), e.mods.isShiftDown(), canMoveX, canErase });
        setMouseCursor (clips::timelineMouseCursor (role));
        repaint();
    }

    void modifierKeysChanged (const juce::ModifierKeys& modifiers) override
    {
        const auto active = (dragZone != TimelineClipZone::none) ? dragZone : activeZone;
        const int ptIdx = (dragZone != TimelineClipZone::none) ? draggedPointIdx : hoveredPointIdx;
        const bool canMoveX = clips::canAutomationPointMoveHorizontally (
            clips::AutomationBoundaryPolicy::pinnedEndpoints,
            ptIdx, static_cast<int> (points.size()));
        const bool canErase = active == TimelineClipZone::gainEnvelopePoint
            && clips::canAutomationPointBeRemoved (
                ptIdx, static_cast<int> (points.size()));
        setMouseCursor (clips::timelineMouseCursor (
            clips::resolveTimelineCursorRole ({ active, modifiers.isAltDown(),
                modifiers.isShiftDown(), canMoveX, canErase })));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        activeZone = TimelineClipZone::none;
        hoveredPointIdx = -1;
        hoveredSegmentIdx = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        hitTestLine (e.position, dragZone, draggedPointIdx, draggedSegmentIdx);

        if (e.mods.isPopupMenu())
        {
            auto context = clips::makeTimelinePointerContext (
                data.id, {}, clips::TimelinePointerTarget::continuousLane,
                clips::ClipKind::continuousLine,
                dragZone == TimelineClipZone::none
                    ? TimelineClipZone::laneBackground : dragZone,
                e.position, clips::timelineSecondsFromCanvasX (
                    e.position.x, getWidth()), e.mods);
            context.pointIndex = draggedPointIdx;
            context.segmentIndex = draggedSegmentIdx;
            host.requestTimelineContextMenu(context);
            dragZone = TimelineClipZone::none;
            draggedPointIdx = -1;
            draggedSegmentIdx = -1;
            return;
        }

        if (dragZone == TimelineClipZone::none)
        {
            if (e.position.y >= getHeight() - 6.0f)
            {
                isResizingLaneHeight = true;
                resizeLaneStartY = e.getScreenY();
                resizeStartLaneHeight = height;
                return;
            }
            isMarqueeActive = marqueeSelector.handleMouseDown (e);
            return;
        }

        // Alt + Click elimina soltanto i punti intermedi: gli endpoint sono permanenti.
        if (e.mods.isAltDown() && !e.mods.isPopupMenu()
            && (dragZone == TimelineClipZone::gainEnvelopePoint || dragZone == TimelineClipZone::gainPointDashedLine))
        {
            if (clips::canAutomationPointBeRemoved (
                    draggedPointIdx, static_cast<int> (points.size())))
            {
                points.erase (points.begin() + draggedPointIdx);
            }
            dragZone = TimelineClipZone::none;
            draggedPointIdx = -1;
            repaint();
            return;
        }

        if (!e.mods.isLeftButtonDown())
            return;

        if ((dragZone == TimelineClipZone::gainEnvelopePoint || dragZone == TimelineClipZone::gainPointDashedLine)
            && draggedPointIdx >= 0)
        {
            if (e.mods.isShiftDown())
                points[static_cast<size_t> (draggedPointIdx)].selected = true;
            else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                points[static_cast<size_t> (draggedPointIdx)].selected = !points[static_cast<size_t> (draggedPointIdx)].selected;
            else if (!points[static_cast<size_t> (draggedPointIdx)].selected)
            {
                for (auto& pt : points) pt.selected = false;
                points[static_cast<size_t> (draggedPointIdx)].selected = true;
            }
        }

        dragStartPos = e.position;
        rollbackPoints = points;
        if (e.mods.isShiftDown()
            && dragZone == TimelineClipZone::gainEnvelopeLine)
        {
            draggedSegmentIdx = clips::pinAutomationSegmentForVerticalDrag (
                points, draggedSegmentIdx);
        }
        snapshotPoints = points;
        hasDragSnapshot = true;

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

        if (!hasDragSnapshot || getWidth() <= 0 || height <= 0)
            return;

        const auto graph = clips::computeContinuousLineLayout (
            juce::Rectangle<float> (0.0f, 0.0f,
                                    static_cast<float> (getWidth()),
                                     static_cast<float> (height)),
            true).graphBounds;
        const float w = graph.getWidth();
        const float h = graph.getHeight();

        if (dragZone == TimelineClipZone::gainPointDashedLine
            && clips::canAutomationPointMoveHorizontally (
                clips::AutomationBoundaryPolicy::pinnedEndpoints,
                draggedPointIdx, static_cast<int> (points.size())))
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            const float curNormX = juce::jlimit (0.0f, 1.0f, (e.position.x - graph.getX()) / w);
            const float minX = points[static_cast<size_t> (draggedPointIdx - 1)].normX + 0.005f;
            const float maxX = points[static_cast<size_t> (draggedPointIdx + 1)].normX - 0.005f;
            points[static_cast<size_t> (draggedPointIdx)].normX = juce::jlimit (minX, maxX, curNormX);
            repaint();
            return;
        }
        else if (dragZone == TimelineClipZone::gainEnvelopePoint && draggedPointIdx >= 0
            && draggedPointIdx < static_cast<int> (points.size()))
        {
            const bool canMoveDraggedX = clips::canAutomationPointMoveHorizontally (
                clips::AutomationBoundaryPolicy::pinnedEndpoints,
                draggedPointIdx, static_cast<int> (points.size()));
            if (!canMoveDraggedX || e.mods.isShiftDown())
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            else
                setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);

            const float curNormY   = juce::jlimit (0.0f, 1.0f, (graph.getBottom() - e.position.y) / h);
            const float curNormX   = juce::jlimit (0.0f, 1.0f, (e.position.x - graph.getX()) / w);
            const float startNormY = juce::jlimit (0.0f, 1.0f, (graph.getBottom() - dragStartPos.y) / h);
            const float startNormX = juce::jlimit (0.0f, 1.0f, (dragStartPos.x - graph.getX()) / w);

            const float deltaY = curNormY - startNormY;
            const float deltaX = e.mods.isShiftDown() ? 0.0f : (curNormX - startNormX);

            for (size_t k = 0; k < points.size() && k < snapshotPoints.size(); ++k)
            {
                if (!snapshotPoints[k].selected)
                    continue;

                points[k].normValue = juce::jlimit (0.0f, 1.0f, snapshotPoints[k].normValue + deltaY);

                if (clips::canAutomationPointMoveHorizontally (
                        clips::AutomationBoundaryPolicy::pinnedEndpoints,
                        static_cast<int> (k), static_cast<int> (points.size())))
                {
                    const float minX = points[k - 1].normX + 0.005f;
                    const float maxX = points[k + 1].normX - 0.005f;
                    points[k].normX = juce::jlimit (minX, maxX, snapshotPoints[k].normX + deltaX);
                }
            }

            // Se si trascina il penultimo nodo e l'ultimo era piatto sul suo stesso valore (coda costante stile TEMPO),
            // mantiene la coda allineata
            if (draggedPointIdx > 0 && draggedPointIdx == static_cast<int> (points.size()) - 2)
            {
                if (std::abs (snapshotPoints.back().normValue - snapshotPoints[static_cast<size_t> (draggedPointIdx)].normValue) < 0.001f)
                    points.back().normValue = points[static_cast<size_t> (draggedPointIdx)].normValue;
            }

            clips::enforceAutomationBoundaryPositions (
                points.data(), static_cast<int> (points.size()),
                clips::AutomationBoundaryPolicy::pinnedEndpoints);

            repaint();
            return;
        }
        else if (dragZone == TimelineClipZone::gainEnvelopeLine && draggedSegmentIdx >= 0
                 && draggedSegmentIdx + 1 < static_cast<int> (points.size()))
        {
            const float deltaY = -(e.position.y - dragStartPos.y) / h;
            const size_t s0 = static_cast<size_t> (draggedSegmentIdx);
            const size_t s1 = s0 + 1;
            points[s0].normValue = juce::jlimit (0.0f, 1.0f, snapshotPoints[s0].normValue + deltaY);
            points[s1].normValue = juce::jlimit (0.0f, 1.0f, snapshotPoints[s1].normValue + deltaY);
            repaint();
        }
        else if (dragZone == TimelineClipZone::gainSegmentCurveHandle && draggedSegmentIdx >= 0
                 && draggedSegmentIdx < static_cast<int> (points.size()) - 1)
        {
            auto& start = points[static_cast<size_t> (draggedSegmentIdx)];
            const auto& end = points[static_cast<size_t> (draggedSegmentIdx + 1)];
            clips::setAutomationControlPoint (
                start, end,
                juce::jlimit (0.0f, 1.0f, (e.position.x - graph.getX()) / w),
                juce::jlimit (0.0f, 1.0f, (graph.getBottom() - e.position.y) / h));
            repaint();
        }
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
        dragZone = TimelineClipZone::none;
        draggedPointIdx = -1;
        draggedSegmentIdx = -1;
        hasDragSnapshot = false;
        mouseMove (e);
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || getWidth() <= 0 || height <= 0) return;

        if (e.position.y >= getHeight() - 6.0f || e.position.y <= 24.0f)
        {
            const int modelHeight = clip_height_law::nextModelHeight (static_cast<float> (height), host.getVerticalZoom());
            host.store().resize (data.id, modelHeight);
            return;
        }

        TimelineClipZone z = TimelineClipZone::none;
        int ptIdx = -1, segIdx = -1;
        hitTestLine (e.position, z, ptIdx, segIdx);

        if ((z == TimelineClipZone::gainEnvelopePoint || z == TimelineClipZone::gainPointDashedLine)
            && ptIdx >= 0 && ptIdx < static_cast<int> (points.size()))
        {
            points[static_cast<size_t> (ptIdx)].normValue = valueToNorm (defaultDisplayValue);
            clips::enforceAutomationBoundaryPositions (
                points.data(), static_cast<int> (points.size()),
                clips::AutomationBoundaryPolicy::pinnedEndpoints);
            repaint();
            return;
        }

        if (z == TimelineClipZone::gainSegmentCurveHandle && segIdx >= 0 && segIdx < static_cast<int> (points.size()) - 1)
        {
            clips::resetAutomationControlPoint (
                points[static_cast<size_t> (segIdx)],
                points[static_cast<size_t> (segIdx + 1)]);
            repaint();
            return;
        }

        // Doppio click sulla linea o sullo sfondo: inserisce un nuovo punto
        const auto graph = clips::computeContinuousLineLayout (
            juce::Rectangle<float> (0.0f, 0.0f,
                                    static_cast<float> (getWidth()),
                                    static_cast<float> (height)),
            true).graphBounds;
        if (!graph.contains (e.position)) { return; }
        const float normX = juce::jlimit (0.01f, 0.99f,
                                          (e.position.x - graph.getX()) / graph.getWidth());
        const float normY = juce::jlimit (0.0f, 1.0f,
                                          (graph.getBottom() - e.position.y) / graph.getHeight());

        for (auto& pt : points) pt.selected = false;
        points.push_back ({ normX, normY, 0.5f, true });
        std::sort (points.begin(), points.end(),
                   [] (const MidiEnvelopePoint& a, const MidiEnvelopePoint& b) { return a.normX < b.normX; });
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
            points = rollbackPoints;
            dragZone = TimelineClipZone::none;
            draggedPointIdx = -1;
            draggedSegmentIdx = -1;
            hasDragSnapshot = false;
            repaint();
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::backspaceKey || key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            bool anyChanged = false;
            if (points.size() > 2)
            {
                auto it = points.begin() + 1;
                while (it != points.end() - 1)
                {
                    if (it->selected) { it = points.erase (it); anyChanged = true; }
                    else ++it;
                }
            }
            if (anyChanged) { repaint(); return true; }
        }
        return false;
    }

    LabComponent& host;
    const juce::Colour curveColour;
    Track data;
    int height = 110;
    bool programAudible = true;

    float minDisplayValue     = 40.0f;
    float maxDisplayValue     = 220.0f;
    float defaultDisplayValue = 120.0f;
    int valueDecimals         = 1;

    std::vector<MidiEnvelopePoint> points;
    std::vector<MidiEnvelopePoint> snapshotPoints;
    std::vector<MidiEnvelopePoint> rollbackPoints;
    std::unique_ptr<ContinuousLineComponent> lineComponent;

    ayra::MarqueeSelector marqueeSelector { *this };
    bool isMarqueeActive = false;

    TimelineClipZone activeZone = TimelineClipZone::none;
    int hoveredPointIdx = -1;
    int hoveredSegmentIdx = -1;

    TimelineClipZone dragZone = TimelineClipZone::none;
    int draggedPointIdx = -1;
    int draggedSegmentIdx = -1;
    bool hasDragSnapshot = false;
    juce::Point<float> dragStartPos;

    bool isResizingLaneHeight = false;
    int resizeLaneStartY = 0;
    int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
