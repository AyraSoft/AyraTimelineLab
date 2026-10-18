/** @file ayra_AudioClipCanvas.cpp
    Adapter del Lab per una lane di clip audio.

    Contiene esclusivamente orchestrazione Message Thread, hit-test e stato
    transiente della fixture. Il documento musicale resta esterno; le singole
    clip sono child `AudioClipComponent` riconciliate per identita'. */

namespace ayra::lab
{
namespace
{
using clips::AudioClipComponent;
using clips::AudioClipVisual;
using clips::ClipFeatures;
using clips::computeClipFeaturesLOD;
using clips::computeClipOverflows;
using clips::GainPoint;
using clips::TimelineClipZone;
namespace clip_gain_law = clips::clip_gain_law;
namespace clip_height_law = clips::clip_height_law;

struct WarpPoint
{
    double sourceTimeSec = 0.0;
    double warpedTimeSec = 0.0;
};

struct ClipUIState
{
    float gainDb = 0.0f;
    std::vector<WarpPoint> warpPoints;
    std::vector<float> warpMarkers;
    std::vector<float> origWarpMarkers;
    std::vector<GainPoint> gainPoints;
    float fadeInCurve = 0.5f;
    float fadeOutCurve = 0.5f;
    float fadeInControlX = 0.5f;
    float fadeOutControlX = 0.5f;
    float fadeInControlWeight = -1.0f;
    float fadeOutControlWeight = -1.0f;

    double origClipLength = 0.0;
    std::int64_t stretchSamples = 0;
    bool isStretching = false;

    double sourceTotalDurationSec = 0.0;
    double sourceOffsetSec = 0.0;
    double startSourceOffsetSec = 0.0;

    std::int64_t slipDeltaSamples = 0;
    bool isSlipping = false;
};

[[nodiscard]] static ayra::curves::CurveBoundarySpec gainBoundarySpec (
    const ClipUIState& ui) noexcept
{
    ayra::curves::CurveBoundarySpec boundary;
    boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
    boundary.fadeInControl = {
        ui.fadeInControlX, ui.fadeInCurve, ui.fadeInControlWeight };
    boundary.fadeOutControl = {
        ui.fadeOutControlX, ui.fadeOutCurve, ui.fadeOutControlWeight };
    return boundary;
}

static void syncClipFadeProjection (Clip& clip, const ClipUIState& ui) noexcept
{
    if (clip.length <= 0.0 || ui.gainPoints.size() < 2) { return; }

    const auto fades = clips::projectAutomationFadeDurations (
        ui.gainPoints.data(), static_cast<int> (ui.gainPoints.size()),
        clip.length,
        [] (const GainPoint& point) noexcept
        {
            return clips::gainPointAsAutomationPoint (point);
        });
    clip.fadeIn = fades.fadeIn;
    clip.fadeOut = fades.fadeOut;
}

static void syncWarpNorms(ClipUIState& ui, double clipLength, double sourceOffset)
{
    ui.warpMarkers.clear();
    ui.origWarpMarkers.clear();
    ui.warpMarkers.reserve(ui.warpPoints.size());
    ui.origWarpMarkers.reserve(ui.warpPoints.size());

    const double len = std::max(0.05, clipLength);
    const bool isStretched = (ui.isStretching || ui.stretchSamples != 0) && (ui.origClipLength > 0.05);

    for (const auto& wp : ui.warpPoints)
    {
        float curNorm = 0.0f;
        float origNorm = 0.0f;

        if (isStretched)
        {
            const float fracInOrig = static_cast<float>((wp.sourceTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
            const float warpedFracInOrig = static_cast<float>((wp.warpedTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
            origNorm = fracInOrig;
            curNorm = warpedFracInOrig;
        }
        else
        {
            curNorm = static_cast<float>((wp.warpedTimeSec - sourceOffset) / len);
            origNorm = static_cast<float>((wp.sourceTimeSec - sourceOffset) / len);
        }

        ui.warpMarkers.push_back(curNorm);
        ui.origWarpMarkers.push_back(origNorm);
    }
}

class AudioClipCanvas final : public juce::Component,
                              public juce::TooltipClient,
                              public clips::AudioClipPresentationProvider
{
public:
    struct WaveCache {
        int seed = -1;
        std::vector<Peaks> rawPeaks;
        std::vector<juce::Path> values;
    };

    explicit AudioClipCanvas (
        LabComponent& h,
        const timeline_visual_style::CurveLineColourVariant colourVariant
            = timeline_visual_style::CurveLineColourVariant::light,
        const TimelineCurveUnderlayStyle underlayStyle
            = TimelineCurveUnderlayStyle::darkBelow)
        : host (h),
          curveColour (timeline_visual_style::curveLineColour (colourVariant)),
          curveUnderlayStyle (underlayStyle)
    {
        setWantsKeyboardFocus(true);
        marqueeSelector.setFillColour(juce::Colour(0x3000d084));
        marqueeSelector.setOutlineColour(juce::Colour(0xe000d084));
        marqueeSelector.onPerformMarqueeSelection = [this](const juce::Rectangle<int>& area, ayra::MarqueeSelector::ModifierMode mode)
        {
            const auto pps = getWidth() / 32.0;
            bool anyChanged = false;
            for (const auto& c : data.clips)
            {
                const auto b = clipBounds(c, pps);
                auto& ui = uiStates[c.id];
                const float waveformHeight = juce::jmax(10.0f, b.getHeight() - clips::audioClipTitleAreaHeight);
                auto dbToY = [&](float db) -> float {
                    const float norm = clip_gain_law::dbToNorm(db);
                    return b.getBottom() - norm * waveformHeight;
                };
                for (size_t gp = 0; gp < ui.gainPoints.size(); ++gp)
                {
                    auto& pt = ui.gainPoints[gp];
                    const float px = clips::automationPointRenderX (
                        b, static_cast<int> (gp),
                        static_cast<int> (ui.gainPoints.size()), pt.normX);
                    const float py = dbToY(pt.gainDb);
                    const bool inside = area.contains(juce::roundToInt(px), juce::roundToInt(py));
                    if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
                    {
                        pt.selected = inside;
                    }
                    else if (mode == ayra::MarqueeSelector::ModifierMode::Add)
                    {
                        if (inside) pt.selected = true;
                    }
                    else if (mode == ayra::MarqueeSelector::ModifierMode::Subtract)
                    {
                        if (inside) pt.selected = false;
                    }
                    else if (mode == ayra::MarqueeSelector::ModifierMode::Toggle)
                    {
                        if (inside) pt.selected = !pt.selected;
                    }
                    anyChanged = true;
                }
            }
            if (anyChanged)
                repaint();
        };

        marqueeSelector.onPerformClickSelection = [this](juce::Point<int>, ayra::MarqueeSelector::ModifierMode mode)
        {
            if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
            {
                for (auto& [cid, ui] : uiStates)
                    for (auto& pt : ui.gainPoints)
                        pt.selected = false;
            }
        };
    }

    void setCurveUnderlayStyle (const TimelineCurveUnderlayStyle style) noexcept
    {
        if (curveUnderlayStyle == style) { return; }
        curveUnderlayStyle = style;
        for (auto& [id, component] : clipComponents)
        {
            juce::ignoreUnused (id);
            component->repaint();
        }
    }

    juce::String getTooltip() override
    {
        switch (activeZone)
        {
            case TimelineClipZone::edgeLeftLoop:
            case TimelineClipZone::edgeRightLoop:
                return "Loop / Duplica clip (Repeat Sizing)\nTrascina verso l'interno per trimmare, verso l'esterno per creare copie concatenate della clip (stile Cubase)";
            case TimelineClipZone::edgeLeftSlip:
            case TimelineClipZone::edgeRightSlip:
                return "Sposta limite con ancoraggio sample (Sizing Moves Contents)\nTrascina per spostare il bordo mantenendo ancorato il sample audio (se c'e' margine nella clip)";
            case TimelineClipZone::edgeLeftTrim:
            case TimelineClipZone::edgeRightTrim:
                return "Trim clip (Normal Sizing)\nRiduce o recupera parte della clip per fare sentire solo una porzione di audio (fino al limite del materiale disponibile)";
            case TimelineClipZone::edgeLeftStretch:
            case TimelineClipZone::edgeRightStretch:
                return "Time Stretch / Warp clip\nTrascina per allungare o accorciare la clip stirando l'audio in campioni (mostra scostamento campioni nel titolo)";
            case TimelineClipZone::gainPointDashedLine:
                return "Tratteggio Fade In/Out\nTrascina orizzontalmente a sinistra/destra per impostare la durata del fade senza alterare il volume di picco";
            case TimelineClipZone::gainEnvelopePoint:
                return "Punto Inviluppo Gain\nTrascina per regolare volume dB e posizione; primo e ultimo sono i fade endpoint e non si eliminano";
            case TimelineClipZone::gainSegmentCurveHandle:
                return "Curvatura Inviluppo Gain\nTrascina in orizzontale e verticale per modellare il segmento (doppio click per lineare)";
            case TimelineClipZone::fadeInCurveHandle:
            case TimelineClipZone::fadeOutCurveHandle:
                return "Curvatura Fade Gain\nTrascina liberamente nel rettangolo del fade (doppio click per lineare)";
            case TimelineClipZone::gainEnvelopeLine:
                return "Linea Inviluppo Gain\nDoppio click per inserire un nuovo punto di automazione, trascina in verticale per alzare/abbassare";
            case TimelineClipZone::topWarpRail:
                return "Binario Warp Superiore\nClicca per inserire un nuovo warp marker sul punto desiderato";
            case TimelineClipZone::warpMarker:
                return "Marker di Warp\nTrascina per dilatare o comprimere il timing audio locale (doppio click per reset, Alt+Click per eliminare)";
            case TimelineClipZone::bottomCutRail:
                return "Binario di Taglio (Forbici)\nClicca per dividere la clip nel punto del cursore";
            case TimelineClipZone::clipTitleMove:
                return "Intestazione Clip\nTrascina per spostare la clip sulla timeline (doppio click per aprire l'editor clip)";
            default:
                if (isHoveringDivider)
                    return "Separatore Traccia\nTrascina verticalmente per ridimensionare l'altezza (Shift o Alt per ridimensionare tutte le tracce, doppio click per reset 152px)";
                if (!hoveredClipId.empty())
                    return "Corpo Clip Audio\nTrascina per selezione marquee dei punti di automazione; i tool bordo dipendono dalla fascia verticale";
                break;
        }
        return {};
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isAltDown() || e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown())
        {
            const float delta = (std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX);
            if (std::abs(delta) > 0.001f)
            {
                host.adjustVerticalZoom(delta > 0.0f ? 0.08f : -0.08f);
                return;
            }
        }
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            vp->mouseWheelMove(e, wheel);
        else
            Component::mouseWheelMove(e, wheel);
    }

    static void generateWarpedWaveforms(WaveCache& cache, const Clip& c, const ClipUIState& ui)
    {
        cache.values.clear();
        cache.values.resize(cache.rawPeaks.size());

        constexpr int kSamples = 256;
        const double totalDur = std::max(0.1, ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (c.length + 8.0));
        const double srcOffset = std::clamp(ui.sourceOffsetSec, 0.0, totalDur);
        const bool isStretched = (ui.isStretching || ui.stretchSamples != 0) && (ui.origClipLength > 0.05);

        auto getSourceAudioSec = [&](float normX) -> double
        {
            if (isStretched)
            {
                return srcOffset + static_cast<double>(normX) * ui.origClipLength;
            }

            const double playbackSec = srcOffset + static_cast<double>(normX) * c.length;
            if (ui.warpPoints.empty())
                return playbackSec;

            if (playbackSec <= ui.warpPoints.front().warpedTimeSec)
            {
                const double t0 = 0.0;
                const double s0 = 0.0;
                const double t1 = ui.warpPoints.front().warpedTimeSec;
                const double s1 = ui.warpPoints.front().sourceTimeSec;
                if (t1 <= t0 + 0.0001) return s1;
                const double frac = std::clamp((playbackSec - t0) / (t1 - t0), 0.0, 1.0);
                return s0 + frac * (s1 - s0);
            }

            for (size_t k = 0; k + 1 < ui.warpPoints.size(); ++k)
            {
                const auto& wp0 = ui.warpPoints[k];
                const auto& wp1 = ui.warpPoints[k + 1];
                if (playbackSec <= wp1.warpedTimeSec)
                {
                    const double span = wp1.warpedTimeSec - wp0.warpedTimeSec;
                    if (span <= 0.0001) return wp0.sourceTimeSec;
                    const double frac = std::clamp((playbackSec - wp0.warpedTimeSec) / span, 0.0, 1.0);
                    return wp0.sourceTimeSec + frac * (wp1.sourceTimeSec - wp0.sourceTimeSec);
                }
            }

            {
                const double t0 = ui.warpPoints.back().warpedTimeSec;
                const double s0 = ui.warpPoints.back().sourceTimeSec;
                const double t1 = totalDur;
                const double s1 = totalDur;
                if (t1 <= t0 + 0.0001) return s0;
                const double frac = std::clamp((playbackSec - t0) / (t1 - t0), 0.0, 1.0);
                return s0 + frac * (s1 - s0);
            }
        };

        const auto boundary = gainBoundarySpec (ui);
        auto evalGainScale = [&](float normX) -> float
        {
            const float gainDb = clips::clip_gain_law::evaluateGainDbAt (
                ui.gainPoints.data(), static_cast<int> (ui.gainPoints.size()),
                ui.gainDb, normX, boundary);
            return clips::clip_gain_law::dbToGainScale (gainDb);
        };

        for (size_t ch = 0; ch < cache.rawPeaks.size(); ++ch)
        {
            const auto& peaks = cache.rawPeaks[ch];
            auto& path = cache.values[ch];
            path.clear();
            path.preallocateSpace(kSamples * 2 + 10);

            for (int i = 0; i < kSamples; ++i)
            {
                const float normX = static_cast<float>(i) / (kSamples - 1.0f);
                const double audioSec = getSourceAudioSec(normX);
                const float srcNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(audioSec / totalDur));
                const float srcIdxF = srcNorm * (peaks.size() - 1.0f);
                const int idx0 = juce::jlimit(0, static_cast<int>(peaks.size() - 1), static_cast<int>(srcIdxF));
                const int idx1 = std::min(idx0 + 1, static_cast<int>(peaks.size() - 1));
                const float frac = srcIdxF - idx0;

                const float maxVal = peaks[static_cast<size_t>(idx0)].maximum * (1.0f - frac) + peaks[static_cast<size_t>(idx1)].maximum * frac;
                const float scale = evalGainScale(normX);

                const float x = 1000.0f * normX;
                const float y = 50.0f - juce::jlimit(0.0f, 48.0f, maxVal * 43.0f * scale);
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }

            for (int i = kSamples - 1; i >= 0; --i)
            {
                const float normX = static_cast<float>(i) / (kSamples - 1.0f);
                const double audioSec = getSourceAudioSec(normX);
                const float srcNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(audioSec / totalDur));
                const float srcIdxF = srcNorm * (peaks.size() - 1.0f);
                const int idx0 = juce::jlimit(0, static_cast<int>(peaks.size() - 1), static_cast<int>(srcIdxF));
                const int idx1 = std::min(idx0 + 1, static_cast<int>(peaks.size() - 1));
                const float frac = srcIdxF - idx0;

                const float minVal = peaks[static_cast<size_t>(idx0)].minimum * (1.0f - frac) + peaks[static_cast<size_t>(idx1)].minimum * frac;
                const float scale = evalGainScale(normX);

                const float x = 1000.0f * normX;
                const float y = 50.0f - juce::jlimit(-48.0f, 0.0f, minVal * 43.0f * scale);
                path.lineTo(x, y);
            }
            path.closeSubPath();
        }
    }

    void updateWaveforms(const std::string& clipId)
    {
        for (auto& c : data.clips)
        {
            if (c.id == clipId && waves.contains(clipId) && uiStates.contains(clipId))
            {
                auto& ui = uiStates[clipId];
                syncClipFadeProjection (c, ui);
                syncWarpNorms(ui, c.length, ui.sourceOffsetSec);
                generateWarpedWaveforms(waves[clipId], c, ui);
                break;
            }
        }
        layoutClipComponents();
    }

    void splitClipUI(const std::string& origId, const std::string& newId, float splitFrac)
    {
        if (!uiStates.contains(origId)) return;
        auto& orig = uiStates[origId];
        ClipUIState next;
        next.gainDb = orig.gainDb;

        const double origLenBefore = orig.origClipLength > 0.0 ? orig.origClipLength : 4.0;
        const double splitSourceTime = orig.sourceOffsetSec + splitFrac * origLenBefore;
        std::vector<WarpPoint> origWps;
        std::vector<WarpPoint> nextWps;
        for (const auto& wp : orig.warpPoints)
        {
            if (wp.sourceTimeSec < splitSourceTime - 0.005)
                origWps.push_back(wp);
            else if (wp.sourceTimeSec > splitSourceTime + 0.005)
                nextWps.push_back(wp);
        }
        orig.warpPoints = std::move(origWps);
        next.warpPoints = std::move(nextWps);

        std::vector<clips::AutomationPoint> source;
        source.reserve (orig.gainPoints.size());
        for (const auto& point : orig.gainPoints)
            source.push_back (clips::gainPointAsAutomationPoint (point));

        const auto split = clips::splitAutomationPointSequence (
            source, clip_gain_law::dbToNorm (orig.gainDb), splitFrac,
            gainBoundarySpec (orig));

        orig.gainPoints.clear();
        orig.gainPoints.reserve (split.left.size());
        for (const auto& point : split.left)
            orig.gainPoints.push_back (clips::automationPointAsGainPoint (point));

        next.gainPoints.reserve (split.right.size());
        for (const auto& point : split.right)
            next.gainPoints.push_back (clips::automationPointAsGainPoint (point));

        orig.fadeInControlX = static_cast<float> (split.leftBoundary.fadeInControl.x);
        orig.fadeInCurve = static_cast<float> (split.leftBoundary.fadeInControl.y);
        orig.fadeInControlWeight = static_cast<float> (
            split.leftBoundary.fadeInControl.weight);
        orig.fadeOutControlX = static_cast<float> (split.leftBoundary.fadeOutControl.x);
        orig.fadeOutCurve = static_cast<float> (split.leftBoundary.fadeOutControl.y);
        orig.fadeOutControlWeight = static_cast<float> (
            split.leftBoundary.fadeOutControl.weight);
        next.fadeInControlX = static_cast<float> (split.rightBoundary.fadeInControl.x);
        next.fadeInCurve = static_cast<float> (split.rightBoundary.fadeInControl.y);
        next.fadeInControlWeight = static_cast<float> (
            split.rightBoundary.fadeInControl.weight);
        next.fadeOutControlX = static_cast<float> (split.rightBoundary.fadeOutControl.x);
        next.fadeOutCurve = static_cast<float> (split.rightBoundary.fadeOutControl.y);
        next.fadeOutControlWeight = static_cast<float> (
            split.rightBoundary.fadeOutControl.weight);

        next.origClipLength = origLenBefore * (1.0 - splitFrac);
        orig.origClipLength = origLenBefore * splitFrac;
        next.sourceTotalDurationSec = orig.sourceTotalDurationSec > 0.0 ? orig.sourceTotalDurationSec : 16.0;
        next.sourceOffsetSec = orig.sourceOffsetSec + splitFrac * origLenBefore;
        next.startSourceOffsetSec = next.sourceOffsetSec;

        syncWarpNorms(orig, orig.origClipLength, orig.sourceOffsetSec);
        syncWarpNorms(next, next.origClipLength, next.sourceOffsetSec);

        uiStates[newId] = std::move(next);
    }

    void duplicateClipLoopUI(const std::string& origId, const std::vector<std::string>& newIds)
    {
        if (!uiStates.contains(origId)) return;
        const auto& orig = uiStates[origId];
        for (const auto& newId : newIds)
        {
            ClipUIState dup = orig;
            dup.isStretching = false;
            dup.isSlipping = false;
            uiStates[newId] = std::move(dup);
            if (waves.contains(origId))
                waves[newId] = waves[origId];
        }
    }

    void setModel(const Track& t, int audioHeight)
    {
        data = t;
        height = audioHeight;
        programAudible = audibility(host.store().state(), data).program;
        std::set<std::string> live;
        for (const auto& c : t.clips)
        {
            live.insert(c.id);
            auto component = clipComponents.find(c.id);
            if (component == clipComponents.end())
            {
                auto& provider = static_cast<clips::AudioClipPresentationProvider&>(*this);
                auto audioClip = std::make_unique<AudioClipComponent>(c.id, provider);
                addAndMakeVisible(*audioClip);
                component = clipComponents.emplace(c.id, std::move(audioClip)).first;
            }
            component->second->toFront(false);

            auto& cache = waves[c.id];
            if (cache.seed != c.seed || cache.rawPeaks.size() != static_cast<std::size_t>(c.channels))
            {
                cache.seed = c.seed;
                cache.rawPeaks.clear();
                cache.rawPeaks.reserve(static_cast<std::size_t>(c.channels));
                for (int ch = 0; ch < c.channels; ++ch)
                    cache.rawPeaks.push_back(makeSyntheticPeaks(c.seed + ch * 19));
            }

            // Inizializza uiStates se nuova clip
            if (!uiStates.contains(c.id))
            {
                ClipUIState s;
                s.origClipLength = c.length;
                s.sourceTotalDurationSec = c.length + 8.0;
                s.sourceOffsetSec = 4.0;
                s.startSourceOffsetSec = s.sourceOffsetSec;

                if (!juce::String(c.id).contains("_cut"))
                {
                    std::vector<float> defaultFractions;
                    if (c.channels == 4)
                        defaultFractions = { 0.22f, 0.58f, 0.81f };
                    else
                        defaultFractions = { 0.35f, 0.70f };

                    for (float frac : defaultFractions)
                    {
                        const double sampleTime = s.sourceOffsetSec + static_cast<double>(frac) * c.length;
                        s.warpPoints.push_back({ sampleTime, sampleTime });
                    }
                }
                syncWarpNorms(s, c.length, s.sourceOffsetSec);
                const auto fadeEndpoints =
                    ayra::curves::fadeEndpointPositionsFromDurations (
                        c.length, c.fadeIn, c.fadeOut);
                s.gainPoints = {
                    { static_cast<float> (fadeEndpoints.first), 0.0f, 0.5f },
                    { static_cast<float> (fadeEndpoints.last), 0.0f, 0.5f }
                };
                uiStates[c.id] = std::move(s);
            }
            else
            {
                syncWarpNorms(uiStates[c.id], c.length, uiStates[c.id].sourceOffsetSec);
            }
            generateWarpedWaveforms(cache, c, uiStates[c.id]);
        }
        for (auto i = waves.begin(); i != waves.end();)
            if (!live.contains(i->first)) i = waves.erase(i); else ++i;
        for (auto i = uiStates.begin(); i != uiStates.end();)
            if (!live.contains(i->first)) i = uiStates.erase(i); else ++i;
        for (auto i = clipComponents.begin(); i != clipComponents.end();)
            if (!live.contains(i->first)) i = clipComponents.erase(i); else ++i;

        layoutClipComponents();
        repaint();
    }

    void setWarpFeatureEnabled(const bool enabled)
    {
        if (clipFeatures.warpEnabled != enabled)
        {
            clipFeatures.warpEnabled = enabled;
            layoutClipComponents();
            repaint();
        }
    }

    [[nodiscard]] bool isWarpFeatureEnabled() const noexcept { return clipFeatures.warpEnabled; }

    void setGainFeatureEnabled(const bool enabled)
    {
        if (clipFeatures.gainEnabled != enabled)
        {
            clipFeatures.gainEnabled = enabled;
            layoutClipComponents();
            repaint();
        }
    }

    [[nodiscard]] bool isGainFeatureEnabled() const noexcept { return clipFeatures.gainEnabled; }

    [[nodiscard]] const ClipFeatures& getClipFeatures() const noexcept { return clipFeatures; }

    juce::Rectangle<float> clipBounds(const Clip& c, double pps) const
    {
        // Space Reclamation: se Warp e' disabilitato, il margine superiore scende da 12 a 4 pt (+8 pt per la waveform!)
        const float topMargin = clipFeatures.warpEnabled ? 12.0f : 4.0f;
        const float bottomMargin = clipFeatures.cutEnabled ? 14.0f : 4.0f;
        const float bodyHeight = juce::jmax(10.0f, static_cast<float>(height) - (topMargin + bottomMargin));
        return { static_cast<float>(c.start * pps), topMargin,
                 static_cast<float>(c.length * pps), bodyHeight };
    }

    void resized() override
    {
        layoutClipComponents();
    }

    void paint(juce::Graphics& g) override
    {
        const auto pps = getWidth() / 32.0;
        auto& laf = host.look();
        laf.drawTrackGrid(g, getLocalBounds(), pps);

        if (data.clips.empty())
        {
            g.setColour(laf.colours().secondary);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12)));
            g.drawText("Ingresso audio / " + juce::String(data.audioChannels) + " canali", 25, 35, getWidth() - 40, 20, juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions().withHeight(10)));
            g.drawText(data.role == Role::performerMix ? "Tensor post-somma, non una cartella" : data.role == Role::songMain ? "Finale Song: Program e Cue distinti" : "Lane pronta per contenuti", 25, 60, getWidth() - 40, 20, juce::Justification::centredLeft);
        }

        if (data.expanded)
        {
            int y = height;
            const auto lane = [&](const juce::String& title)
            {
                g.setColour(laf.colours().border);
                g.drawHorizontalLine(y, 0, static_cast<float>(getWidth()));
                g.setColour(laf.colours().secondary);
                g.drawText(title, 20, y + 8, getWidth() - 30, 18, juce::Justification::centredLeft);
                y += 34;
            };
            for (int ch = 0; ch < data.midiPins; ++ch) lane("MIDI IN " + juce::String(ch + 1) + " / endpoint fixture");
            for (int ch = 0; ch < data.oscPins; ++ch) lane("OSC IN " + juce::String(ch + 1));
            for (int ch = 0; ch < data.dmxPins; ++ch) lane("DMX IN " + juce::String(ch + 1));
            lane(juce::String(data.automations) + " automazioni / fixture");
        }
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (loopPreview.active && loopPreview.duplicateCount > 0 && !draggedClipId.empty())
        {
            const auto pps = getWidth() / 32.0;
            const Clip* cl = nullptr;
            for (const auto& c : data.clips) if (c.id == draggedClipId) { cl = &c; break; }
            if (cl != nullptr)
            {
                const auto origBounds = clipBounds(*cl, pps);
                const auto& laf = host.look();
                const auto tint = laf.colours().accent;
                const juce::Path* wavePath = nullptr;
                if (waves.contains(cl->id) && !waves[cl->id].values.empty())
                    wavePath = &waves[cl->id].values[0];

                for (int k = 0; k < loopPreview.duplicateCount; ++k)
                {
                    const auto dupBounds = clips::computeLoopDuplicateBounds(
                        origBounds, loopPreview.isRightEdge, k);
                    clips::drawLoopDuplicatePreview(
                        g, dupBounds, cl->name.empty() ? cl->id : cl->name,
                        k + 1, loopPreview.isRightEdge, tint, wavePath);
                }
            }
        }

        marqueeSelector.drawMarquee(g);
    }

private:
    [[nodiscard]] AudioClipVisual makeAudioClipVisual (
        const std::string& clipId,
        const juce::Rectangle<float> localBodyBounds,
        const juce::Point<float> parentOrigin) override
    {
        AudioClipVisual visual;
        visual.bounds = localBodyBounds;

        const auto clip = std::find_if(data.clips.begin(), data.clips.end(),
                                       [&clipId](const Clip& candidate)
                                       {
                                           return candidate.id == clipId;
                                       });
        const auto state = uiStates.find(clipId);
        const auto wave = waves.find(clipId);
        if (clip == data.clips.end() || state == uiStates.end() || wave == waves.end())
            return visual;

        auto& ui = state->second;
        syncWarpNorms(ui, clip->length, ui.sourceOffsetSec);

        visual.title = juce::String::fromUTF8(clip->name.c_str());
        visual.tint = juce::Colour(data.colour);
        visual.curveColour = curveColour;
        visual.curveUnderlayStyle = curveUnderlayStyle;
        visual.selected = clipId == host.store().state().selectedClip;
        visual.suspended = !programAudible;
        visual.numChannels = clip->channels;
        visual.channelFormatText = clip->channels == 2
                                 ? juce::String("Stereo")
                                 : juce::String(clip->channels) + " ch / "
                                   + juce::String(data.audioChannels) + " pos";

        visual.gainDb = ui.gainDb;
        visual.fadeInPx = ui.gainPoints.size() >= 2
            ? ui.gainPoints.front().normX * localBodyBounds.getWidth() : 0.0f;
        visual.fadeOutPx = ui.gainPoints.size() >= 2
            ? (1.0f - ui.gainPoints.back().normX) * localBodyBounds.getWidth() : 0.0f;
        visual.fadeInCurve = ui.fadeInCurve;
        visual.fadeOutCurve = ui.fadeOutCurve;
        visual.fadeInControlX = ui.fadeInControlX;
        visual.fadeOutControlX = ui.fadeOutControlX;
        visual.fadeInControlWeight = ui.fadeInControlWeight;
        visual.fadeOutControlWeight = ui.fadeOutControlWeight;

        visual.warpMarkersNorm = ui.warpMarkers.data();
        visual.origWarpMarkersNorm = ui.origWarpMarkers.data();
        visual.numWarpMarkers = static_cast<int>(ui.warpMarkers.size());
        visual.clipLengthSec = clip->length;

        visual.gainPoints = ui.gainPoints.data();
        visual.numGainPoints = static_cast<int>(ui.gainPoints.size());
        visual.waveformPaths = wave->second.values.data();
        visual.numWaveformPaths = static_cast<int>(wave->second.values.size());

        visual.stretchDeltaSamples = ui.stretchSamples;
        visual.isStretchActive = ui.isStretching;
        visual.slipDeltaSamples = ui.slipDeltaSamples;
        const bool isDraggingWarp = (activeZone == TimelineClipZone::warpMarker || activeZone == TimelineClipZone::topWarpRail);
        const bool isDraggingGain = (activeZone == TimelineClipZone::gainEnvelopePoint ||
                                     activeZone == TimelineClipZone::gainEnvelopeLine ||
                                     activeZone == TimelineClipZone::gainSegmentCurveHandle ||
                                     activeZone == TimelineClipZone::fadeInCurveHandle ||
                                     activeZone == TimelineClipZone::fadeOutCurveHandle ||
                                     activeZone == TimelineClipZone::gainPointDashedLine);

        visual.features = computeClipFeaturesLOD(clipFeatures, localBodyBounds.getHeight(), isDraggingWarp, isDraggingGain);

        if (hoveredClipId == clipId || (dragZone != TimelineClipZone::none && draggedClipId == clipId))
        {
            const float localMouseX = mousePos.x - parentOrigin.x;
            visual.hoveredZone = (dragZone != TimelineClipZone::none && draggedClipId == clipId) ? dragZone : activeZone;
            visual.hoveredMarkerIdx = (dragZone != TimelineClipZone::none && draggedClipId == clipId) ? draggedMarkerIdx : hoveredMarkerIdx;
            visual.hoveredGainPointIdx = (dragZone != TimelineClipZone::none && draggedClipId == clipId) ? draggedGainIdx : hoveredGainIdx;
            visual.hoveredCurveSegmentIdx = (dragZone != TimelineClipZone::none && draggedClipId == clipId) ? draggedCurveSegmentIdx : hoveredCurveSegmentIdx;
            visual.hoverMouseX = localMouseX;
            visual.showWarpAddGhost = visual.features.showWarpRail
                                   && activeZone == TimelineClipZone::topWarpRail
                                   && hoveredMarkerIdx < 0;
            visual.warpAddGhostX = localMouseX;
            visual.isDraggingAllGain = false;
        }

        return visual;
    }

    void layoutClipComponents()
    {
        if (getWidth() <= 0)
            return;

        const auto pixelsPerSecond = getWidth() / 32.0;
        const float topOverflow = clipFeatures.warpEnabled ? 8.0f : 0.0f;
        const float bottomOverflow = clipFeatures.cutEnabled ? 6.0f : 0.0f;

        for (const auto& clip : data.clips)
        {
            if (const auto component = clipComponents.find(clip.id);
                component != clipComponents.end())
            {
                component->second->setDecorationOverflows(topOverflow, bottomOverflow, 7.0f);
                component->second->setBodyBoundsInParent(
                    clipBounds(clip, pixelsPerSecond));
            }
        }
    }

    struct HitResult
    {
        const Clip* clip = nullptr;
        TimelineClipZone zone = TimelineClipZone::none;
        int markerIdx = -1;
        int gainIdx = -1;
        int curveSegmentIdx = -1;
        juce::Rectangle<float> bounds;
    };

    HitResult hitTestZones(juce::Point<float> p) const
    {
        HitResult res;
        const auto pps = getWidth() / 32.0;
        const float laneBottom = static_cast<float>(height);

        const bool isDraggingWarp = (activeZone == TimelineClipZone::warpMarker || activeZone == TimelineClipZone::topWarpRail);
        const bool isDraggingGain = (activeZone == TimelineClipZone::gainEnvelopePoint ||
                                     activeZone == TimelineClipZone::gainEnvelopeLine ||
                                     activeZone == TimelineClipZone::gainSegmentCurveHandle ||
                                     activeZone == TimelineClipZone::fadeInCurveHandle ||
                                     activeZone == TimelineClipZone::fadeOutCurveHandle ||
                                     activeZone == TimelineClipZone::gainPointDashedLine);

        for (auto it = data.clips.rbegin(); it != data.clips.rend(); ++it)
        {
            const auto& c = *it;
            const auto b = clipBounds(c, pps);
            const auto feat = computeClipFeaturesLOD(clipFeatures, b.getHeight(), isDraggingWarp, isDraggingGain);

            // Verifica orizzontale: all'interno della clip
            if (p.x >= b.getX() - 6.0f && p.x <= b.getRight() + 6.0f)
            {
                // A) Barra inferiore di taglio: STRETTAMENTE SULLA BARRA SOTTO LA CLIP (b.getBottom() fino a b.getBottom() + 8.0f)
                if (feat.showCutRail && p.y >= b.getBottom() && p.y <= b.getBottom() + 8.0f)
                {
                    res.clip = &c;
                    res.bounds = b;
                    res.zone = TimelineClipZone::bottomCutRail;
                    return res;
                }

                // B) Top Warp Rail / Header Top Area
                if (p.y >= 0.0f && p.y < b.getY())
                {
                    res.clip = &c;
                    res.bounds = b;
                    res.zone = feat.showWarpRail ? TimelineClipZone::topWarpRail : TimelineClipZone::clipTitleMove;
                    if (feat.showWarpRail)
                    {
                        const auto& ui = uiStates.at(c.id);
                        for (int m = 0; m < static_cast<int>(ui.warpMarkers.size()); ++m)
                        {
                            const float curNorm = ui.warpMarkers[static_cast<size_t>(m)];
                            if (curNorm < -0.001f || curNorm > 1.001f) continue;
                            const float wx = b.getX() + curNorm * b.getWidth();
                            if (std::abs(p.x - wx) <= 8.0f)
                            {
                                res.zone = TimelineClipZone::warpMarker;
                                res.markerIdx = m;
                                break;
                            }
                        }
                    }
                    return res;
                }

                // C) All'interno del corpo della clip (b.getY() a b.getBottom())
                if (p.y >= b.getY() && p.y < b.getBottom())
                {
                    res.clip = &c;
                    res.bounds = b;
                    const auto& ui = uiStates.at(c.id);

                    const float waveformHeight = juce::jmax(10.0f, b.getHeight() - clips::audioClipTitleAreaHeight);
                    const juce::Rectangle<float> curveBounds (
                        b.getX(), b.getBottom() - waveformHeight,
                        b.getWidth(), waveformHeight);

                    auto dbToY = [&](float db) -> float
                    {
                        const float norm = clip_gain_law::dbToNorm(db);
                        return b.getBottom() - norm * waveformHeight;
                    };

                    // 1. Bracket laterali di Loop / Slip / Trim / Stretch (STRETTAMENTE sul filo del bordo 0-2.5px)
                    if (p.x <= b.getX() + 2.5f)
                    {
                        const float relY = (p.y - b.getY()) / b.getHeight();
                        res.zone = clips::computeClipEdgeZone (true, relY);
                        return res;
                    }
                    if (p.x >= b.getRight() - 2.5f)
                    {
                        const float relY = (p.y - b.getY()) / b.getHeight();
                        res.zone = clips::computeClipEdgeZone (false, relY);
                        return res;
                    }

                    // 2. Zona Titolo in cima alla clip: presa per spostare l'intera clip a destra o sinistra
                    if (p.y <= b.getY() + clips::audioClipTitleAreaHeight)
                    {
                        res.zone = TimelineClipZone::clipTitleMove;
                        return res;
                    }

                    // Interazioni Gain (curvatura, punti, tratteggio, linea): solo se il Gain Envelope e' visibile (LOD / enabled)
                    if (feat.showGainEnvelope)
                    {
                        const auto boundaryGeometry =
                            clips::computeProjectedAutomationBoundaryGeometry (
                                curveBounds, ui.gainPoints.data(),
                                static_cast<int> (ui.gainPoints.size()),
                                gainBoundarySpec (ui),
                                [] (const GainPoint& point) noexcept
                                {
                                    return clips::gainPointAsAutomationPoint (point);
                                });
                        if (boundaryGeometry.hasFadeIn
                            && boundaryGeometry.fadeIn.control.getDistanceFrom (p) <= 8.5f)
                        {
                            res.zone = TimelineClipZone::fadeInCurveHandle;
                            return res;
                        }
                        if (boundaryGeometry.hasFadeOut
                            && boundaryGeometry.fadeOut.control.getDistanceFrom (p) <= 8.5f)
                        {
                            res.zone = TimelineClipZone::fadeOutCurveHandle;
                            return res;
                        }

                        // 3. Handle di curvatura dei segmenti (1:1 mapping, prioritari sui punti se vicini)
                        if (ui.gainPoints.size() >= 2)
                        {
                            for (size_t s = 0; s + 1 < ui.gainPoints.size(); ++s)
                            {
                                const auto& n0 = ui.gainPoints[s];
                                const auto& n1 = ui.gainPoints[s + 1];
                                const auto handle = clips::automationControlPixelPosition (
                                    curveBounds,
                                    clips::gainPointAsAutomationPoint (n0),
                                    clips::gainPointAsAutomationPoint (n1),
                                    static_cast<int> (s),
                                    static_cast<int> (ui.gainPoints.size()));

                                if (handle.getDistanceFrom(p) <= 8.5f)
                                {
                                    res.zone = TimelineClipZone::gainSegmentCurveHandle;
                                    res.curveSegmentIdx = static_cast<int>(s);
                                    return res;
                                }
                            }
                        }

                        // 4. Handle dei punti Gain, inclusi gli endpoint di fade permanenti
                        for (int gp = 0; gp < static_cast<int>(ui.gainPoints.size()); ++gp)
                        {
                            const auto& pt = ui.gainPoints[static_cast<size_t>(gp)];
                            const auto point = clips::automationPointPixelPosition (
                                curveBounds, clips::gainPointAsAutomationPoint (pt),
                                gp, static_cast<int> (ui.gainPoints.size()));

                            if (point.getDistanceFrom(p) <= 8.5f)
                            {
                                res.zone = TimelineClipZone::gainEnvelopePoint;
                                res.gainIdx = gp;
                                return res;
                            }
                        }

                        // 5. Linee tratteggiate verticali dei punti di gain: drag orizzontale esclusivo (frecce sx/dx)
                        for (int gp = 0; gp < static_cast<int>(ui.gainPoints.size()); ++gp)
                        {
                            const auto& pt = ui.gainPoints[static_cast<size_t>(gp)];
                            const float px = clips::automationPointRenderX (
                                curveBounds, gp,
                                static_cast<int> (ui.gainPoints.size()), pt.normX);
                            const float py = dbToY(pt.gainDb);

                            if (p.y >= py + 6.0f && p.y <= b.getBottom() - 4.0f)
                            {
                                bool hitDashed = false;
                                if (gp == 0 && pt.normX <= 0.01f)
                                {
                                    // Per punto estremo sinistro: aggancio da 2.5px a 16px dentro il bordo
                                    hitDashed = (p.x > b.getX() + 2.5f && p.x <= b.getX() + 16.0f);
                                }
                                else if (gp == static_cast<int>(ui.gainPoints.size()) - 1 && pt.normX >= 0.99f)
                                {
                                    // Per punto estremo destro: aggancio da 2.5px a 16px dentro il bordo
                                    hitDashed = (p.x >= b.getRight() - 16.0f && p.x < b.getRight() - 2.5f);
                                }
                                else
                                {
                                    hitDashed = (std::abs(p.x - px) <= 7.0f);
                                }

                                if (hitDashed)
                                {
                                    res.zone = TimelineClipZone::gainPointDashedLine;
                                    res.gainIdx = gp;
                                    return res;
                                }
                            }
                        }

                        // 6. Linea di Inviluppo Gain (segmenti tra punti)
                        if (ui.gainPoints.size() >= 2)
                        {
                            for (size_t s = 0; s + 1 < ui.gainPoints.size(); ++s)
                            {
                                const auto& n0 = ui.gainPoints[s];
                                const auto& n1 = ui.gainPoints[s + 1];
                                const float x0 = clips::automationPointRenderX (
                                    curveBounds, static_cast<int> (s),
                                    static_cast<int> (ui.gainPoints.size()), n0.normX);
                                const float x1 = clips::automationPointRenderX (
                                    curveBounds, static_cast<int> (s + 1),
                                    static_cast<int> (ui.gainPoints.size()), n1.normX);
                                if (p.x >= x0 && p.x <= x1)
                                {
                                    const float span = std::max(0.001f, x1 - x0);
                                    const float u = juce::jlimit(0.0f, 1.0f, (p.x - x0) / span);
                                    const auto a0 = clips::gainPointAsAutomationPoint (n0);
                                    const auto a1 = clips::gainPointAsAutomationPoint (n1);
                                    const float expectedY = b.getBottom() - waveformHeight
                                        * clips::evaluateAutomationSegment (
                                            a0, a1, a0.normX + u * (a1.normX - a0.normX));
                                    if (std::abs(p.y - expectedY) <= 8.0f)
                                    {
                                        res.zone = TimelineClipZone::gainEnvelopeLine;
                                        res.curveSegmentIdx = static_cast<int>(s);
                                        return res;
                                    }
                                    break;
                                }
                            }
                        }
                    }

                    res.zone = TimelineClipZone::none;
                    return res;
                }
            }
        }
        return res;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        mousePos = e.position;
        const auto hit = hitTestZones(e.position);
        if (hit.clip != nullptr)
        {
            hoveredClipId = hit.clip->id;
            activeZone = hit.zone;
            hoveredMarkerIdx = hit.markerIdx;
            hoveredGainIdx = hit.gainIdx;
            hoveredCurveSegmentIdx = hit.curveSegmentIdx;

            const auto& ui = uiStates.at (hit.clip->id);
            const bool canMoveX = activeZone != TimelineClipZone::gainEnvelopePoint
                || clips::canAutomationPointMoveHorizontally (
                    gainBoundarySpec (ui).mode, hit.gainIdx,
                    static_cast<int> (ui.gainPoints.size()));
            const bool canErase = clips::isClipDeletionZone (activeZone)
                || activeZone == TimelineClipZone::warpMarker
                || (activeZone == TimelineClipZone::gainEnvelopePoint
                    && clips::canAutomationPointBeRemoved (
                        hit.gainIdx, static_cast<int> (ui.gainPoints.size())));
            setMouseCursor (clips::timelineMouseCursor (
                clips::resolveTimelineCursorRole ({ activeZone,
                    e.mods.isAltDown(), e.mods.isShiftDown(), canMoveX, canErase })));
        }
        else
        {
            hoveredClipId.clear();
            activeZone = TimelineClipZone::none;
            hoveredMarkerIdx = -1;
            hoveredGainIdx = -1;
            hoveredCurveSegmentIdx = -1;
            isHoveringDivider = (e.position.y >= getHeight() - 6.0f);
            if (isHoveringDivider)
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            else
                setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        repaint();
    }

    void modifierKeysChanged (const juce::ModifierKeys& modifiers) override
    {
        const auto active = (dragZone != TimelineClipZone::none) ? dragZone : activeZone;
        const auto& cid = (dragZone != TimelineClipZone::none) ? draggedClipId : hoveredClipId;
        const int pointIdx = (dragZone != TimelineClipZone::none) ? draggedGainIdx : hoveredGainIdx;
        bool canMoveX = true;
        bool canErase = clips::isClipDeletionZone (active) || active == TimelineClipZone::warpMarker;
        if (uiStates.contains (cid))
        {
            const auto& ui = uiStates.at (cid);
            canMoveX = clips::canAutomationPointMoveHorizontally (
                gainBoundarySpec (ui).mode, pointIdx,
                static_cast<int> (ui.gainPoints.size()));
            canErase = canErase || (active == TimelineClipZone::gainEnvelopePoint
                && clips::canAutomationPointBeRemoved (
                    pointIdx, static_cast<int> (ui.gainPoints.size())));
        }
        setMouseCursor (clips::timelineMouseCursor (
            clips::resolveTimelineCursorRole ({ active, modifiers.isAltDown(),
                modifiers.isShiftDown(), canMoveX, canErase })));
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredClipId.clear();
        activeZone = TimelineClipZone::none;
        hoveredMarkerIdx = -1;
        hoveredGainIdx = -1;
        hoveredCurveSegmentIdx = -1;
        isHoveringDivider = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto hit = hitTestZones(e.position);

        if (hit.clip != nullptr && e.mods.isPopupMenu())
        {
            auto context = clips::makeTimelinePointerContext (
                data.id, hit.clip->id, clips::TimelinePointerTarget::clip,
                clips::ClipKind::audio, clips::semanticClipZone (hit.zone),
                e.position, clips::timelineSecondsFromCanvasX (
                    e.position.x, getWidth()), e.mods);
            context.markerIndex = hit.markerIdx;
            context.pointIndex = hit.gainIdx;
            context.segmentIndex = hit.curveSegmentIdx;
            host.requestTimelineContextMenu(context);
            return;
        }

        if (hit.clip != nullptr && e.mods.isAltDown()
            && e.mods.isLeftButtonDown()
            && clips::isClipDeletionZone (hit.zone))
        {
            std::ignore = host.deleteClip(data.id, hit.clip->id);
            return;
        }

        if (!hit.clip || hit.zone == TimelineClipZone::none)
        {
            if (e.position.y >= getHeight() - 6.0f)
            {
                isResizingLaneHeight = true;
                resizeLaneStartY = e.getScreenY();
                resizeStartLaneHeight = height;
                return;
            }
            isMarqueeActive = marqueeSelector.handleMouseDown(e);
            return;
        }

        host.selectClip(hit.clip->id);
        grabKeyboardFocus();

        draggedClipId = hit.clip->id;
        dragZone = hit.zone;
        draggedMarkerIdx = hit.markerIdx;
        draggedGainIdx = hit.gainIdx;
        draggedCurveSegmentIdx = hit.curveSegmentIdx;
        dragStartPos = e.position;

        auto& ui = uiStates[draggedClipId];

        // Alt + Click su Warp Marker: Elimina (NON con il tasto destro)
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && hit.zone == TimelineClipZone::warpMarker)
        {
            if (hit.markerIdx >= 0 && hit.markerIdx < static_cast<int>(ui.warpPoints.size()))
            {
                ui.warpPoints.erase(ui.warpPoints.begin() + hit.markerIdx);
                syncWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
                updateWaveforms(draggedClipId);
                repaint();
                return;
            }
        }

        // Alt + Click su punto di inviluppo Gain: Elimina punto (gli estremi non si possono eliminare, NON con il tasto destro)
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && hit.zone == TimelineClipZone::gainEnvelopePoint)
        {
            if (clips::canAutomationPointBeRemoved (
                    hit.gainIdx, static_cast<int> (ui.gainPoints.size())))
            {
                ui.gainPoints.erase(ui.gainPoints.begin() + hit.gainIdx);
                updateWaveforms(draggedClipId);
            }
            draggedClipId.clear();
            dragZone = TimelineClipZone::none;
            draggedGainIdx = -1;
            repaint();
            return;
        }

        if (!e.mods.isLeftButtonDown()) return;

        // Gestione selezione punti di Gain
        if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.gainIdx >= 0 && hit.gainIdx < static_cast<int>(ui.gainPoints.size()))
        {
            if (e.mods.isShiftDown())
            {
                ui.gainPoints[static_cast<size_t>(hit.gainIdx)].selected = true;
            }
            else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            {
                ui.gainPoints[static_cast<size_t>(hit.gainIdx)].selected = !ui.gainPoints[static_cast<size_t>(hit.gainIdx)].selected;
            }
            else
            {
                if (!ui.gainPoints[static_cast<size_t>(hit.gainIdx)].selected)
                {
                    for (auto& pt : ui.gainPoints) pt.selected = false;
                    ui.gainPoints[static_cast<size_t>(hit.gainIdx)].selected = true;
                }
            }
        }

        // Presa del titolo per spostare la clip
        if (hit.zone == TimelineClipZone::clipTitleMove)
        {
            startClipStart = hit.clip->start;
            return;
        }

        // Drag esclusivo orizzontale della linea tratteggiata del punto
        if (hit.zone == TimelineClipZone::gainPointDashedLine && hit.gainIdx >= 0)
        {
            startPtNormX = ui.gainPoints[static_cast<size_t>(hit.gainIdx)].normX;
            startGainPoints = ui.gainPoints;
            return;
        }

        // Click su Top Warp Rail: aggiungi marker
        if (dragZone == TimelineClipZone::topWarpRail && hit.markerIdx < 0)
        {
            const float normX = juce::jlimit(0.01f, 0.99f, (e.position.x - hit.bounds.getX()) / hit.bounds.getWidth());
            double sampleTime = 0.0;
            if (ui.isStretching || ui.stretchSamples != 0)
                sampleTime = ui.startSourceOffsetSec + static_cast<double>(normX) * ui.origClipLength;
            else
                sampleTime = ui.sourceOffsetSec + static_cast<double>(normX) * hit.clip->length;

            ui.warpPoints.push_back({ sampleTime, sampleTime });
            std::sort(ui.warpPoints.begin(), ui.warpPoints.end(),
                      [](const WarpPoint& a, const WarpPoint& b) { return a.sourceTimeSec < b.sourceTimeSec; });
            syncWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
            updateWaveforms(draggedClipId);
            repaint();
            return;
        }

        // Click su Bottom Scissor Cut Rail: taglia la clip partizionando markers senza duplicati
        if (dragZone == TimelineClipZone::bottomCutRail)
        {
            const auto pps = getWidth() / 32.0;
            const double splitTime = e.position.x / pps;
            const float splitFrac = static_cast<float>((splitTime - hit.clip->start) / hit.clip->length);

            const auto newId = host.splitClip(data.id, hit.clip->id, splitTime);
            if (!newId.empty())
                splitClipUI(hit.clip->id, newId, splitFrac);
            return;
        }

        if (e.mods.isShiftDown()
            && hit.zone == TimelineClipZone::gainEnvelopeLine)
        {
            draggedCurveSegmentIdx = clips::pinAutomationSegmentForVerticalDrag (
                ui.gainPoints, hit.curveSegmentIdx);
        }

        // Salva gli stati dopo l'eventuale duplicazione pinned del segmento.
        startGainPoints = ui.gainPoints;
        startClipStart = hit.clip->start;
        startClipLength = hit.clip->length;
        if (ui.origClipLength <= 0.001)
            ui.origClipLength = hit.clip->length;
        if (ui.sourceTotalDurationSec <= 0.001)
        {
            ui.sourceTotalDurationSec = hit.clip->length + 8.0;
            ui.sourceOffsetSec = 4.0;
        }
        startSourceOffsetSec = ui.sourceOffsetSec;

        loopPreview = {};
        if (hit.zone == TimelineClipZone::edgeLeftLoop || hit.zone == TimelineClipZone::edgeRightLoop)
        {
            loopPreview.active = true;
            loopPreview.clipId = hit.clip->id;
            loopPreview.isRightEdge = (hit.zone == TimelineClipZone::edgeRightLoop);
            loopPreview.originalStart = startClipStart;
            loopPreview.originalLength = startClipLength;
            loopPreview.duplicateCount = 0;
            loopPreview.trimDelta = 0.0;
        }

        if (hit.zone == TimelineClipZone::edgeLeftStretch || hit.zone == TimelineClipZone::edgeRightStretch)
        {
            ui.origClipLength = hit.clip->length;
            ui.startSourceOffsetSec = ui.sourceOffsetSec;
        }

    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        mousePos = e.position;
        if (isResizingLaneHeight)
        {
            const int deltaY = e.getScreenY() - resizeLaneStartY;
            const int newH = std::clamp(resizeStartLaneHeight + deltaY, minimumHeight(data), 1200);
            if (e.mods.isShiftDown() || e.mods.isAltDown())
            {
                const float factor = static_cast<float>(newH) / static_cast<float>(juce::jmax(1, resizeStartLaneHeight));
                host.adjustVerticalZoom(factor - 1.0f);
            }
            else
            {
                host.store().resize(data.id, newH);
            }
            return;
        }

        if (isMarqueeActive)
        {
            marqueeSelector.handleMouseDrag(e);
            return;
        }

        if (draggedClipId.empty() || !uiStates.contains(draggedClipId))
        {
            repaint();
            return;
        }

        auto& ui = uiStates[draggedClipId];
        const auto pps = getWidth() / 32.0;

        Clip* cl = nullptr;
        for (auto& c : data.clips) if (c.id == draggedClipId) { cl = &c; break; }
        if (!cl) return;
        const auto b = clipBounds(*cl, pps);

        // A) Spostamento della clip a destra o sinistra tramite presa sul titolo
        if (dragZone == TimelineClipZone::clipTitleMove)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            cl->start = std::max(0.0, startClipStart + deltaSec);
            layoutClipComponents();
            repaint();
            return;
        }

        if (dragZone == TimelineClipZone::fadeInCurveHandle ||
            dragZone == TimelineClipZone::fadeOutCurveHandle)
        {
            const float waveformHeight = juce::jmax (
                10.0f, b.getHeight() - clips::audioClipTitleAreaHeight);
            const juce::Rectangle<float> curveBounds (
                b.getX(), b.getBottom() - waveformHeight,
                b.getWidth(), waveformHeight);
            const auto geometry = clips::computeProjectedAutomationBoundaryGeometry (
                curveBounds, ui.gainPoints.data(),
                static_cast<int> (ui.gainPoints.size()),
                gainBoundarySpec (ui),
                [] (const GainPoint& point) noexcept
                {
                    return clips::gainPointAsAutomationPoint (point);
                });
            const bool fadeIn = dragZone == TimelineClipZone::fadeInCurveHandle;
            const auto& segment = fadeIn ? geometry.fadeIn : geometry.fadeOut;
            const auto control = ayra::curves::makeBoundedQuadraticControl (
                { segment.start.x, segment.start.y,
                  segment.end.x, segment.end.y },
                e.position.x, e.position.y);
            if (fadeIn)
            {
                ui.fadeInControlX = static_cast<float> (control.x);
                ui.fadeInCurve = static_cast<float> (control.y);
                ui.fadeInControlWeight = -1.0f;
            }
            else
            {
                ui.fadeOutControlX = static_cast<float> (control.x);
                ui.fadeOutCurve = static_cast<float> (control.y);
                ui.fadeOutControlWeight = -1.0f;
            }
            updateWaveforms (draggedClipId);
            repaint();
            return;
        }

        // B) Drag esclusivo orizzontale della linea tratteggiata (frecce sx/dx)
        if (dragZone == TimelineClipZone::gainPointDashedLine && draggedGainIdx >= 0)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            if (draggedGainIdx < static_cast<int>(ui.gainPoints.size()))
            {
                const float currentNormX = juce::jlimit(0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
                const float startNormX = juce::jlimit(0.0f, 1.0f, (dragStartPos.x - b.getX()) / b.getWidth());
                const float deltaX = currentNormX - startNormX;

                const bool isMulti = ui.gainPoints[static_cast<size_t>(draggedGainIdx)].selected;
                if (isMulti)
                {
                    for (size_t k = 0; k < ui.gainPoints.size() && k < startGainPoints.size(); ++k)
                    {
                        if (startGainPoints[k].selected)
                        {
                            const float minX = (k == 0) ? 0.0f : (ui.gainPoints[k - 1].normX + 0.01f);
                            const float maxX = (k == ui.gainPoints.size() - 1) ? 1.0f : (ui.gainPoints[k + 1].normX - 0.01f);
                            ui.gainPoints[k].normX = juce::jlimit(minX, maxX, startGainPoints[k].normX + deltaX);
                        }
                    }
                }
                else
                {
                    const float minX = (draggedGainIdx == 0) ? 0.0f : (ui.gainPoints[static_cast<size_t>(draggedGainIdx - 1)].normX + 0.01f);
                    const float maxX = (draggedGainIdx == static_cast<int>(ui.gainPoints.size()) - 1) ? 1.0f : (ui.gainPoints[static_cast<size_t>(draggedGainIdx + 1)].normX - 0.01f);
                    ui.gainPoints[static_cast<size_t>(draggedGainIdx)].normX = juce::jlimit(minX, maxX, currentNormX);
                }
                updateWaveforms(draggedClipId);
                repaint();
                return;
            }
        }

        if (dragZone == TimelineClipZone::gainEnvelopeLine && draggedCurveSegmentIdx >= 0)
        {
            if (draggedCurveSegmentIdx + 1 < static_cast<int>(ui.gainPoints.size()))
            {
                const float deltaDb = -(e.position.y - dragStartPos.y) * 0.40f;
                const size_t s0 = static_cast<size_t>(draggedCurveSegmentIdx);
                const size_t s1 = s0 + 1;
                ui.gainPoints[s0].gainDb = juce::jlimit(-60.0f, 48.0f, startGainPoints[s0].gainDb + deltaDb);
                ui.gainPoints[s1].gainDb = juce::jlimit(-60.0f, 48.0f, startGainPoints[s1].gainDb + deltaDb);
                updateWaveforms(draggedClipId);
                repaint();
            }
        }
        else if (dragZone == TimelineClipZone::gainEnvelopePoint && draggedGainIdx >= 0)
        {
            if (e.mods.isShiftDown())
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            else
                setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);

            if (draggedGainIdx < static_cast<int>(ui.gainPoints.size()))
            {
                const float waveformHeight = juce::jmax(10.0f, b.getHeight() - clips::audioClipTitleAreaHeight);
                const float normY = juce::jlimit(0.0f, 1.0f, (b.getBottom() - e.position.y) / waveformHeight);
                const float currentDb = clip_gain_law::normToDb(normY);
                const float currentNormX = juce::jlimit(0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());

                const float startNormY = juce::jlimit(0.0f, 1.0f, (b.getBottom() - dragStartPos.y) / waveformHeight);
                const float startDb = clip_gain_law::normToDb(startNormY);
                const float startNormX = juce::jlimit(0.0f, 1.0f, (dragStartPos.x - b.getX()) / b.getWidth());

                const float deltaDb = currentDb - startDb;
                const float deltaX = e.mods.isShiftDown() ? 0.0f : (currentNormX - startNormX);

                const bool isMulti = ui.gainPoints[static_cast<size_t>(draggedGainIdx)].selected;
                if (isMulti)
                {
                    for (size_t k = 0; k < ui.gainPoints.size() && k < startGainPoints.size(); ++k)
                    {
                        if (startGainPoints[k].selected)
                        {
                            ui.gainPoints[k].gainDb = juce::jlimit(-60.0f, 48.0f, startGainPoints[k].gainDb + deltaDb);
                            if (!e.mods.isShiftDown())
                            {
                                const float minX = (k == 0) ? 0.0f : (ui.gainPoints[k - 1].normX + 0.01f);
                                const float maxX = (k == ui.gainPoints.size() - 1) ? 1.0f : (ui.gainPoints[k + 1].normX - 0.01f);
                                ui.gainPoints[k].normX = juce::jlimit(minX, maxX, startGainPoints[k].normX + deltaX);
                            }
                        }
                    }
                }
                else
                {
                    const float minX = (draggedGainIdx == 0) ? 0.0f : (ui.gainPoints[static_cast<size_t>(draggedGainIdx - 1)].normX + 0.01f);
                    const float maxX = (draggedGainIdx == static_cast<int>(ui.gainPoints.size()) - 1) ? 1.0f : (ui.gainPoints[static_cast<size_t>(draggedGainIdx + 1)].normX - 0.01f);
                    if (!e.mods.isShiftDown())
                        ui.gainPoints[static_cast<size_t>(draggedGainIdx)].normX = juce::jlimit(minX, maxX, currentNormX);
                    ui.gainPoints[static_cast<size_t>(draggedGainIdx)].gainDb = currentDb;
                }
                updateWaveforms(draggedClipId);
                repaint();
            }
        }
        else if (dragZone == TimelineClipZone::gainSegmentCurveHandle && draggedCurveSegmentIdx >= 0)
        {
            if (draggedCurveSegmentIdx < static_cast<int>(ui.gainPoints.size()) - 1)
            {
                auto& start = ui.gainPoints[static_cast<size_t>(draggedCurveSegmentIdx)];
                const auto& end = ui.gainPoints[static_cast<size_t>(draggedCurveSegmentIdx + 1)];
                const float waveformHeight = juce::jmax (
                    10.0f, b.getHeight() - clips::audioClipTitleAreaHeight);
                clips::setGainControlPoint (
                    start, end,
                    juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth()),
                    juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / waveformHeight));
            }
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftLoop)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            if (deltaSec >= 0.0)
            {
                const double maxDelta = startClipLength - 0.125;
                const double clampedDelta = std::clamp(deltaSec, 0.0, maxDelta);
                cl->start = startClipStart + clampedDelta;
                cl->length = startClipLength - clampedDelta;
                ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
                ui.origClipLength = cl->length;
                ui.stretchSamples = 0;
                ui.isStretching = false;
                loopPreview.duplicateCount = 0;
                loopPreview.trimDelta = clampedDelta;
            }
            else
            {
                cl->start = startClipStart;
                cl->length = startClipLength;
                ui.sourceOffsetSec = startSourceOffsetSec;
                ui.origClipLength = cl->length;
                ui.stretchSamples = 0;
                ui.isStretching = false;
                loopPreview.duplicateCount = clips::computeLoopDuplicateCount(
                    false, deltaSec, startClipStart, startClipLength);
                loopPreview.trimDelta = 0.0;
            }
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightLoop)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            if (deltaSec <= 0.0)
            {
                const double minDelta = -(startClipLength - 0.125);
                const double clampedDelta = std::clamp(deltaSec, minDelta, 0.0);
                cl->length = startClipLength + clampedDelta;
                ui.origClipLength = cl->length;
                ui.stretchSamples = 0;
                ui.isStretching = false;
                loopPreview.duplicateCount = 0;
                loopPreview.trimDelta = clampedDelta;
            }
            else
            {
                cl->start = startClipStart;
                cl->length = startClipLength;
                ui.origClipLength = cl->length;
                ui.stretchSamples = 0;
                ui.isStretching = false;
                loopPreview.duplicateCount = clips::computeLoopDuplicateCount(
                    true, deltaSec, startClipStart, startClipLength);
                loopPreview.trimDelta = 0.0;
            }
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftSlip)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double totalDur = ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (startClipLength + 8.0);
            const double maxExpand = std::max(0.0, totalDur - (startSourceOffsetSec + startClipLength));
            const double minDelta = -std::min(maxExpand, startClipStart);
            const double maxDelta = startClipLength - 0.125;
            const double clampedDelta = std::clamp(deltaSec, minDelta, maxDelta);

            cl->start = startClipStart + clampedDelta;
            cl->length = startClipLength - clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec;
            ui.origClipLength = cl->length;
            ui.stretchSamples = 0;
            ui.isStretching = false;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightSlip)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double maxExpand = std::max(0.0, startSourceOffsetSec);
            const double minDelta = -(startClipLength - 0.125);
            const double maxDelta = maxExpand;
            const double clampedDelta = std::clamp(deltaSec, minDelta, maxDelta);

            cl->length = startClipLength + clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec - clampedDelta;
            ui.origClipLength = cl->length;
            ui.stretchSamples = 0;
            ui.isStretching = false;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftTrim)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double maxExpand = std::max(0.0, startSourceOffsetSec);
            const double minDelta = -std::min(maxExpand, startClipStart);
            const double maxDelta = startClipLength - 0.125;
            const double clampedDelta = std::clamp(deltaSec, minDelta, maxDelta);

            cl->start = startClipStart + clampedDelta;
            cl->length = startClipLength - clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
            ui.origClipLength = cl->length;
            ui.stretchSamples = 0;
            ui.isStretching = false;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightTrim)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double totalDur = ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (startClipLength + 8.0);
            const double maxExpand = std::max(0.0, totalDur - (startSourceOffsetSec + startClipLength));
            const double minDelta = -(startClipLength - 0.125);
            const double maxDelta = maxExpand;
            const double clampedDelta = std::clamp(deltaSec, minDelta, maxDelta);

            cl->length = startClipLength + clampedDelta;
            ui.origClipLength = cl->length;
            ui.stretchSamples = 0;
            ui.isStretching = false;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftStretch)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double fixedEnd = startClipStart + startClipLength;
            const double newStart = std::clamp(startClipStart + deltaSec, 0.0, fixedEnd - 0.125);
            cl->start = newStart;
            cl->length = fixedEnd - newStart;
            ui.stretchSamples = static_cast<std::int64_t>(std::round((cl->length - ui.origClipLength) * 48000.0));
            ui.isStretching = true;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightStretch)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            cl->length = std::max(0.125, startClipLength + deltaSec);
            ui.stretchSamples = static_cast<std::int64_t>(std::round((cl->length - ui.origClipLength) * 48000.0));
            ui.isStretching = true;
            updateWaveforms(draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::warpMarker && draggedMarkerIdx >= 0)
        {
            if (draggedMarkerIdx < static_cast<int>(ui.warpPoints.size()))
            {
                float leftLimit = 0.01f;
                if (draggedMarkerIdx > 0 && draggedMarkerIdx - 1 < static_cast<int>(ui.warpMarkers.size()))
                {
                    const float prevNorm = ui.warpMarkers[static_cast<size_t>(draggedMarkerIdx - 1)];
                    if (prevNorm >= 0.0f && prevNorm <= 1.0f)
                        leftLimit = prevNorm + 0.015f;
                }

                float rightLimit = 0.99f;
                if (draggedMarkerIdx < static_cast<int>(ui.warpPoints.size()) - 1 && draggedMarkerIdx + 1 < static_cast<int>(ui.warpMarkers.size()))
                {
                    const float nextNorm = ui.warpMarkers[static_cast<size_t>(draggedMarkerIdx + 1)];
                    if (nextNorm >= 0.0f && nextNorm <= 1.0f)
                        rightLimit = nextNorm - 0.015f;
                }

                const float rawNorm = (e.position.x - b.getX()) / b.getWidth();
                const float clampedNorm = juce::jlimit(leftLimit, rightLimit, rawNorm);

                if (ui.isStretching || ui.stretchSamples != 0)
                {
                    ui.warpPoints[static_cast<size_t>(draggedMarkerIdx)].warpedTimeSec =
                        ui.startSourceOffsetSec + static_cast<double>(clampedNorm) * ui.origClipLength;
                }
                else
                {
                    ui.warpPoints[static_cast<size_t>(draggedMarkerIdx)].warpedTimeSec =
                        ui.sourceOffsetSec + static_cast<double>(clampedNorm) * cl->length;
                }

                syncWarpNorms(ui, cl->length, ui.sourceOffsetSec);
                updateWaveforms(draggedClipId);
                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (isResizingLaneHeight)
        {
            isResizingLaneHeight = false;
            return;
        }

        if (isMarqueeActive)
        {
            marqueeSelector.handleMouseUp(e);
            isMarqueeActive = false;
            return;
        }

        if (dragZone == TimelineClipZone::clipTitleMove && !draggedClipId.empty())
        {
            for (const auto& c : data.clips)
            {
                if (c.id == draggedClipId)
                {
                    host.moveClip(data.id, draggedClipId, c.start);
                    break;
                }
            }
        }
        if (dragZone == TimelineClipZone::edgeLeftLoop || dragZone == TimelineClipZone::edgeRightLoop)
        {
            if (loopPreview.active && !draggedClipId.empty())
            {
                if (loopPreview.duplicateCount > 0)
                {
                    const auto created = host.duplicateClipLoop(
                        data.id, draggedClipId, loopPreview.duplicateCount, loopPreview.isRightEdge);
                    duplicateClipLoopUI(draggedClipId, created);
                }
                else
                {
                    for (const auto& c : data.clips)
                    {
                        if (c.id == draggedClipId)
                        {
                            host.resizeClip(data.id, c.id, c.start, c.length);
                            break;
                        }
                    }
                }
            }
            loopPreview = {};
        }
        if (dragZone == TimelineClipZone::edgeLeftTrim || dragZone == TimelineClipZone::edgeRightTrim ||
            dragZone == TimelineClipZone::edgeLeftStretch || dragZone == TimelineClipZone::edgeRightStretch ||
            dragZone == TimelineClipZone::edgeLeftSlip || dragZone == TimelineClipZone::edgeRightSlip)
        {
            if (!draggedClipId.empty())
            {
                for (const auto& c : data.clips)
                {
                    if (c.id == draggedClipId)
                    {
                        host.resizeClip(data.id, c.id, c.start, c.length);
                        break;
                    }
                }
            }
        }
        if (uiStates.contains(draggedClipId))
        {
            uiStates[draggedClipId].isStretching = false;
            uiStates[draggedClipId].isSlipping = false;
        }

        if (dragZone == TimelineClipZone::warpMarker && !draggedClipId.empty() && uiStates.contains(draggedClipId))
        {
            std::sort(uiStates[draggedClipId].warpPoints.begin(), uiStates[draggedClipId].warpPoints.end(),
                      [](const WarpPoint& a, const WarpPoint& b) { return a.warpedTimeSec < b.warpedTimeSec; });
            for (const auto& c : data.clips)
            {
                if (c.id == draggedClipId)
                {
                    syncWarpNorms(uiStates[draggedClipId], c.length, uiStates[draggedClipId].sourceOffsetSec);
                    break;
                }
            }
        }
        if ((dragZone == TimelineClipZone::gainEnvelopePoint || dragZone == TimelineClipZone::gainPointDashedLine) && !draggedClipId.empty() && uiStates.contains(draggedClipId))
        {
            std::sort(uiStates[draggedClipId].gainPoints.begin(), uiStates[draggedClipId].gainPoints.end(),
                      [](const GainPoint& a, const GainPoint& b){ return a.normX < b.normX; });
        }
        if ((dragZone == TimelineClipZone::gainEnvelopePoint
             || dragZone == TimelineClipZone::gainPointDashedLine)
            && !draggedClipId.empty()
            && (draggedGainIdx == 0
                || (uiStates.contains (draggedClipId)
                    && draggedGainIdx == static_cast<int> (
                        uiStates[draggedClipId].gainPoints.size()) - 1)))
        {
            for (auto& c : data.clips)
            {
                if (c.id == draggedClipId)
                {
                    syncClipFadeProjection (c, uiStates[draggedClipId]);
                    host.updateClipFade (
                        data.id, draggedClipId, c.fadeIn, c.fadeOut);
                    break;
                }
            }
        }
        draggedClipId.clear();
        dragZone = TimelineClipZone::none;
        draggedMarkerIdx = -1;
        draggedGainIdx = -1;
        draggedCurveSegmentIdx = -1;
        mouseMove(e);
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::backspaceKey || key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            bool anyChanged = false;
            for (auto& [cid, ui] : uiStates)
            {
                if (ui.gainPoints.size() > 2)
                {
                    auto it = ui.gainPoints.begin() + 1;
                    while (it != ui.gainPoints.end() - 1)
                    {
                        if (it->selected)
                        {
                            it = ui.gainPoints.erase(it);
                            anyChanged = true;
                        }
                        else
                        {
                            ++it;
                        }
                    }
                    if (anyChanged)
                        updateWaveforms(cid);
                }
            }
            if (anyChanged)
            {
                repaint();
                return true;
            }
        }
        if (key.isKeyCode(juce::KeyPress::upKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
        {
            host.adjustVerticalZoom(0.12f);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::downKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
        {
            host.adjustVerticalZoom(-0.12f);
            return true;
        }
        return false;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;

        const auto hit = hitTestZones(e.position);

        // 0. Doppio click sul titolo o header della clip (o intera clip se compressa):
        //    cicla le 3 altezze canoniche condivise (96 -> 256 -> 400 -> 96) tenendo conto dello zoom verticale
        if (hit.clip != nullptr && clip_height_law::isHeaderAreaOrClosedClip(hit.zone, hit.bounds.getHeight(), hit.markerIdx))
        {
            const int modelHeight = clip_height_law::nextModelHeight(static_cast<float>(height), host.getVerticalZoom());
            draggedClipId.clear();
            dragZone = TimelineClipZone::none;
            host.store().resize(data.id, modelHeight);
            return;
        }

        if (e.position.y >= getHeight() - 6.0f)
        {
            const int modelHeight = clip_height_law::nextModelHeight(static_cast<float>(height), host.getVerticalZoom());
            host.store().resize(data.id, modelHeight);
            return;
        }

        if (hit.clip == nullptr) return;

        auto& ui = uiStates[hit.clip->id];

        // Reset Stretch (doppio click su bracket di stretch riporta a lunghezza originale)
        if (hit.zone == TimelineClipZone::edgeLeftStretch || hit.zone == TimelineClipZone::edgeRightStretch)
        {
            if (ui.origClipLength > 0.125)
            {
                ui.stretchSamples = 0;
                ui.isStretching = false;
                host.resizeClip(data.id, hit.clip->id, hit.clip->start, ui.origClipLength);
                updateWaveforms(hit.clip->id);
                repaint();
                return;
            }
        }

        // Reset Slip / Sizing Moves Contents (doppio click su bracket riporta all'offset sample iniziale 4.0s)
        if (hit.zone == TimelineClipZone::edgeLeftSlip || hit.zone == TimelineClipZone::edgeRightSlip)
        {
            ui.sourceOffsetSec = 4.0;
            ui.slipDeltaSamples = 0;
            updateWaveforms(hit.clip->id);
            repaint();
            return;
        }

        // 1. Doppio click su punto di inviluppo Gain: reset a gain 0 dB
        if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.gainIdx >= 0 && hit.gainIdx < static_cast<int>(ui.gainPoints.size()))
        {
            ui.gainPoints[static_cast<size_t>(hit.gainIdx)].gainDb = 0.0f;
            updateWaveforms(hit.clip->id);
            repaint();
            return;
        }

        // 2. Doppio click su triangolino Warp Marker: reset a scostamento originale
        if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 && hit.markerIdx < static_cast<int>(ui.warpPoints.size()))
        {
            ui.warpPoints[static_cast<size_t>(hit.markerIdx)].warpedTimeSec =
                ui.warpPoints[static_cast<size_t>(hit.markerIdx)].sourceTimeSec;
            syncWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
            updateWaveforms(hit.clip->id);
            repaint();
            return;
        }

        // 3. Doppio click sulla linea di gain: aggiungi un nuovo punto di automazione
        if (hit.zone == TimelineClipZone::gainEnvelopeLine)
        {
            const float waveformHeight = juce::jmax(
                10.0f, hit.bounds.getHeight() - clips::audioClipTitleAreaHeight);
            const float normX = juce::jlimit(0.01f, 0.99f, (e.position.x - hit.bounds.getX()) / hit.bounds.getWidth());
            const float normY = juce::jlimit(0.0f, 1.0f, (hit.bounds.getBottom() - e.position.y) / waveformHeight);
            const float clickedDb = clip_gain_law::normToDb(normY);

            for (auto& pt : ui.gainPoints) pt.selected = false;
            GainPoint gp { normX, clickedDb, 0.5f, true };
            ui.gainPoints.push_back(gp);
            std::sort(ui.gainPoints.begin(), ui.gainPoints.end(), [](const GainPoint& a, const GainPoint& b){ return a.normX < b.normX; });
            updateWaveforms(hit.clip->id);
            repaint();
            return;
        }

        // 4. Doppio click su handle di curvatura (tensione) tra i punti: reset al default 0.5f
        if (hit.zone == TimelineClipZone::gainSegmentCurveHandle && hit.curveSegmentIdx >= 0 && hit.curveSegmentIdx < static_cast<int>(ui.gainPoints.size()) - 1)
        {
            clips::resetGainControlPoint (
                ui.gainPoints[static_cast<size_t>(hit.curveSegmentIdx)],
                ui.gainPoints[static_cast<size_t>(hit.curveSegmentIdx + 1)]);
            updateWaveforms(hit.clip->id);
            repaint();
            return;
        }

        if (hit.zone == TimelineClipZone::fadeInCurveHandle)
        {
            ui.fadeInControlX = 0.5f;
            ui.fadeInCurve = 0.5f;
            ui.fadeInControlWeight = -1.0f;
            updateWaveforms (hit.clip->id);
            repaint();
            return;
        }
        if (hit.zone == TimelineClipZone::fadeOutCurveHandle)
        {
            ui.fadeOutControlX = 0.5f;
            ui.fadeOutCurve = 0.5f;
            ui.fadeOutControlWeight = -1.0f;
            updateWaveforms (hit.clip->id);
            repaint();
            return;
        }

        // Altrimenti (doppio click generico sul corpo della clip): apri la clip nell'editor
        const auto id = hit.clip->id, owner = data.id;
        host.openClip(owner, id);
    }

private:
    ClipFeatures clipFeatures;
    LabComponent& host;
    const juce::Colour curveColour;
    TimelineCurveUnderlayStyle curveUnderlayStyle;
    Track data;
    int height = 152;
    bool programAudible = true;
    std::map<std::string, WaveCache> waves;
    std::map<std::string, ClipUIState> uiStates;
    std::map<std::string, std::unique_ptr<AudioClipComponent>> clipComponents;

    ayra::MarqueeSelector marqueeSelector { *this };
    bool isMarqueeActive = false;
    double startClipStart = 0.0;
    double startClipLength = 0.0;
    double startSourceOffsetSec = 0.0;
    float startPtNormX = 0.0f;

    std::string hoveredClipId;
    TimelineClipZone activeZone = TimelineClipZone::none;
    int hoveredMarkerIdx = -1;
    int hoveredGainIdx = -1;
    int hoveredCurveSegmentIdx = -1;
    juce::Point<float> mousePos { -1.0f, -1.0f };

    std::string draggedClipId;
    TimelineClipZone dragZone = TimelineClipZone::none;
    clips::ClipLoopDragState loopPreview;
    int draggedMarkerIdx = -1;
    int draggedGainIdx = -1;
    int draggedCurveSegmentIdx = -1;
    juce::Point<float> dragStartPos;
    std::vector<GainPoint> startGainPoints;
    bool isHoveringDivider = false;
    bool isResizingLaneHeight = false;
    int resizeLaneStartY = 0;
    int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
