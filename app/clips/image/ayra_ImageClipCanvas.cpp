/** @file ayra_ImageClipCanvas.cpp
    Adapter del Lab per una lane di clip Sequenza Immagini (`ImageClipComponent`).
    Contiene orchestrazione Message Thread, hit-test, campionamento warppato dei
    fotogrammi nella filmstrip, editing della curva di Opacita' (0%..100%),
    split con De Casteljau e ciclo dei 3 livelli di zoom sul titolo. */

namespace ayra::lab
{
namespace
{

using clips::ClipFeatures;
using clips::computeClipFeaturesLOD;
using clips::AutomationPoint;
using clips::ImageClipComponent;
using clips::ImageClipVisual;
using clips::ImageFrameVisual;
using clips::OpacityPoint;
using clips::TimelineClipZone;
using clips::CrossfadeInteractionKind;
using clips::defaultImageCrossfadeSolver;
using clips::ImageCrossfadeContext;
using clips::ImageCrossfadeResult;
using clips::ImageCrossfadeSolver;
using clips::makeBoundedCurvePixelSegment;
namespace clip_opacity_law = clips::clip_opacity_law;
namespace clip_height_law = clips::clip_height_law;

struct ImageWarpPoint
{
    double sourceTimeSec = 0.0;
    double warpedTimeSec = 0.0;
};

struct ImageClipUIState
{
    float baseOpacity = 1.0f;
    float frameRate   = 24.0f;
    float aspectRatio = 16.0f / 9.0f;
    int totalSourceFrames = 384;

    std::vector<ImageWarpPoint> warpPoints;
    std::vector<float> warpMarkers;
    std::vector<float> origWarpMarkers;
    std::vector<OpacityPoint> opacityPoints;
    ayra::curves::QuadraticControl fadeInControl {};
    ayra::curves::QuadraticControl fadeOutControl {};

    // Cache visuale dei fotogrammi proiettati (0 allocazioni in paint)
    std::vector<ImageFrameVisual> cachedFrames;

    double origClipLength = 0.0;
    std::int64_t stretchFrames = 0;
    bool isStretching = false;

    double sourceTotalDurationSec = 0.0;
    double sourceOffsetSec = 0.0;
    double defaultSourceOffsetSec = 4.0;
    double startSourceOffsetSec = 0.0;
    std::int64_t slipDeltaFrames = 0;
    bool isSlipping = false;
};

[[nodiscard]] static ayra::curves::CurveBoundarySpec opacityBoundarySpec (
    const ImageClipUIState& ui) noexcept
{
    ayra::curves::CurveBoundarySpec boundary;
    boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
    boundary.fadeInControl = ui.fadeInControl;
    boundary.fadeOutControl = ui.fadeOutControl;
    return boundary;
}

static void syncClipFadeProjection (Clip& clip, const ImageClipUIState& ui) noexcept
{
    if (clip.length <= 0.0 || ui.opacityPoints.size() < 2) { return; }

    const auto fades = clips::projectAutomationFadeDurations (
        ui.opacityPoints.data(), static_cast<int> (ui.opacityPoints.size()),
        clip.length,
        [] (const OpacityPoint& point) noexcept
        {
            return clips::opacityPointAsAutomationPoint (point);
        });
    clip.fadeIn = fades.fadeIn;
    clip.fadeOut = fades.fadeOut;
}

static void syncImageWarpNorms (ImageClipUIState& ui, double clipLength, double sourceOffset)
{
    ui.warpMarkers.clear();
    ui.origWarpMarkers.clear();
    ui.warpMarkers.reserve (ui.warpPoints.size());
    ui.origWarpMarkers.reserve (ui.warpPoints.size());

    const double len = std::max (0.05, clipLength);
    const bool isStretched = (ui.isStretching || ui.stretchFrames != 0) && (ui.origClipLength > 0.05);

    for (const auto& wp : ui.warpPoints)
    {
        float curNorm = 0.0f;
        float origNorm = 0.0f;

        if (isStretched)
        {
            origNorm = static_cast<float> ((wp.sourceTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
            curNorm  = static_cast<float> ((wp.warpedTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
        }
        else
        {
            curNorm  = static_cast<float> ((wp.warpedTimeSec - sourceOffset) / len);
            origNorm = static_cast<float> ((wp.sourceTimeSec - sourceOffset) / len);
        }

        ui.warpMarkers.push_back (curNorm);
        ui.origWarpMarkers.push_back (origNorm);
    }
}

static juce::Array<juce::Image> loadSampleThumbnails()
{
    juce::Array<juce::Image> images;
    juce::Array<juce::File> candidateDirs;
    candidateDirs.add (juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures/images"));
    candidateDirs.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                           .getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory().getChildFile ("fixtures/images"));
    candidateDirs.add (juce::File ("/Users/ayra/Downloads/AyraTimelineLab/fixtures/images"));

    juce::File targetDir;
    for (const auto& dir : candidateDirs)
    {
        if (dir.isDirectory())
        {
            targetDir = dir;
            break;
        }
    }

    if (targetDir.isDirectory())
    {
        for (int i = 1; i <= 8; ++i)
        {
            const auto fileName = "frame_" + juce::String (i).paddedLeft ('0', 2) + ".png";
            const auto imgFile = targetDir.getChildFile (fileName);
            if (imgFile.existsAsFile())
            {
                auto loaded = juce::ImageFileFormat::loadFrom (imgFile);
                if (loaded.isValid())
                    images.add (loaded);
            }
        }
    }
    return images;
}

class ImageClipCanvas final : public juce::Component,
                              public juce::TooltipClient,
                              public clips::ImageClipPresentationProvider
{
public:
    // Crossfade Compensation Solver per ImageClip
    ImageCrossfadeSolver crossfadeSolver;

    void setCrossfadeSolver (ImageCrossfadeSolver solver)
    {
        crossfadeSolver = std::move (solver);
    }

    [[nodiscard]] ImageCrossfadeResult solveImageCrossfade (const ImageCrossfadeContext& ctx) const
    {
        if (crossfadeSolver != nullptr)
            return crossfadeSolver (ctx);
        return defaultImageCrossfadeSolver (ctx);
    }

    [[nodiscard]] const Clip* findClip (const std::string& clipId) const noexcept
    {
        for (const auto& c : data.clips)
            if (c.id == clipId)
                return &c;
        return nullptr;
    }

    struct CrossfadePartnerLookup
    {
        bool found = false;
        std::string partnerClipId;
        double overlapDuration = 0.0;
    };

    [[nodiscard]] CrossfadePartnerLookup findActiveCrossfadeForFadeIn (const std::string& clipId) const
    {
        for (const auto& pair : xfadePairs)
        {
            if (pair.second == clipId)
            {
                const auto* left = findClip (pair.first);
                const auto* right = findClip (pair.second);
                if (left != nullptr && right != nullptr)
                {
                    if (clips::isClipSubInterval (left->start, left->length, right->start, right->length))
                        continue;
                    const double ovStart = std::max (left->start, right->start);
                    const double ovEnd   = std::min (left->start + left->length, right->start + right->length);
                    return { true, pair.first, std::max (0.001, ovEnd - ovStart) };
                }
            }
        }
        return {};
    }

    [[nodiscard]] CrossfadePartnerLookup findActiveCrossfadeForFadeOut (const std::string& clipId) const
    {
        for (const auto& pair : xfadePairs)
        {
            if (pair.first == clipId)
            {
                const auto* left = findClip (pair.first);
                const auto* right = findClip (pair.second);
                if (left != nullptr && right != nullptr)
                {
                    if (clips::isClipSubInterval (left->start, left->length, right->start, right->length))
                        continue;
                    const double ovStart = std::max (left->start, right->start);
                    const double ovEnd   = std::min (left->start + left->length, right->start + right->length);
                    return { true, pair.second, std::max (0.001, ovEnd - ovStart) };
                }
            }
        }
        return {};
    }

    //==============================================================================
    // Crossfade Interaction Dispatchers (SSOT per Drag & Doppio Click)
    //==============================================================================

    void applyCrossfadeCurvature (const std::string& clipId, bool isFadeIn, const ayra::curves::QuadraticControl& control)
    {
        if (!uiStates.contains (clipId)) return;
        auto& ui = uiStates[clipId];

        if (isFadeIn)
        {
            const auto partner = findActiveCrossfadeForFadeIn (clipId);
            if (partner.found && uiStates.contains (partner.partnerClipId))
            {
                auto& leftUi = uiStates[partner.partnerClipId];
                ImageCrossfadeContext ctx;
                ctx.interaction = CrossfadeInteractionKind::curvatureChange;
                ctx.modifiedClipId = clipId;
                ctx.isLeftClipModified = false;
                ctx.leftClipId = partner.partnerClipId;
                ctx.rightClipId = clipId;
                ctx.currentLeftAlpha = leftUi.opacityPoints.back().opacity;
                ctx.currentRightAlpha = ui.opacityPoints.front().opacity;
                ctx.leftFadeOutControl = leftUi.fadeOutControl;
                ctx.rightFadeInControl = control;
                ctx.requestedControl = control;
                ctx.overlapDurationSec = partner.overlapDuration;

                const auto res = solveImageCrossfade (ctx);
                ui.fadeInControl = res.rightFadeInControl;
                leftUi.fadeOutControl = res.leftFadeOutControl;
                updateImageClip (partner.partnerClipId);
            }
            else
            {
                ui.fadeInControl = control;
            }
        }
        else // fadeOut
        {
            const auto partner = findActiveCrossfadeForFadeOut (clipId);
            if (partner.found && uiStates.contains (partner.partnerClipId))
            {
                auto& rightUi = uiStates[partner.partnerClipId];
                ImageCrossfadeContext ctx;
                ctx.interaction = CrossfadeInteractionKind::curvatureChange;
                ctx.modifiedClipId = clipId;
                ctx.isLeftClipModified = true;
                ctx.leftClipId = clipId;
                ctx.rightClipId = partner.partnerClipId;
                ctx.currentLeftAlpha = ui.opacityPoints.back().opacity;
                ctx.currentRightAlpha = rightUi.opacityPoints.front().opacity;
                ctx.leftFadeOutControl = control;
                ctx.rightFadeInControl = rightUi.fadeInControl;
                ctx.requestedControl = control;
                ctx.overlapDurationSec = partner.overlapDuration;

                const auto res = solveImageCrossfade (ctx);
                ui.fadeOutControl = res.leftFadeOutControl;
                rightUi.fadeInControl = res.rightFadeInControl;
                updateImageClip (partner.partnerClipId);
            }
            else
            {
                ui.fadeOutControl = control;
            }
        }

        updateImageClip (clipId);
        layoutClipComponents();
        repaint();
    }

    void applyCrossfadeLevel (const std::string& clipId, bool isFadeIn, float requestedOpacity)
    {
        if (!uiStates.contains (clipId)) return;
        auto& ui = uiStates[clipId];

        if (isFadeIn)
        {
            if (!ui.opacityPoints.empty())
                ui.opacityPoints.front().opacity = requestedOpacity;
            const auto partner = findActiveCrossfadeForFadeIn (clipId);
            if (partner.found && uiStates.contains (partner.partnerClipId))
            {
                auto& leftUi = uiStates[partner.partnerClipId];
                ImageCrossfadeContext ctx;
                ctx.interaction = CrossfadeInteractionKind::levelChange;
                ctx.modifiedClipId = clipId;
                ctx.isLeftClipModified = false;
                ctx.leftClipId = partner.partnerClipId;
                ctx.rightClipId = clipId;
                ctx.currentLeftAlpha = leftUi.opacityPoints.back().opacity;
                ctx.currentRightAlpha = requestedOpacity;
                ctx.requestedAlpha = requestedOpacity;
                ctx.leftFadeOutControl = leftUi.fadeOutControl;
                ctx.rightFadeInControl = ui.fadeInControl;
                ctx.overlapDurationSec = partner.overlapDuration;

                const auto res = solveImageCrossfade (ctx);
                ui.fadeInControl = res.rightFadeInControl;
                leftUi.fadeOutControl = res.leftFadeOutControl;
                updateImageClip (partner.partnerClipId);
            }
        }
        else // fadeOut
        {
            if (!ui.opacityPoints.empty())
                ui.opacityPoints.back().opacity = requestedOpacity;
            const auto partner = findActiveCrossfadeForFadeOut (clipId);
            if (partner.found && uiStates.contains (partner.partnerClipId))
            {
                auto& rightUi = uiStates[partner.partnerClipId];
                ImageCrossfadeContext ctx;
                ctx.interaction = CrossfadeInteractionKind::levelChange;
                ctx.modifiedClipId = clipId;
                ctx.isLeftClipModified = true;
                ctx.leftClipId = clipId;
                ctx.rightClipId = partner.partnerClipId;
                ctx.currentLeftAlpha = requestedOpacity;
                ctx.currentRightAlpha = rightUi.opacityPoints.front().opacity;
                ctx.requestedAlpha = requestedOpacity;
                ctx.leftFadeOutControl = ui.fadeOutControl;
                ctx.rightFadeInControl = rightUi.fadeInControl;
                ctx.overlapDurationSec = partner.overlapDuration;

                const auto res = solveImageCrossfade (ctx);
                ui.fadeOutControl = res.leftFadeOutControl;
                rightUi.fadeInControl = res.rightFadeInControl;
                updateImageClip (partner.partnerClipId);
            }
        }

        updateImageClip (clipId);
        layoutClipComponents();
        repaint();
    }

    explicit ImageClipCanvas (LabComponent& h) : host (h)
    {
        setWantsKeyboardFocus (true);
        marqueeSelector.setFillColour (juce::Colour (0x3019b6e6));
        marqueeSelector.setOutlineColour (juce::Colour (0xe019b6e6));

        marqueeSelector.onPerformMarqueeSelection = [this] (const juce::Rectangle<int>& area,
                                                            ayra::MarqueeSelector::ModifierMode mode)
        {
            const auto pps = getWidth() / 32.0;
            bool anyChanged = false;

            for (const auto& c : data.clips)
            {
                const auto b = clipBounds (c, pps);
                auto& ui = uiStates[c.id];

                constexpr float kTitleAreaHeight = 16.0f;
                const float envHeight = juce::jmax (10.0f, b.getHeight() - kTitleAreaHeight);

                for (int opacityIndex = 0;
                     opacityIndex < static_cast<int> (ui.opacityPoints.size());
                     ++opacityIndex)
                {
                    auto& pt = ui.opacityPoints[static_cast<size_t> (opacityIndex)];
                    const float px = clips::automationPointRenderX (
                        b, opacityIndex,
                        static_cast<int> (ui.opacityPoints.size()), pt.normX);
                    const float py = b.getBottom() - clip_opacity_law::opacityToNorm (pt.opacity) * envHeight;
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
            }

            if (anyChanged)
                repaint();
        };

        marqueeSelector.onPerformClickSelection = [this] (juce::Point<int>,
                                                           ayra::MarqueeSelector::ModifierMode mode)
        {
            if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
            {
                for (auto& [cid, ui] : uiStates)
                    for (auto& pt : ui.opacityPoints)
                        pt.selected = false;
                repaint();
            }
        };
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
                return "Slip Sequenza Immagini (Sizing Moves Contents)\nTrascina per scorrere quali fotogrammi vengono mostrati nella clip";
            case TimelineClipZone::edgeLeftTrim:
            case TimelineClipZone::edgeRightTrim:
                return "Trim Sequenza Immagini (Normal Sizing)\nRiduce o recupera fotogrammi della sequenza fino al limite disponibile";
            case TimelineClipZone::edgeLeftStretch:
            case TimelineClipZone::edgeRightStretch:
                return "Time Stretch Sequenza Immagini\nTrascina per rallentare o accelerare la sequenza di fotogrammi (doppio click per reset)";
            case TimelineClipZone::gainPointDashedLine:
                return "Tratteggio Fade In/Out Opacita'\nTrascina orizzontalmente per impostare la durata del fade visivo senza alterare il picco di opacita'";
            case TimelineClipZone::gainEnvelopePoint:
                return "Punto Inviluppo Opacita'\nPrimo e ultimo sono i fade endpoint permanenti; Alt+Click elimina solo i punti interni";
            case TimelineClipZone::gainSegmentCurveHandle:
                return "Curvatura Inviluppo Opacita'\nTrascina in orizzontale e verticale per modellare la dissolvenza (doppio click per lineare)";
            case TimelineClipZone::fadeInCurveHandle:
            case TimelineClipZone::fadeOutCurveHandle:
                return "Curvatura Fade Opacita'\nTrascina liberamente nel rettangolo del fade (doppio click per lineare)";
            case TimelineClipZone::gainEnvelopeLine:
                return "Linea Inviluppo Opacita'\nDoppio click per inserire un nuovo punto di opacita', trascina in verticale per alzare/abbassare";
            case TimelineClipZone::topWarpRail:
                return "Binario Warp Superiore\nClicca per inserire un nuovo warp marker sui fotogrammi";
            case TimelineClipZone::warpMarker:
                return "Marker di Warp Fotogrammi\nTrascina per comprimere o dilatare il timing dei fotogrammi (doppio click per reset, Alt+Click per eliminare)";
            case TimelineClipZone::bottomCutRail:
                return "Binario di Taglio (Forbici)\nClicca per dividere la sequenza immagini nel punto del cursore";
            case TimelineClipZone::clipTitleMove:
                return "Intestazione Clip Immagini\nTrascina per spostare la clip sulla timeline; Doppio click per ciclare i 3 livelli di zoom";
            default:
                if (isHoveringDivider)
                    return "Separatore Traccia\nTrascina verticalmente per ridimensionare l'altezza (sotto 46px mostra solo i fotogrammi)";
                if (!hoveredClipId.empty())
                    return "Corpo Clip Sequenza Immagini\nTrascina per selezione marquee dei punti di opacita'";
                break;
        }
        return {};
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

    // Calcola il tempo sorgente (secondi) corrispondente a una coordinata normalizzata X [0..1]
    // tenendo conto di Slip, Stretch e Warp Markers (identico a getSourceAudioSec).
    static double getSourceImageSec (const Clip& c, const ImageClipUIState& ui, float normX) noexcept
    {
        const double totalDur = std::max (0.1, ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (c.length + 8.0));
        const double srcOffset = std::clamp (ui.sourceOffsetSec, 0.0, totalDur);
        const bool isStretched = (ui.isStretching || ui.stretchFrames != 0) && (ui.origClipLength > 0.05);

        if (isStretched)
            return srcOffset + static_cast<double> (normX) * ui.origClipLength;

        const double playbackSec = srcOffset + static_cast<double> (normX) * c.length;
        if (ui.warpPoints.empty())
            return playbackSec;

        if (playbackSec <= ui.warpPoints.front().warpedTimeSec)
        {
            const double t0 = 0.0, s0 = 0.0;
            const double t1 = ui.warpPoints.front().warpedTimeSec;
            const double s1 = ui.warpPoints.front().sourceTimeSec;
            if (t1 <= t0 + 0.0001) return s1;
            const double frac = std::clamp ((playbackSec - t0) / (t1 - t0), 0.0, 1.0);
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
                const double frac = std::clamp ((playbackSec - wp0.warpedTimeSec) / span, 0.0, 1.0);
                return wp0.sourceTimeSec + frac * (wp1.sourceTimeSec - wp0.sourceTimeSec);
            }
        }

        const double t0 = ui.warpPoints.back().warpedTimeSec;
        const double s0 = ui.warpPoints.back().sourceTimeSec;
        const double t1 = totalDur, s1 = totalDur;
        if (t1 <= t0 + 0.0001) return s0;
        const double frac = std::clamp ((playbackSec - t0) / (t1 - t0), 0.0, 1.0);
        return s0 + frac * (s1 - s0);
    }

    void rebuildFilmstripCache (const Clip& c, ImageClipUIState& ui)
    {
        ui.cachedFrames.clear();

        const double pps = std::max (1.0, getWidth() / 32.0);
        const auto b = clipBounds (c, pps);
        const float clipW = std::max (8.0f, b.getWidth());
        const float clipH = std::max (12.0f, b.getHeight());

        // Larghezza naturale di ogni miniatura nella filmstrip in base all'aspect ratio
        const float targetTileW = juce::jlimit (28.0f, 180.0f, clipH * ui.aspectRatio * 0.72f);
        const int numTiles = juce::jlimit (1, 64, static_cast<int> (std::ceil (clipW / targetTileW)));
        ui.cachedFrames.reserve (static_cast<size_t> (numTiles));

        for (int i = 0; i < numTiles; ++i)
        {
            const float norm0 = static_cast<float> (i) / static_cast<float> (numTiles);
            const float norm1 = static_cast<float> (i + 1) / static_cast<float> (numTiles);
            const float midNorm = (norm0 + norm1) * 0.5f;

            const double srcSec = getSourceImageSec (c, ui, midNorm);
            const int frameIdx = juce::jlimit (0, std::max (1, ui.totalSourceFrames - 1),
                                               static_cast<int> (std::floor (srcSec * static_cast<double> (ui.frameRate))));

            ImageFrameVisual fv;
            fv.startNormX = norm0;
            fv.endNormX   = norm1;
            fv.sourceFrameIndex = frameIdx;
            if (!sampleThumbnails.isEmpty())
            {
                const int thumbIdx = std::abs (frameIdx) % sampleThumbnails.size();
                fv.thumbnail = sampleThumbnails[thumbIdx];
            }
            ui.cachedFrames.push_back (fv);
        }
    }

    void updateImageClip (const std::string& clipId)
    {
        const bool isOpacityDrag = (dragZone == TimelineClipZone::gainEnvelopePoint
                                 || dragZone == TimelineClipZone::gainPointDashedLine
                                 || dragZone == TimelineClipZone::gainEnvelopeLine);
        syncActiveImageCrossfades (clipId, isOpacityDrag);
        for (auto& c : data.clips)
        {
            if (c.id == clipId && uiStates.contains (clipId))
            {
                auto& ui = uiStates[clipId];
                syncClipFadeProjection (c, ui);
                syncImageWarpNorms (ui, c.length, ui.sourceOffsetSec);
                rebuildFilmstripCache (c, ui);
                break;
            }
        }
        layoutClipComponents();
    }

    void duplicateImageClipLoopUI (const std::string& origId, const std::vector<std::string>& newIds)
    {
        if (!uiStates.contains (origId)) return;
        const auto& orig = uiStates[origId];
        for (const auto& newId : newIds)
        {
            ImageClipUIState dup = orig;
            dup.isStretching = false;
            dup.isSlipping   = false;
            uiStates[newId] = std::move (dup);
        }
    }

    void splitImageClipUI (const std::string& origId, const std::string& newId, float splitFrac)
    {
        if (!uiStates.contains (origId))
            return;

        auto& orig = uiStates[origId];
        ImageClipUIState next = orig;

        const double origLenBefore = orig.origClipLength > 0.0 ? orig.origClipLength : 4.0;
        const double splitSourceTime = orig.sourceOffsetSec + static_cast<double> (splitFrac) * origLenBefore;

        std::vector<ImageWarpPoint> origWps, nextWps;
        for (const auto& wp : orig.warpPoints)
        {
            if (wp.sourceTimeSec < splitSourceTime - 0.005)
                origWps.push_back (wp);
            else if (wp.sourceTimeSec > splitSourceTime + 0.005)
                nextWps.push_back (wp);
        }
        orig.warpPoints = std::move (origWps);
        next.warpPoints = std::move (nextWps);

        std::vector<AutomationPoint> source;
        source.reserve (orig.opacityPoints.size());
        for (const auto& point : orig.opacityPoints)
            source.push_back (clips::opacityPointAsAutomationPoint (point));

        const auto split = clips::splitAutomationPointSequence (
            source, orig.baseOpacity, splitFrac, opacityBoundarySpec (orig));

        orig.opacityPoints.clear();
        orig.opacityPoints.reserve (split.left.size());
        for (const auto& point : split.left)
            orig.opacityPoints.push_back (
                clips::automationPointAsOpacityPoint (point));

        next.opacityPoints.clear();
        next.opacityPoints.reserve (split.right.size());
        for (const auto& point : split.right)
            next.opacityPoints.push_back (
                clips::automationPointAsOpacityPoint (point));

        orig.fadeInControl = split.leftBoundary.fadeInControl;
        orig.fadeOutControl = split.leftBoundary.fadeOutControl;
        next.fadeInControl = split.rightBoundary.fadeInControl;
        next.fadeOutControl = split.rightBoundary.fadeOutControl;

        next.origClipLength = origLenBefore * (1.0 - splitFrac);
        orig.origClipLength = origLenBefore * splitFrac;
        next.sourceTotalDurationSec = orig.sourceTotalDurationSec > 0.0 ? orig.sourceTotalDurationSec : 16.0;
        next.sourceOffsetSec = splitSourceTime;
        next.defaultSourceOffsetSec = splitSourceTime;
        next.startSourceOffsetSec = next.sourceOffsetSec;

        syncImageWarpNorms (orig, orig.origClipLength, orig.sourceOffsetSec);
        syncImageWarpNorms (next, next.origClipLength, next.sourceOffsetSec);

        uiStates[newId] = std::move (next);
    }

    void setModel (const Track& t, int imageLaneHeight)
    {
        data = t;
        height = imageLaneHeight;
        programAudible = audibility (host.store().state(), data).program;

        std::set<std::string> live;
        for (const auto& c : t.clips)
        {
            live.insert (c.id);
            auto component = clipComponents.find (c.id);
            if (component == clipComponents.end())
            {
                auto& provider = static_cast<clips::ImageClipPresentationProvider&> (*this);
                auto imgClip = std::make_unique<ImageClipComponent> (c.id, provider);
                addAndMakeVisible (*imgClip);
                component = clipComponents.emplace (c.id, std::move (imgClip)).first;
            }
            component->second->toFront (false);

            if (!uiStates.contains (c.id))
            {
                ImageClipUIState s;
                s.origClipLength = c.length;
                s.sourceTotalDurationSec = c.length + 8.0;
                s.sourceOffsetSec = 4.0;
                s.defaultSourceOffsetSec = s.sourceOffsetSec;
                s.startSourceOffsetSec = s.sourceOffsetSec;
                s.totalSourceFrames = static_cast<int> (std::round (s.sourceTotalDurationSec * s.frameRate));

                if (!juce::String (c.id).contains ("_cut"))
                {
                    for (float frac : { 0.35f, 0.70f })
                    {
                        const double sampleTime = s.sourceOffsetSec + static_cast<double> (frac) * c.length;
                        s.warpPoints.push_back ({ sampleTime, sampleTime });
                    }
                }
                syncImageWarpNorms (s, c.length, s.sourceOffsetSec);
                const auto fadeEndpoints =
                    ayra::curves::fadeEndpointPositionsFromDurations (
                        c.length, c.fadeIn, c.fadeOut);
                s.opacityPoints = {
                    { static_cast<float> (fadeEndpoints.first), 1.0f, 0.5f, false },
                    { static_cast<float> (fadeEndpoints.last), 1.0f, 0.5f, false }
                };
                rebuildFilmstripCache (c, s);
                uiStates[c.id] = std::move (s);
            }
            else
            {
                auto& state = uiStates[c.id];
                syncImageWarpNorms (state, c.length, state.sourceOffsetSec);
                rebuildFilmstripCache (c, state);
            }
        }

        for (auto i = uiStates.begin(); i != uiStates.end();)
            if (!live.contains (i->first)) i = uiStates.erase (i); else ++i;
        for (auto i = clipComponents.begin(); i != clipComponents.end();)
            if (!live.contains (i->first)) i = clipComponents.erase (i); else ++i;

        layoutClipComponents();
        repaint();
    }

    void setWarpFeatureEnabled (const bool enabled)
    {
        if (clipFeatures.warpEnabled != enabled)
        {
            clipFeatures.warpEnabled = enabled;
            layoutClipComponents();
            repaint();
        }
    }
    [[nodiscard]] bool isWarpFeatureEnabled() const noexcept { return clipFeatures.warpEnabled; }

    void setOpacityFeatureEnabled (const bool enabled)
    {
        if (clipFeatures.gainEnabled != enabled)
        {
            clipFeatures.gainEnabled = enabled;
            layoutClipComponents();
            repaint();
        }
    }
    [[nodiscard]] bool isOpacityFeatureEnabled() const noexcept { return clipFeatures.gainEnabled; }
    void setGainFeatureEnabled (const bool enabled) { setOpacityFeatureEnabled (enabled); }
    [[nodiscard]] bool isGainFeatureEnabled() const noexcept { return isOpacityFeatureEnabled(); }

    [[nodiscard]] const ClipFeatures& getClipFeatures() const noexcept { return clipFeatures; }

    juce::Rectangle<float> clipBounds (const Clip& c, double pps) const
    {
        const float topMargin    = clipFeatures.warpEnabled ? 12.0f : 4.0f;
        const float bottomMargin = clipFeatures.cutEnabled  ? 14.0f : 4.0f;
        const float bodyHeight   = juce::jmax (10.0f, static_cast<float> (height) - (topMargin + bottomMargin));
        return { static_cast<float> (c.start * pps), topMargin,
                 static_cast<float> (c.length * pps), bodyHeight };
    }

    void resized() override
    {
        for (const auto& c : data.clips)
            if (uiStates.contains (c.id))
                rebuildFilmstripCache (c, uiStates[c.id]);
        layoutClipComponents();
    }

    void paint (juce::Graphics& g) override
    {
        const auto pps = getWidth() / 32.0;
        auto& laf = host.look();
        laf.drawTrackGrid (g, getLocalBounds(), pps);

        if (data.clips.empty())
        {
            g.setColour (laf.colours().secondary);
            g.setFont (juce::Font (juce::FontOptions().withHeight (12)));
            g.drawText ("Sequenza Immagini / Video Stream Lane", 25, 35, getWidth() - 40, 20, juce::Justification::centredLeft);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (loopPreview.active && loopPreview.duplicateCount > 0 && !draggedClipId.empty())
        {
            const auto pps = getWidth() / 32.0;
            const Clip* cl = nullptr;
            for (const auto& c : data.clips) if (c.id == draggedClipId) { cl = &c; break; }
            if (cl != nullptr)
            {
                const auto origBounds = clipBounds (*cl, pps);
                const auto tint = juce::Colour (data.colour);
                for (int k = 0; k < loopPreview.duplicateCount; ++k)
                {
                    const auto dupBounds = clips::computeLoopDuplicateBounds (
                        origBounds, loopPreview.isRightEdge, k);
                    clips::drawLoopDuplicatePreview (
                        g, dupBounds, cl->name.empty() ? cl->id : cl->name,
                        k + 1, loopPreview.isRightEdge, tint, nullptr);
                }
            }
        }

        marqueeSelector.drawMarquee (g);
    }

private:

    struct OverlapHitInfo
    {
        bool inOverlap = false;
        bool inCrossfade = false;
        std::string leftClipId;
        std::string rightClipId;
        double startSec = 0.0;
        double endSec = 0.0;
        bool isSubsumed = false;
        std::string partnerClipId;
    };

    [[nodiscard]] std::vector<const Clip*> getClipsSortedByZOrder() const
    {
        std::vector<const Clip*> sorted;
        sorted.reserve (data.clips.size());
        for (const auto& c : data.clips)
            sorted.push_back (&c);
        std::stable_sort (sorted.begin(), sorted.end(),
                          [] (const Clip* a, const Clip* b) {
                              if (std::abs (a->start - b->start) > 1.0e-4)
                                  return a->start < b->start;
                              return a->length > b->length;
                          });
        return sorted;
    }

    [[nodiscard]] OverlapHitInfo findOverlapAtTime (double timeSec, const std::string& refClipId = {}) const
    {
        const auto sorted = getClipsSortedByZOrder();
        for (size_t i = 0; i + 1 < sorted.size(); ++i)
        {
            for (size_t j = i + 1; j < sorted.size(); ++j)
            {
                const auto* cA = sorted[i];
                const auto* cB = sorted[j];
                const auto* left = (cA->start <= cB->start) ? cA : cB;
                const auto* right = (cA->start <= cB->start) ? cB : cA;
                const double ovStart = std::max (left->start, right->start);
                const double ovEnd   = std::min (left->start + left->length, right->start + right->length);
                if (ovEnd > ovStart + 1.0e-3 && timeSec >= ovStart - 1.0e-3 && timeSec <= ovEnd + 1.0e-3)
                {
                    if (!refClipId.empty() && left->id != refClipId && right->id != refClipId)
                        continue;
                    const bool isSubsumed = clips::isClipSubInterval (left->start, left->length, right->start, right->length);
                    const bool inXfade = (!isSubsumed) && xfadePairs.contains ({ left->id, right->id });
                    const std::string partner = (left->id == refClipId) ? right->id : left->id;
                    return { true, inXfade, left->id, right->id, ovStart, ovEnd, isSubsumed, partner };
                }
            }
        }
        return {};
    }

    [[nodiscard]] OverlapHitInfo findOverlapForTopClip (const std::string& topClipId) const
    {
        const auto sorted = getClipsSortedByZOrder();
        for (size_t i = 0; i + 1 < sorted.size(); ++i)
        {
            for (size_t j = i + 1; j < sorted.size(); ++j)
            {
                const auto* cA = sorted[i];
                const auto* cB = sorted[j];
                if (cB->id != topClipId)
                    continue;
                const auto* left = (cA->start <= cB->start) ? cA : cB;
                const auto* right = (cA->start <= cB->start) ? cB : cA;
                const double ovStart = std::max (left->start, right->start);
                const double ovEnd   = std::min (left->start + left->length, right->start + right->length);
                if (ovEnd > ovStart + 1.0e-3)
                {
                    const bool isSubsumed = clips::isClipSubInterval (left->start, left->length, right->start, right->length);
                    const bool inXfade = (!isSubsumed) && xfadePairs.contains ({ left->id, right->id });
                    return { true, inXfade, left->id, right->id, ovStart, ovEnd, isSubsumed, cA->id };
                }
            }
        }
        return {};
    }

    void syncActiveImageCrossfades (const std::string& modifiedClipId = {}, bool syncAlphaValue = false)
    {
        const auto sorted = getClipsSortedByZOrder();
        std::set<std::pair<std::string, std::string>> validPairs;

        for (size_t i = 0; i + 1 < sorted.size(); ++i)
        {
            for (size_t j = i + 1; j < sorted.size(); ++j)
            {
                const auto* cA = sorted[i];
                const auto* cB = sorted[j];
                const auto* left = (cA->start <= cB->start) ? cA : cB;
                const auto* right = (cA->start <= cB->start) ? cB : cA;
                const double ovStart = std::max (left->start, right->start);
                const double ovEnd   = std::min (left->start + left->length, right->start + right->length);
                const std::pair<std::string, std::string> key { left->id, right->id };

                const bool isSubsumed = clips::isClipSubInterval (left->start, left->length, right->start, right->length);

                if (ovEnd > ovStart + 0.01 && (!isSubsumed) && xfadePairs.contains (key))
                {
                    validPairs.insert (key);
                    if (!uiStates.contains (left->id) || !uiStates.contains (right->id))
                        continue;

                    auto& leftUi  = uiStates[left->id];
                    auto& rightUi = uiStates[right->id];
                    if (leftUi.opacityPoints.size() < 2 || rightUi.opacityPoints.size() < 2)
                        continue;

                    const float nominalLeftFadeIn = left->length > 0.001
                        ? static_cast<float> (left->fadeIn / left->length) : 0.0f;
                    const float targetLeftFadeOutNormX = static_cast<float> (
                        (ovStart - left->start) / std::max (0.05, left->length));
                    clips::applySmartShrinkPushLeft (leftUi.opacityPoints, targetLeftFadeOutNormX, nominalLeftFadeIn);

                    const float nominalRightFadeOut = right->length > 0.001
                        ? static_cast<float> (1.0 - right->fadeOut / right->length) : 1.0f;
                    const float targetRightFadeInNormX = static_cast<float> (
                        (ovEnd - right->start) / std::max (0.05, right->length));
                    clips::applySmartShrinkPushRight (rightUi.opacityPoints, targetRightFadeInNormX, nominalRightFadeOut);

                    ImageCrossfadeContext ctx;
                    ctx.interaction = syncAlphaValue ? CrossfadeInteractionKind::levelChange : CrossfadeInteractionKind::boundaryResize;
                    ctx.modifiedClipId = modifiedClipId;
                    ctx.isLeftClipModified = (modifiedClipId == left->id);
                    ctx.leftClipId = left->id;
                    ctx.rightClipId = right->id;
                    ctx.currentLeftAlpha = leftUi.opacityPoints.back().opacity;
                    ctx.currentRightAlpha = rightUi.opacityPoints.front().opacity;
                    ctx.requestedAlpha = (modifiedClipId == left->id) ? leftUi.opacityPoints.back().opacity : rightUi.opacityPoints.front().opacity;
                    ctx.leftFadeOutControl = leftUi.fadeOutControl;
                    ctx.rightFadeInControl = rightUi.fadeInControl;
                    ctx.overlapDurationSec = ovEnd - ovStart;

                    const auto res = solveImageCrossfade (ctx);
                    leftUi.fadeOutControl = res.leftFadeOutControl;
                    rightUi.fadeInControl = res.rightFadeInControl;
                }
            }
        }

        for (const auto& oldPair : xfadePairs)
        {
            if (!validPairs.contains (oldPair))
            {
                const auto* cLeft = findClip (oldPair.first);
                const auto* cRight = findClip (oldPair.second);
                if (cLeft != nullptr && uiStates.contains (oldPair.first) && !uiStates[oldPair.first].opacityPoints.empty())
                {
                    auto& pts = uiStates[oldPair.first].opacityPoints;
                    const float nominalFadeIn = cLeft->length > 0.001 ? static_cast<float> (cLeft->fadeIn / cLeft->length) : 0.0f;
                    const float nominalFadeOut = cLeft->length > 0.001 ? static_cast<float> (1.0 - cLeft->fadeOut / cLeft->length) : 1.0f;
                    pts.front().normX = nominalFadeIn;
                    pts.back().normX  = nominalFadeOut;
                }
                if (cRight != nullptr && uiStates.contains (oldPair.second) && !uiStates[oldPair.second].opacityPoints.empty())
                {
                    auto& pts = uiStates[oldPair.second].opacityPoints;
                    const float nominalFadeIn = cRight->length > 0.001 ? static_cast<float> (cRight->fadeIn / cRight->length) : 0.0f;
                    const float nominalFadeOut = cRight->length > 0.001 ? static_cast<float> (1.0 - cRight->fadeOut / cRight->length) : 1.0f;
                    pts.front().normX = nominalFadeIn;
                    pts.back().normX  = nominalFadeOut;
                }
            }
        }
        xfadePairs = std::move (validPairs);
    }

    [[nodiscard]] ImageClipVisual makeImageClipVisual (
        const std::string& clipId,
        const juce::Rectangle<float> localBodyBounds,
        const juce::Point<float> parentOrigin) override
    {
        ImageClipVisual visual;
        visual.bounds = localBodyBounds;

        const auto clip = std::find_if (data.clips.begin(), data.clips.end(),
                                        [&clipId] (const Clip& candidate) { return candidate.id == clipId; });
        const auto state = uiStates.find (clipId);
        if (clip == data.clips.end() || state == uiStates.end())
            return visual;

        auto& ui = state->second;
        syncImageWarpNorms (ui, clip->length, ui.sourceOffsetSec);

        visual.title              = juce::String::fromUTF8 (clip->name.c_str());
        visual.tint               = juce::Colour (data.colour != 0 ? data.colour : 0xff19b6e6);
        visual.selected           = (clipId == host.store().state().selectedClip);
        visual.suspended          = !programAudible;
        visual.frameRate          = ui.frameRate;
        visual.totalSourceFrames  = ui.totalSourceFrames;
        visual.frameAspectRatio   = ui.aspectRatio;
        visual.visualSeed         = clip->seed;

        const int visibleFramesCount = static_cast<int> (std::round (clip->length * ui.frameRate));
        visual.sequenceFormatText = juce::String (juce::roundToInt (ui.frameRate)) + " fps / "
                                  + juce::String (visibleFramesCount) + " frm";

        visual.frames             = ui.cachedFrames.data();
        visual.numFrames          = static_cast<int> (ui.cachedFrames.size());

        visual.baseOpacity        = ui.baseOpacity;
        visual.fadeInControl      = ui.fadeInControl;
        visual.fadeOutControl     = ui.fadeOutControl;
        visual.opacityPoints      = ui.opacityPoints.data();
        visual.numOpacityPoints   = static_cast<int> (ui.opacityPoints.size());

        visual.warpMarkersNorm     = ui.warpMarkers.data();
        visual.origWarpMarkersNorm = ui.origWarpMarkers.data();
        visual.numWarpMarkers      = static_cast<int> (ui.warpMarkers.size());
        visual.clipLengthSec       = clip->length;

        visual.stretchDeltaFrames = ui.stretchFrames;
        visual.isStretchActive    = ui.isStretching;
        visual.slipDeltaFrames    = ui.slipDeltaFrames;
        visual.isSlipActive       = ui.isSlipping;

        const bool isDraggedClip = (draggedClipId == clipId);
        const bool isDraggingWarp = isDraggedClip
                                 && (dragZone == TimelineClipZone::warpMarker
                                     || dragZone == TimelineClipZone::topWarpRail);
        const bool isDraggingOpacity = isDraggedClip
                                    && (dragZone == TimelineClipZone::gainEnvelopePoint
                                        || dragZone == TimelineClipZone::gainEnvelopeLine
                                        || dragZone == TimelineClipZone::gainSegmentCurveHandle
                                        || dragZone == TimelineClipZone::fadeInCurveHandle
                                        || dragZone == TimelineClipZone::fadeOutCurveHandle
                                        || dragZone == TimelineClipZone::gainPointDashedLine);

        // LOD identico all'Audio: sotto 46px mostra solo i fotogrammi; medio/grande mostra anche la linea di opacita'
        visual.features = computeClipFeaturesLOD (clipFeatures, localBodyBounds.getHeight(), isDraggingWarp, isDraggingOpacity);

        if (hoveredClipId == clipId || (isDraggedClip && dragZone != TimelineClipZone::none))
        {
            const float localMouseX = mousePos.x - parentOrigin.x;
            visual.hoveredZone = isDraggedClip ? dragZone : activeZone;
            visual.hoveredMarkerIdx = isDraggedClip ? draggedMarkerIdx : hoveredMarkerIdx;
            visual.hoveredOpacityPointIdx = isDraggedClip ? draggedOpacityIdx : hoveredOpacityIdx;
            visual.hoveredCurveSegmentIdx = isDraggedClip ? draggedCurveSegmentIdx : hoveredCurveSegmentIdx;
            visual.hoverMouseX            = localMouseX;
            visual.showWarpAddGhost       = visual.features.showWarpRail
                                         && activeZone == TimelineClipZone::topWarpRail
                                         && hoveredMarkerIdx < 0;
            visual.warpAddGhostX          = localMouseX;
            visual.isDraggingAllOpacity   = false;
        }

        const auto ovInfo = findOverlapForTopClip (clipId);
        if (ovInfo.inOverlap && clip->length > 0.001)
        {
            visual.overlap.hasOverlap  = true;
            visual.overlap.isCrossfade = ovInfo.inCrossfade;
            visual.overlap.isTopClip   = true;
            visual.overlap.startNormX  = juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.startSec - clip->start) / clip->length));
            visual.overlap.endNormX    = juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.endSec - clip->start) / clip->length));
            visual.overlap.partnerTint = visual.tint;

            const std::string partnerId = ovInfo.partnerClipId.empty()
                ? ((ovInfo.leftClipId == clipId) ? ovInfo.rightClipId : ovInfo.leftClipId)
                : ovInfo.partnerClipId;
            const auto* partnerClip = findClip (partnerId);
            if (partnerClip != nullptr && uiStates.contains (partnerId) && partnerClip->length > 0.001)
            {
                const auto& partnerUi = uiStates.at (partnerId);
                visual.overlap.showPartnerCurve = true;
                if (ovInfo.inCrossfade && !partnerUi.opacityPoints.empty())
                {
                    visual.overlap.partnerStartNormY    = clip_opacity_law::opacityToNorm (partnerUi.opacityPoints.back().opacity);
                    visual.overlap.partnerEndNormY      = 0.0f;
                    visual.overlap.partnerCurveControl  = partnerUi.fadeOutControl;
                    visual.overlap.partnerPointSelected = partnerUi.opacityPoints.back().selected;
                }
                else
                {
                    const float leftNorm0 = juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.startSec - partnerClip->start) / partnerClip->length));
                    const float leftNorm1 = juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.endSec - partnerClip->start) / partnerClip->length));
                    visual.overlap.partnerStartNormY = clip_opacity_law::evaluateOpacityAt (
                        partnerUi.opacityPoints.data(), static_cast<int> (partnerUi.opacityPoints.size()), partnerUi.baseOpacity, leftNorm0, partnerUi.fadeInControl, partnerUi.fadeOutControl);
                    visual.overlap.partnerEndNormY   = clip_opacity_law::evaluateOpacityAt (
                        partnerUi.opacityPoints.data(), static_cast<int> (partnerUi.opacityPoints.size()), partnerUi.baseOpacity, leftNorm1, partnerUi.fadeInControl, partnerUi.fadeOutControl);
                }
            }
        }

        return visual;
    }

    void layoutClipComponents()
    {
        if (getWidth() <= 0)
            return;

        const auto pixelsPerSecond = getWidth() / 32.0;
        const float topOverflow    = clipFeatures.warpEnabled ? 8.0f : 0.0f;
        const float bottomOverflow = clipFeatures.cutEnabled  ? 6.0f : 0.0f;

        syncActiveImageCrossfades (draggedClipId, false);
        for (const auto* clipPtr : getClipsSortedByZOrder())
        {
            const auto& clip = *clipPtr;
            if (const auto component = clipComponents.find (clip.id);
                component != clipComponents.end())
            {
                component->second->toFront (false);
                component->second->setDecorationOverflows (topOverflow, bottomOverflow, 7.0f);
                component->second->setBodyBoundsInParent (clipBounds (clip, pixelsPerSecond));
            }
        }
    }

    struct HitResult
    {
        const Clip* clip = nullptr;
        TimelineClipZone zone = TimelineClipZone::none;
        int markerIdx = -1;
        int opacityIdx = -1;
        int curveSegmentIdx = -1;
        juce::Rectangle<float> bounds;
    };

    HitResult hitTestZones (juce::Point<float> p) const
    {
        HitResult res;
        const auto pps = getWidth() / 32.0;
        const bool isDraggingWarp = (activeZone == TimelineClipZone::warpMarker || activeZone == TimelineClipZone::topWarpRail);
        const bool isDraggingOpacity = (activeZone == TimelineClipZone::gainEnvelopePoint ||
                                        activeZone == TimelineClipZone::gainEnvelopeLine ||
                                        activeZone == TimelineClipZone::gainSegmentCurveHandle ||
                                        activeZone == TimelineClipZone::fadeInCurveHandle ||
                                        activeZone == TimelineClipZone::fadeOutCurveHandle ||
                                        activeZone == TimelineClipZone::gainPointDashedLine);

        const auto sortedClips = getClipsSortedByZOrder();
        for (auto it = sortedClips.rbegin(); it != sortedClips.rend(); ++it)
        {
            const auto& c = **it;
            const auto b = clipBounds (c, pps);
            const auto feat = computeClipFeaturesLOD (clipFeatures, b.getHeight(), isDraggingWarp, isDraggingOpacity);

            if (p.x >= b.getX() - 6.0f && p.x <= b.getRight() + 6.0f)
            {
                // A) Barra inferiore di taglio
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
                        const auto& ui = uiStates.at (c.id);
                        for (int m = 0; m < static_cast<int> (ui.warpMarkers.size()); ++m)
                        {
                            const float curNorm = ui.warpMarkers[static_cast<size_t> (m)];
                            if (curNorm < -0.001f || curNorm > 1.001f) continue;
                            const float wx = b.getX() + curNorm * b.getWidth();
                            if (std::abs (p.x - wx) <= 8.0f)
                            {
                                res.zone = TimelineClipZone::warpMarker;
                                res.markerIdx = m;
                                break;
                            }
                        }
                    }
                    return res;
                }

                // C) Corpo della Clip Immagini
                if (p.y >= b.getY() && p.y < b.getBottom())
                {
                    res.clip = &c;
                    res.bounds = b;
                    const auto& ui = uiStates.at (c.id);

                    constexpr float kTitleAreaHeight = 16.0f;
                    const float envHeight = juce::jmax (10.0f, b.getHeight() - kTitleAreaHeight);
                    const juce::Rectangle<float> curveBounds (
                        b.getX(), b.getBottom() - envHeight, b.getWidth(), envHeight);
                    auto opacityToY = [&] (float op) -> float
                    {
                        return b.getBottom() - clip_opacity_law::opacityToNorm (op) * envHeight;
                    };

                    // 1. Bracket laterali di Loop (0..25%), Slip (25..50%), Trim (50..75%), Stretch (75..100%)
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

                    // 2. Zona Titolo in cima (sui fotogrammi)
                    if (p.y <= b.getY() + kTitleAreaHeight)
                    {
                        res.zone = TimelineClipZone::clipTitleMove;
                        return res;
                    }

                    // 3. Interazioni Linea Opacita' (visibili a zoom medio e grande)
                    if (feat.showGainEnvelope)
                    {
                        const auto boundaryGeometry =
                            clips::computeProjectedAutomationBoundaryGeometry (
                                curveBounds, ui.opacityPoints.data(),
                                static_cast<int> (ui.opacityPoints.size()),
                                opacityBoundarySpec (ui),
                                [] (const OpacityPoint& point) noexcept
                                {
                                    return clips::opacityPointAsAutomationPoint (point);
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

                        // Rilevamento della curva partner visibile nell'area di overlap della top clip
                        const auto ovInfo = findOverlapForTopClip (c.id);
                        if (ovInfo.inCrossfade && !ovInfo.leftClipId.empty())
                        {
                            const auto leftClipIt = std::find_if (data.clips.begin(), data.clips.end(),
                                                                  [&] (const Clip& clp) { return clp.id == ovInfo.leftClipId; });
                            const auto leftUiIt = uiStates.find (ovInfo.leftClipId);
                            if (leftClipIt != data.clips.end() && leftUiIt != uiStates.end() && !leftUiIt->second.opacityPoints.empty())
                            {
                                const float cx0 = curveBounds.getX() + juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.startSec - c.start) / c.length)) * curveBounds.getWidth();
                                const float cx1 = curveBounds.getX() + juce::jlimit (0.0f, 1.0f, static_cast<float> ((ovInfo.endSec - c.start) / c.length))   * curveBounds.getWidth();
                                const float py0 = curveBounds.getBottom() - juce::jlimit (0.0f, 1.0f, leftUiIt->second.opacityPoints.back().opacity) * curveBounds.getHeight();
                                const float py1 = curveBounds.getBottom();
                                const auto partnerSeg = makeBoundedCurvePixelSegment (
                                    { cx0, py0 }, { cx1, py1 }, leftUiIt->second.fadeOutControl);

                                if (partnerSeg.control.getDistanceFrom (p) <= 8.5f)
                                {
                                    res.clip = &(*leftClipIt);
                                    res.zone = TimelineClipZone::fadeOutCurveHandle;
                                    return res;
                                }
                            }
                        }

                        if (ui.opacityPoints.size() >= 2)
                        {
                            for (size_t s = 0; s + 1 < ui.opacityPoints.size(); ++s)
                            {
                                const auto& n0 = ui.opacityPoints[s];
                                const auto& n1 = ui.opacityPoints[s + 1];
                                const AutomationPoint start {
                                    n0.normX, n0.opacity, n0.tension, n0.selected,
                                    n0.controlX, n0.controlY, n0.controlWeight };
                                const AutomationPoint end {
                                    n1.normX, n1.opacity, n1.tension, n1.selected,
                                    n1.controlX, n1.controlY, n1.controlWeight };
                                const auto handle = clips::automationControlPixelPosition (
                                    curveBounds, start, end, static_cast<int> (s),
                                    static_cast<int> (ui.opacityPoints.size()));

                                if (handle.getDistanceFrom (p) <= 8.5f)
                                {
                                    res.zone = TimelineClipZone::gainSegmentCurveHandle;
                                    res.curveSegmentIdx = static_cast<int> (s);
                                    return res;
                                }
                            }
                        }

                        for (int op = 0; op < static_cast<int> (ui.opacityPoints.size()); ++op)
                        {
                            const auto& pt = ui.opacityPoints[static_cast<size_t> (op)];
                            const AutomationPoint automationPoint {
                                pt.normX, pt.opacity, pt.tension, pt.selected,
                                pt.controlX, pt.controlY, pt.controlWeight };
                            const auto point = clips::automationPointPixelPosition (
                                curveBounds, automationPoint, op,
                                static_cast<int> (ui.opacityPoints.size()));

                            if (point.getDistanceFrom (p) <= 8.5f)
                            {
                                res.zone = TimelineClipZone::gainEnvelopePoint;
                                res.opacityIdx = op;
                                return res;
                            }
                        }

                        for (int op = 0; op < static_cast<int> (ui.opacityPoints.size()); ++op)
                        {
                            const auto& pt = ui.opacityPoints[static_cast<size_t> (op)];
                            const float px = clips::automationPointRenderX (
                                curveBounds, op,
                                static_cast<int> (ui.opacityPoints.size()), pt.normX);
                            const float py = opacityToY (pt.opacity);

                            if (p.y >= py + 6.0f && p.y <= b.getBottom() - 4.0f)
                            {
                                bool hitDashed = false;
                                if (op == 0 && pt.normX <= 0.01f)
                                    hitDashed = (p.x > b.getX() + 2.5f && p.x <= b.getX() + 16.0f);
                                else if (op == static_cast<int> (ui.opacityPoints.size()) - 1 && pt.normX >= 0.99f)
                                    hitDashed = (p.x >= b.getRight() - 16.0f && p.x < b.getRight() - 2.5f);
                                else
                                    hitDashed = (std::abs (p.x - px) <= 7.0f);

                                if (hitDashed)
                                {
                                    res.zone = TimelineClipZone::gainPointDashedLine;
                                    res.opacityIdx = op;
                                    return res;
                                }
                            }
                        }

                        if (ui.opacityPoints.size() >= 2)
                        {
                            for (size_t s = 0; s + 1 < ui.opacityPoints.size(); ++s)
                            {
                                const auto& n0 = ui.opacityPoints[s];
                                const auto& n1 = ui.opacityPoints[s + 1];
                                const float x0 = clips::automationPointRenderX (
                                    curveBounds, static_cast<int> (s),
                                    static_cast<int> (ui.opacityPoints.size()), n0.normX);
                                const float x1 = clips::automationPointRenderX (
                                    curveBounds, static_cast<int> (s + 1),
                                    static_cast<int> (ui.opacityPoints.size()), n1.normX);
                                if (p.x >= x0 && p.x <= x1)
                                {
                                    const float normX = juce::jlimit (
                                        n0.normX, n1.normX,
                                        (p.x - b.getX()) / b.getWidth());
                                    const float expectedY = b.getBottom()
                                        - clip_opacity_law::evaluateOpacitySegmentUnclamped (
                                            n0, n1, normX) * envHeight;

                                    if (std::abs (p.y - expectedY) <= 8.0f)
                                    {
                                        res.zone = TimelineClipZone::gainEnvelopeLine;
                                        res.curveSegmentIdx = static_cast<int> (s);
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

    void mouseMove (const juce::MouseEvent& e) override
    {
        mousePos = e.position;
        const auto hit = hitTestZones (e.position);

        if (hit.clip != nullptr)
        {
            hoveredClipId          = hit.clip->id;
            activeZone             = hit.zone;
            hoveredMarkerIdx       = hit.markerIdx;
            hoveredOpacityIdx      = hit.opacityIdx;
            hoveredCurveSegmentIdx = hit.curveSegmentIdx;

            const auto& ui = uiStates.at (hit.clip->id);
            const bool canErase = clips::isClipDeletionZone (activeZone)
                || activeZone == TimelineClipZone::warpMarker
                || (activeZone == TimelineClipZone::gainEnvelopePoint
                    && clips::canAutomationPointBeRemoved (
                        hit.opacityIdx, static_cast<int> (ui.opacityPoints.size())));
            setMouseCursor (clips::timelineMouseCursor (
                clips::resolveTimelineCursorRole ({ activeZone,
                    e.mods.isAltDown(), e.mods.isShiftDown(), true, canErase })));
        }
        else
        {
            hoveredClipId.clear();
            activeZone = TimelineClipZone::none;
            hoveredMarkerIdx = -1;
            hoveredOpacityIdx = -1;
            hoveredCurveSegmentIdx = -1;
            isHoveringDivider = (e.position.y >= getHeight() - 6.0f);
            setMouseCursor (isHoveringDivider ? juce::MouseCursor::UpDownResizeCursor
                                              : juce::MouseCursor::NormalCursor);
        }
        repaint();
    }

    void modifierKeysChanged (const juce::ModifierKeys& modifiers) override
    {
        const auto active = (dragZone != TimelineClipZone::none) ? dragZone : activeZone;
        const auto cid = (dragZone != TimelineClipZone::none) ? draggedClipId : hoveredClipId;
        const int opIdx = (dragZone != TimelineClipZone::none) ? draggedOpacityIdx : hoveredOpacityIdx;

        bool canErase = clips::isClipDeletionZone (active) || active == TimelineClipZone::warpMarker;
        if (uiStates.contains (cid))
            canErase = canErase || (active == TimelineClipZone::gainEnvelopePoint
                && clips::canAutomationPointBeRemoved (opIdx,
                    static_cast<int> (uiStates.at (cid).opacityPoints.size())));
        setMouseCursor (clips::timelineMouseCursor (
            clips::resolveTimelineCursorRole ({ active, modifiers.isAltDown(),
                modifiers.isShiftDown(), true, canErase })));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredClipId.clear();
        activeZone = TimelineClipZone::none;
        hoveredMarkerIdx = -1;
        hoveredOpacityIdx = -1;
        hoveredCurveSegmentIdx = -1;
        isHoveringDivider = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto hit = hitTestZones (e.position);

        if (hit.clip != nullptr && e.mods.isPopupMenu())
        {
            auto context = clips::makeTimelinePointerContext (
                data.id, hit.clip->id, clips::TimelinePointerTarget::clip,
                clips::ClipKind::imageSequence,
                clips::semanticClipZone (hit.zone), e.position,
                clips::timelineSecondsFromCanvasX (
                    e.position.x, getWidth()), e.mods);
            context.markerIndex = hit.markerIdx;
            context.pointIndex = hit.opacityIdx;
            context.segmentIndex = hit.curveSegmentIdx;
            const auto ovInfo = findOverlapAtTime (context.timelineSeconds, hit.clip->id);
            if (ovInfo.inOverlap)
            {
                context.isInClipOverlap = true;
                context.isInCrossfade = ovInfo.inCrossfade;
                context.overlappedPartnerClipId = (ovInfo.leftClipId == hit.clip->id) ? ovInfo.rightClipId : ovInfo.leftClipId;
                context.overlapStartSeconds = ovInfo.startSec;
                context.overlapEndSeconds = ovInfo.endSec;
            }
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
            if (e.mods.isLeftButtonDown() && e.position.y >= getHeight() - 6.0f)
            {
                isResizingLaneHeight = true;
                resizeLaneStartY = e.getScreenY();
                resizeStartLaneHeight = height;
                return;
            }
            isMarqueeActive = marqueeSelector.handleMouseDown (e);
            return;
        }

        host.selectClip (hit.clip->id);
        grabKeyboardFocus();

        draggedClipId          = hit.clip->id;
        dragZone               = hit.zone;
        draggedMarkerIdx       = hit.markerIdx;
        draggedOpacityIdx      = hit.opacityIdx;
        draggedCurveSegmentIdx = hit.curveSegmentIdx;
        dragStartPos           = e.position;

        auto& ui = uiStates[draggedClipId];

        // Snapshot per rollback con tasto Esc
        hasDragSnapshot = true;
        snapshotUIState = ui;
        startClipStart  = hit.clip->start;
        startClipLength = hit.clip->length;

        // Alt + Click su Warp Marker: Elimina
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && hit.zone == TimelineClipZone::warpMarker)
        {
            if (hit.markerIdx >= 0 && hit.markerIdx < static_cast<int> (ui.warpPoints.size()))
            {
                ui.warpPoints.erase (ui.warpPoints.begin() + hit.markerIdx);
                updateImageClip (draggedClipId);
                resetDragState();
                repaint();
                return;
            }
        }

        // Alt + Click su punto di Opacita' intermedio: Elimina
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && hit.zone == TimelineClipZone::gainEnvelopePoint)
        {
            if (clips::canAutomationPointBeRemoved (
                    hit.opacityIdx, static_cast<int> (ui.opacityPoints.size())))
            {
                ui.opacityPoints.erase (ui.opacityPoints.begin() + hit.opacityIdx);
                updateImageClip (draggedClipId);
            }
            resetDragState();
            repaint();
            return;
        }

        if (!e.mods.isLeftButtonDown())
        {
            resetDragState();
            return;
        }

        if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.opacityIdx >= 0 && hit.opacityIdx < static_cast<int> (ui.opacityPoints.size()))
        {
            if (e.mods.isShiftDown())
                ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].selected = true;
            else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].selected = !ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].selected;
            else if (!ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].selected)
            {
                for (auto& pt : ui.opacityPoints) pt.selected = false;
                ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].selected = true;
            }
        }

        if (hit.zone == TimelineClipZone::clipTitleMove)
            return;

        if (hit.zone == TimelineClipZone::gainPointDashedLine && hit.opacityIdx >= 0)
        {
            startOpacityPoints = ui.opacityPoints;
            return;
        }

        // Click su Top Warp Rail: inserisce un nuovo Warp Marker sui fotogrammi
        if (dragZone == TimelineClipZone::topWarpRail && hit.markerIdx < 0)
        {
            const float normX = juce::jlimit (0.01f, 0.99f, (e.position.x - hit.bounds.getX()) / hit.bounds.getWidth());
            const double sampleTime = (ui.isStretching || ui.stretchFrames != 0)
                                    ? (ui.startSourceOffsetSec + static_cast<double> (normX) * ui.origClipLength)
                                    : (ui.sourceOffsetSec + static_cast<double> (normX) * hit.clip->length);

            const double minDistance = std::max (0.005, hit.clip->length * 0.015);
            const auto dup = std::find_if (ui.warpPoints.begin(), ui.warpPoints.end(),
                                           [sampleTime, minDistance] (const ImageWarpPoint& m)
                                           {
                                               return std::abs (m.sourceTimeSec - sampleTime) < minDistance;
                                           });
            if (dup == ui.warpPoints.end())
            {
                ui.warpPoints.push_back ({ sampleTime, sampleTime });
                std::sort (ui.warpPoints.begin(), ui.warpPoints.end(),
                           [] (const ImageWarpPoint& a, const ImageWarpPoint& b) { return a.sourceTimeSec < b.sourceTimeSec; });
                updateImageClip (draggedClipId);
            }
            resetDragState();
            repaint();
            return;
        }

        // Click su Bottom Scissor Cut Rail: divide la clip immagini
        if (dragZone == TimelineClipZone::bottomCutRail)
        {
            const auto pps = getWidth() / 32.0;
            const double splitTime = e.position.x / pps;
            constexpr double minimumSegmentLength = 0.2;
            std::vector<std::pair<std::string, float>> clipsToSplit;
            for (const auto& candidate : data.clips)
            {
                const double rel = splitTime - candidate.start;
                if (rel > minimumSegmentLength && rel < candidate.length - minimumSegmentLength)
                    clipsToSplit.emplace_back (candidate.id, static_cast<float> (rel / candidate.length));
            }
            for (const auto& [targetId, splitFrac] : clipsToSplit)
            {
                const auto newId = host.splitClip (data.id, targetId, splitTime);
                if (!newId.empty())
                {
                    splitImageClipUI (targetId, newId, splitFrac);
                    updateImageClip (targetId);
                    updateImageClip (newId);
                }
            }
            syncActiveImageCrossfades();
            resetDragState();
            repaint();
            return;
        }

        if (e.mods.isShiftDown()
            && hit.zone == TimelineClipZone::gainEnvelopeLine)
        {
            draggedCurveSegmentIdx = clips::pinAutomationSegmentForVerticalDrag (
                ui.opacityPoints, hit.curveSegmentIdx);
        }

        startOpacityPoints = ui.opacityPoints;

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

    void mouseDrag (const juce::MouseEvent& e) override
    {
        mousePos = e.position;

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

        if (draggedClipId.empty() || !uiStates.contains (draggedClipId))
        {
            repaint();
            return;
        }

        auto& ui = uiStates[draggedClipId];
        const auto pps = getWidth() / 32.0;
        Clip* cl = nullptr;
        for (auto& c : data.clips) if (c.id == draggedClipId) { cl = &c; break; }
        if (!cl) return;
        const auto b = clipBounds (*cl, pps);

        // A) Spostamento della clip tramite presa sul titolo
        if (dragZone == TimelineClipZone::clipTitleMove)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            cl->start = std::max (0.0, startClipStart + deltaSec);
            layoutClipComponents();
            repaint();
            return;
        }

        // B) Drag orizzontale esclusivo della linea tratteggiata (Fade In/Out di Opacita')
        if (dragZone == TimelineClipZone::gainPointDashedLine && draggedOpacityIdx >= 0)
        {
            if (draggedOpacityIdx < static_cast<int> (ui.opacityPoints.size()))
            {
                const float currentNormX = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
                const float startNormX   = juce::jlimit (0.0f, 1.0f, (dragStartPos.x - b.getX()) / b.getWidth());
                const float deltaX       = currentNormX - startNormX;

                const bool isMulti = ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx)].selected;
                if (isMulti)
                {
                    for (size_t k = 0; k < ui.opacityPoints.size() && k < startOpacityPoints.size(); ++k)
                    {
                        if (startOpacityPoints[k].selected)
                        {
                            const float minX = (k == 0) ? 0.0f : (ui.opacityPoints[k - 1].normX + 0.01f);
                            const float maxX = (k == ui.opacityPoints.size() - 1) ? 1.0f : (ui.opacityPoints[k + 1].normX - 0.01f);
                            ui.opacityPoints[k].normX = juce::jlimit (minX, maxX, startOpacityPoints[k].normX + deltaX);
                        }
                    }
                }
                else
                {
                    const float minX = (draggedOpacityIdx == 0) ? 0.0f : (ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx - 1)].normX + 0.01f);
                    const float maxX = (draggedOpacityIdx == static_cast<int> (ui.opacityPoints.size()) - 1) ? 1.0f : (ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx + 1)].normX - 0.01f);
                    ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx)].normX = juce::jlimit (minX, maxX, currentNormX);
                }

                updateImageClip (draggedClipId);
                repaint();
                return;
            }
        }

        constexpr float kTitleAreaHeight = 16.0f;
        const float envHeight = juce::jmax (10.0f, b.getHeight() - kTitleAreaHeight);

        if (dragZone == TimelineClipZone::gainEnvelopeLine && draggedCurveSegmentIdx >= 0)
        {
            if (draggedCurveSegmentIdx + 1 < static_cast<int> (ui.opacityPoints.size()) &&
                draggedCurveSegmentIdx + 1 < static_cast<int> (startOpacityPoints.size()))
            {
                const float deltaOp = -(e.position.y - dragStartPos.y) / envHeight;
                const size_t s0 = static_cast<size_t> (draggedCurveSegmentIdx);
                const size_t s1 = s0 + 1;
                ui.opacityPoints[s0].opacity = juce::jlimit (0.0f, 1.0f, startOpacityPoints[s0].opacity + deltaOp);
                ui.opacityPoints[s1].opacity = juce::jlimit (0.0f, 1.0f, startOpacityPoints[s1].opacity + deltaOp);
                updateImageClip (draggedClipId);
                repaint();
            }
        }
        else if (dragZone == TimelineClipZone::gainEnvelopePoint && draggedOpacityIdx >= 0)
        {
            if (draggedOpacityIdx < static_cast<int> (ui.opacityPoints.size()))
            {
                const float normY = juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / envHeight);
                const float currentOp = clip_opacity_law::normToOpacity (normY);
                const float currentNormX = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());

                const float startNormY = juce::jlimit (0.0f, 1.0f, (b.getBottom() - dragStartPos.y) / envHeight);
                const float startOp = clip_opacity_law::normToOpacity (startNormY);
                const float startNormX = juce::jlimit (0.0f, 1.0f, (dragStartPos.x - b.getX()) / b.getWidth());

                const float deltaOp = currentOp - startOp;
                const float deltaX  = e.mods.isShiftDown() ? 0.0f : (currentNormX - startNormX);

                const bool isMulti = ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx)].selected;
                if (isMulti)
                {
                    for (size_t k = 0; k < ui.opacityPoints.size() && k < startOpacityPoints.size(); ++k)
                    {
                        if (startOpacityPoints[k].selected)
                        {
                            ui.opacityPoints[k].opacity = juce::jlimit (0.0f, 1.0f, startOpacityPoints[k].opacity + deltaOp);
                            const float minX = (k == 0) ? 0.0f : (ui.opacityPoints[k - 1].normX + 0.01f);
                            const float maxX = (k == ui.opacityPoints.size() - 1) ? 1.0f : (ui.opacityPoints[k + 1].normX - 0.01f);
                            ui.opacityPoints[k].normX = juce::jlimit (minX, maxX, startOpacityPoints[k].normX + deltaX);
                        }
                    }
                }
                else
                {
                    const float minX = (draggedOpacityIdx == 0) ? 0.0f : (ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx - 1)].normX + 0.01f);
                    const float maxX = (draggedOpacityIdx == static_cast<int> (ui.opacityPoints.size()) - 1) ? 1.0f : (ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx + 1)].normX - 0.01f);
                    if (!e.mods.isShiftDown())
                        ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx)].normX   = juce::jlimit (minX, maxX, currentNormX);
                    ui.opacityPoints[static_cast<size_t> (draggedOpacityIdx)].opacity = currentOp;

                    // Se il punto di ancoraggio modificato si trova sul confine del crossfade,
                    // interroga direttamente il solver (levelChange) per aggiornare la clip partner
                    if (draggedOpacityIdx == 0)
                    {
                        const auto partner = findActiveCrossfadeForFadeIn (draggedClipId);
                        if (partner.found && uiStates.contains (partner.partnerClipId))
                        {
                            auto& leftUi = uiStates[partner.partnerClipId];
                            ImageCrossfadeContext ctx;
                            ctx.interaction = CrossfadeInteractionKind::levelChange;
                            ctx.modifiedClipId = draggedClipId;
                            ctx.isLeftClipModified = false;
                            ctx.leftClipId = partner.partnerClipId;
                            ctx.rightClipId = draggedClipId;
                            ctx.currentLeftAlpha = leftUi.opacityPoints.back().opacity;
                            ctx.currentRightAlpha = currentOp;
                            ctx.requestedAlpha = currentOp;
                            ctx.leftFadeOutControl = leftUi.fadeOutControl;
                            ctx.rightFadeInControl = ui.fadeInControl;
                            ctx.overlapDurationSec = partner.overlapDuration;

                            const auto res = solveImageCrossfade (ctx);
                            ui.fadeInControl = res.rightFadeInControl;
                            leftUi.fadeOutControl = res.leftFadeOutControl;
                            updateImageClip (partner.partnerClipId);
                        }
                    }
                    else if (draggedOpacityIdx == static_cast<int> (ui.opacityPoints.size()) - 1)
                    {
                        const auto partner = findActiveCrossfadeForFadeOut (draggedClipId);
                        if (partner.found && uiStates.contains (partner.partnerClipId))
                        {
                            auto& rightUi = uiStates[partner.partnerClipId];
                            ImageCrossfadeContext ctx;
                            ctx.interaction = CrossfadeInteractionKind::levelChange;
                            ctx.modifiedClipId = draggedClipId;
                            ctx.isLeftClipModified = true;
                            ctx.leftClipId = draggedClipId;
                            ctx.rightClipId = partner.partnerClipId;
                            ctx.currentLeftAlpha = currentOp;
                            ctx.currentRightAlpha = rightUi.opacityPoints.front().opacity;
                            ctx.requestedAlpha = currentOp;
                            ctx.leftFadeOutControl = ui.fadeOutControl;
                            ctx.rightFadeInControl = rightUi.fadeInControl;
                            ctx.overlapDurationSec = partner.overlapDuration;

                            const auto res = solveImageCrossfade (ctx);
                            ui.fadeOutControl = res.leftFadeOutControl;
                            rightUi.fadeInControl = res.rightFadeInControl;
                            updateImageClip (partner.partnerClipId);
                        }
                    }
                }

                updateImageClip (draggedClipId);
                repaint();
            }
        }
        else if (dragZone == TimelineClipZone::gainSegmentCurveHandle && draggedCurveSegmentIdx >= 0)
        {
            if (draggedCurveSegmentIdx < static_cast<int> (ui.opacityPoints.size()) - 1)
            {
                const size_t segment = static_cast<size_t> (draggedCurveSegmentIdx);
                const float normX = juce::jlimit (
                    0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
                const float opacity = clip_opacity_law::normToOpacity (
                    juce::jlimit (0.0f, 1.0f,
                        (b.getBottom() - e.position.y) / envHeight));
                clip_opacity_law::setOpacityControlPoint (
                    ui.opacityPoints[segment], ui.opacityPoints[segment + 1],
                    normX, opacity);
            }
            updateImageClip (draggedClipId);
            repaint();
        }
        else if ((dragZone == TimelineClipZone::fadeInCurveHandle
                  || dragZone == TimelineClipZone::fadeOutCurveHandle)
                 && ui.opacityPoints.size() >= 2)
        {
            const bool fadeIn = dragZone == TimelineClipZone::fadeInCurveHandle;
            const juce::Rectangle<float> curveBounds (
                b.getX(), b.getBottom() - envHeight, b.getWidth(), envHeight);
            const auto geometry = clips::computeProjectedAutomationBoundaryGeometry (
                curveBounds, ui.opacityPoints.data(),
                static_cast<int> (ui.opacityPoints.size()),
                opacityBoundarySpec (ui),
                [] (const OpacityPoint& point) noexcept
                {
                    return clips::opacityPointAsAutomationPoint (point);
                });
            const auto& segment = fadeIn ? geometry.fadeIn : geometry.fadeOut;
            const auto control = ayra::curves::makeBoundedQuadraticControl (
                { segment.start.x, segment.start.y,
                  segment.end.x, segment.end.y },
                e.position.x, e.position.y);

            applyCrossfadeCurvature (draggedClipId, fadeIn, control);
            return;
        }
        // Edge Tools: Loop / Slip / Trim / Time-Stretch
        if (dragZone == TimelineClipZone::edgeLeftLoop)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            if (deltaSec >= 0.0)
            {
                const double maxDelta = startClipLength - 0.125;
                const double clampedDelta = std::clamp (deltaSec, 0.0, maxDelta);
                cl->start  = startClipStart + clampedDelta;
                cl->length = startClipLength - clampedDelta;
                ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
                ui.origClipLength  = cl->length;
                ui.stretchFrames   = 0;
                ui.isStretching    = false;
                loopPreview.duplicateCount = 0;
                loopPreview.trimDelta = clampedDelta;
            }
            else
            {
                cl->start  = startClipStart;
                cl->length = startClipLength;
                ui.sourceOffsetSec = startSourceOffsetSec;
                ui.origClipLength  = cl->length;
                ui.stretchFrames   = 0;
                ui.isStretching    = false;
                loopPreview.duplicateCount = clips::computeLoopDuplicateCount (
                    false, deltaSec, startClipStart, startClipLength);
                loopPreview.trimDelta = 0.0;
            }
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightLoop)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            if (deltaSec <= 0.0)
            {
                const double minDelta = -(startClipLength - 0.125);
                const double clampedDelta = std::clamp (deltaSec, minDelta, 0.0);
                cl->length = startClipLength + clampedDelta;
                ui.origClipLength  = cl->length;
                ui.stretchFrames   = 0;
                ui.isStretching    = false;
                loopPreview.duplicateCount = 0;
                loopPreview.trimDelta = clampedDelta;
            }
            else
            {
                cl->start  = startClipStart;
                cl->length = startClipLength;
                ui.origClipLength  = cl->length;
                ui.stretchFrames   = 0;
                ui.isStretching    = false;
                loopPreview.duplicateCount = clips::computeLoopDuplicateCount (
                    true, deltaSec, startClipStart, startClipLength);
                loopPreview.trimDelta = 0.0;
            }
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftSlip)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double totalDur = ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (startClipLength + 8.0);
            const double maxExpand = std::max (0.0, totalDur - (startSourceOffsetSec + startClipLength));
            const double clampedDelta = std::clamp (deltaSec, -std::min (maxExpand, startClipStart), startClipLength - 0.125);
            cl->start  = startClipStart + clampedDelta;
            cl->length = startClipLength - clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec;
            ui.origClipLength  = cl->length;
            ui.stretchFrames   = 0;
            ui.isStretching    = false;
            ui.isSlipping      = true;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightSlip)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double maxExpand = std::max (0.0, startSourceOffsetSec);
            const double clampedDelta = std::clamp (deltaSec, -(startClipLength - 0.125), maxExpand);
            cl->length = startClipLength + clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec - clampedDelta;
            ui.origClipLength  = cl->length;
            ui.stretchFrames   = 0;
            ui.isStretching    = false;
            ui.isSlipping      = true;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftTrim)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double maxExpand = std::max (0.0, startSourceOffsetSec);
            const double clampedDelta = std::clamp (deltaSec, -std::min (maxExpand, startClipStart), startClipLength - 0.125);
            cl->start  = startClipStart + clampedDelta;
            cl->length = startClipLength - clampedDelta;
            ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
            ui.origClipLength  = cl->length;
            ui.stretchFrames   = 0;
            ui.isStretching    = false;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightTrim)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double totalDur = ui.sourceTotalDurationSec > 0.0 ? ui.sourceTotalDurationSec : (startClipLength + 8.0);
            const double maxExpand = std::max (0.0, totalDur - (startSourceOffsetSec + startClipLength));
            const double clampedDelta = std::clamp (deltaSec, -(startClipLength - 0.125), maxExpand);
            cl->length = startClipLength + clampedDelta;
            ui.origClipLength = cl->length;
            ui.stretchFrames  = 0;
            ui.isStretching   = false;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeLeftStretch)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            const double fixedEnd = startClipStart + startClipLength;
            const double newStart = std::clamp (startClipStart + deltaSec, 0.0, fixedEnd - 0.125);
            cl->start  = newStart;
            cl->length = fixedEnd - newStart;
            ui.stretchFrames = static_cast<std::int64_t> (std::round ((cl->length - ui.origClipLength) * static_cast<double> (ui.frameRate)));
            ui.isStretching  = true;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::edgeRightStretch)
        {
            const double deltaSec = (e.position.x - dragStartPos.x) / pps;
            cl->length = std::max (0.125, startClipLength + deltaSec);
            ui.stretchFrames = static_cast<std::int64_t> (std::round ((cl->length - ui.origClipLength) * static_cast<double> (ui.frameRate)));
            ui.isStretching  = true;
            updateImageClip (draggedClipId);
            repaint();
        }
        else if (dragZone == TimelineClipZone::warpMarker && draggedMarkerIdx >= 0)
        {
            if (draggedMarkerIdx < static_cast<int> (ui.warpPoints.size()))
            {
                float leftLimit = 0.01f;
                if (draggedMarkerIdx > 0 && draggedMarkerIdx - 1 < static_cast<int> (ui.warpMarkers.size()))
                {
                    const float prevNorm = ui.warpMarkers[static_cast<size_t> (draggedMarkerIdx - 1)];
                    if (prevNorm >= 0.0f && prevNorm <= 1.0f) leftLimit = prevNorm + 0.015f;
                }
                float rightLimit = 0.99f;
                if (draggedMarkerIdx < static_cast<int> (ui.warpPoints.size()) - 1 && draggedMarkerIdx + 1 < static_cast<int> (ui.warpMarkers.size()))
                {
                    const float nextNorm = ui.warpMarkers[static_cast<size_t> (draggedMarkerIdx + 1)];
                    if (nextNorm >= 0.0f && nextNorm <= 1.0f) rightLimit = nextNorm - 0.015f;
                }

                const float rawNorm = (e.position.x - b.getX()) / b.getWidth();
                const float clampedNorm = juce::jlimit (leftLimit, rightLimit, rawNorm);

                if (ui.isStretching || ui.stretchFrames != 0)
                    ui.warpPoints[static_cast<size_t> (draggedMarkerIdx)].warpedTimeSec =
                        ui.startSourceOffsetSec + static_cast<double> (clampedNorm) * ui.origClipLength;
                else
                    ui.warpPoints[static_cast<size_t> (draggedMarkerIdx)].warpedTimeSec =
                        ui.sourceOffsetSec + static_cast<double> (clampedNorm) * cl->length;

                updateImageClip (draggedClipId);
                repaint();
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isResizingLaneHeight) { isResizingLaneHeight = false; resetDragState(); return; }
        if (isMarqueeActive) { marqueeSelector.handleMouseUp (e); isMarqueeActive = false; resetDragState(); return; }

        if (dragZone == TimelineClipZone::clipTitleMove && !draggedClipId.empty())
        {
            for (const auto& c : data.clips)
                if (c.id == draggedClipId) { host.moveClip (data.id, draggedClipId, c.start); break; }
        }

        if (dragZone == TimelineClipZone::edgeLeftLoop || dragZone == TimelineClipZone::edgeRightLoop)
        {
            if (loopPreview.active && !draggedClipId.empty())
            {
                if (loopPreview.duplicateCount > 0)
                {
                    const auto created = host.duplicateClipLoop (
                        data.id, draggedClipId, loopPreview.duplicateCount, loopPreview.isRightEdge);
                    duplicateImageClipLoopUI (draggedClipId, created);
                }
                else
                {
                    for (const auto& c : data.clips)
                    {
                        if (c.id == draggedClipId)
                        {
                            host.resizeClip (data.id, c.id, c.start, c.length);
                            break;
                        }
                    }
                }
            }
            loopPreview = {};
        }

        if (dragZone == TimelineClipZone::edgeLeftTrim    || dragZone == TimelineClipZone::edgeRightTrim ||
            dragZone == TimelineClipZone::edgeLeftStretch || dragZone == TimelineClipZone::edgeRightStretch ||
            dragZone == TimelineClipZone::edgeLeftSlip    || dragZone == TimelineClipZone::edgeRightSlip)
        {
            for (const auto& c : data.clips)
                if (c.id == draggedClipId) { host.resizeClip (data.id, c.id, c.start, c.length); break; }
        }

        if (uiStates.contains (draggedClipId))
        {
            auto& ui = uiStates[draggedClipId];
            ui.isStretching = false;
            ui.isSlipping   = false;

            if (dragZone == TimelineClipZone::warpMarker)
            {
                std::sort (ui.warpPoints.begin(), ui.warpPoints.end(),
                           [] (const ImageWarpPoint& a, const ImageWarpPoint& b) { return a.warpedTimeSec < b.warpedTimeSec; });
            }

            if (dragZone == TimelineClipZone::gainEnvelopePoint || dragZone == TimelineClipZone::gainPointDashedLine)
            {
                const int endpointIndex = draggedOpacityIdx;
                std::sort (ui.opacityPoints.begin(), ui.opacityPoints.end(),
                           [] (const OpacityPoint& a, const OpacityPoint& b) { return a.normX < b.normX; });
                if (!ui.opacityPoints.empty()
                    && (endpointIndex == 0
                        || endpointIndex == static_cast<int> (ui.opacityPoints.size()) - 1))
                {
                    for (auto& c : data.clips)
                    {
                        if (c.id == draggedClipId)
                        {
                            syncClipFadeProjection (c, ui);
                            host.updateClipFade (
                                data.id, draggedClipId, c.fadeIn, c.fadeOut);
                            break;
                        }
                    }
                }
            }

            updateImageClip (draggedClipId);
        }

        resetDragState();
        mouseMove (e);
        repaint();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        // Rollback deterministico della gesture attiva con Escape
        if (key.isKeyCode (juce::KeyPress::escapeKey) && !draggedClipId.empty() && hasDragSnapshot)
        {
            uiStates[draggedClipId] = snapshotUIState;
            for (auto& c : data.clips)
            {
                if (c.id == draggedClipId)
                {
                    c.start  = startClipStart;
                    c.length = startClipLength;
                    break;
                }
            }
            updateImageClip (draggedClipId);
            resetDragState();
            repaint();
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::backspaceKey || key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            bool anyChanged = false;
            for (auto& [cid, ui] : uiStates)
            {
                if (ui.opacityPoints.size() > 2)
                {
                    auto it = ui.opacityPoints.begin() + 1;
                    while (it != ui.opacityPoints.end() - 1)
                    {
                        if (it->selected) { it = ui.opacityPoints.erase (it); anyChanged = true; }
                        else ++it;
                    }
                    if (anyChanged)
                        updateImageClip (cid);
                }
            }
            if (anyChanged) { repaint(); return true; }
        }

        if (key.isKeyCode (juce::KeyPress::upKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
        {
            host.adjustVerticalZoom (0.12f);
            return true;
        }
        if (key.isKeyCode (juce::KeyPress::downKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
        {
            host.adjustVerticalZoom (-0.12f);
            return true;
        }
        return false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;

        const auto hit = hitTestZones (e.position);

        // 0. Doppio click sul titolo o header della clip (o intera clip se compressa):
        //    cicla le 3 altezze canoniche condivise (96 -> 256 -> 400 -> 96) tenendo conto dello zoom verticale
        if (hit.clip != nullptr && clip_height_law::isHeaderAreaOrClosedClip (hit.zone, hit.bounds.getHeight(), hit.markerIdx))
        {
            const int modelHeight = clip_height_law::nextModelHeight (static_cast<float> (height), host.getVerticalZoom());
            resetDragState();
            host.store().resize (data.id, modelHeight);
            return;
        }

        // Se doppio click sul divisore inferiore della traccia
        if (e.position.y >= getHeight() - 6.0f)
        {
            const int modelHeight = clip_height_law::nextModelHeight (static_cast<float> (height), host.getVerticalZoom());
            host.store().resize (data.id, modelHeight);
            return;
        }

        if (hit.clip == nullptr) return;
        auto& ui = uiStates[hit.clip->id];

        const double clickSec = clips::timelineSecondsFromCanvasX (e.position.x, getWidth());
        const auto ovInfo = findOverlapAtTime (clickSec, hit.clip->id);
        if (ovInfo.inOverlap && hit.zone == TimelineClipZone::none)
        {
            if (ovInfo.isSubsumed)
            {
                // In Layering / Mute Overlay mode (clip totally inside another), 2-way crossfade is disabled.
                return;
            }
            const std::pair<std::string, std::string> pairKey { ovInfo.leftClipId, ovInfo.rightClipId };
            if (xfadePairs.contains (pairKey))
                xfadePairs.erase (pairKey);
            else
                xfadePairs.insert (pairKey);
            syncActiveImageCrossfades (hit.clip->id, true);
            updateImageClip (ovInfo.leftClipId);
            updateImageClip (ovInfo.rightClipId);
            repaint();
            return;
        }

        // Reset Stretch
        if (hit.zone == TimelineClipZone::edgeLeftStretch || hit.zone == TimelineClipZone::edgeRightStretch)
        {
            if (ui.origClipLength > 0.125)
            {
                ui.stretchFrames = 0;
                ui.isStretching  = false;
                host.resizeClip (data.id, hit.clip->id, hit.clip->start, ui.origClipLength);
                updateImageClip (hit.clip->id);
                repaint();
                return;
            }
        }

        // Reset Slip
        if (hit.zone == TimelineClipZone::edgeLeftSlip || hit.zone == TimelineClipZone::edgeRightSlip)
        {
            ui.sourceOffsetSec = ui.defaultSourceOffsetSec;
            ui.slipDeltaFrames = 0;
            updateImageClip (hit.clip->id);
            repaint();
            return;
        }

        // 1. Doppio click su punto di Opacita': reset a 100% (1.0f)
        if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.opacityIdx >= 0 && hit.opacityIdx < static_cast<int> (ui.opacityPoints.size()))
        {
            ui.opacityPoints[static_cast<size_t> (hit.opacityIdx)].opacity = 1.0f;
            updateImageClip (hit.clip->id);
            repaint();
            return;
        }

        // 2. Doppio click su triangolino Warp Marker: reset a posizione originale
        if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 && hit.markerIdx < static_cast<int> (ui.warpPoints.size()))
        {
            ui.warpPoints[static_cast<size_t> (hit.markerIdx)].warpedTimeSec =
                ui.warpPoints[static_cast<size_t> (hit.markerIdx)].sourceTimeSec;
            updateImageClip (hit.clip->id);
            repaint();
            return;
        }

        // 3. Doppio click sulla linea di Opacita': aggiunge un nuovo punto
        if (hit.zone == TimelineClipZone::gainEnvelopeLine)
        {
            constexpr float kTitleAreaHeight = 16.0f;
            const float envHeight = juce::jmax (10.0f, hit.bounds.getHeight() - kTitleAreaHeight);
            const float normX = juce::jlimit (0.01f, 0.99f, (e.position.x - hit.bounds.getX()) / hit.bounds.getWidth());
            const float normY = juce::jlimit (0.0f, 1.0f, (hit.bounds.getBottom() - e.position.y) / envHeight);
            const float clickedOp = clip_opacity_law::normToOpacity (normY);

            for (auto& pt : ui.opacityPoints) pt.selected = false;
            ui.opacityPoints.push_back ({ normX, clickedOp, 0.5f, true });
            std::sort (ui.opacityPoints.begin(), ui.opacityPoints.end(),
                       [] (const OpacityPoint& a, const OpacityPoint& b) { return a.normX < b.normX; });
            updateImageClip (hit.clip->id);
            repaint();
            return;
        }

        // 4. Doppio click su handle di curvatura: reset a lineare 0.5f
        if (hit.zone == TimelineClipZone::gainSegmentCurveHandle && hit.curveSegmentIdx >= 0
            && hit.curveSegmentIdx < static_cast<int> (ui.opacityPoints.size()) - 1)
        {
            const size_t segment = static_cast<size_t> (hit.curveSegmentIdx);
            clip_opacity_law::resetOpacityControlPoint (
                ui.opacityPoints[segment], ui.opacityPoints[segment + 1]);
            updateImageClip (hit.clip->id);
            repaint();
            return;
        }

        if (hit.zone == TimelineClipZone::fadeInCurveHandle)
        {
            applyCrossfadeCurvature (hit.clip->id, true, { 0.5, 0.5, -1.0 });
            return;
        }
        if (hit.zone == TimelineClipZone::fadeOutCurveHandle)
        {
            applyCrossfadeCurvature (hit.clip->id, false, { 0.5, 0.5, -1.0 });
            return;
        }

        host.openClip (data.id, hit.clip->id);
    }

private:
    void resetDragState() noexcept
    {
        draggedClipId.clear();
        dragZone = TimelineClipZone::none;
        draggedMarkerIdx = -1;
        draggedOpacityIdx = -1;
        draggedCurveSegmentIdx = -1;
        hasDragSnapshot = false;
        loopPreview = {};
    }

    juce::Array<juce::Image> sampleThumbnails { loadSampleThumbnails() };

    ClipFeatures clipFeatures;
    LabComponent& host;
    Track data;
    int height = 116;
    bool programAudible = true;

    std::map<std::string, ImageClipUIState> uiStates;
    std::set<std::pair<std::string, std::string>> xfadePairs;
    std::map<std::string, std::unique_ptr<ImageClipComponent>> clipComponents;

    ayra::MarqueeSelector marqueeSelector { *this };
    bool isMarqueeActive = false;

    double startClipStart = 0.0;
    double startClipLength = 0.0;
    double startSourceOffsetSec = 0.0;
    clips::ClipLoopDragState loopPreview;

    std::string hoveredClipId;
    TimelineClipZone activeZone = TimelineClipZone::none;
    int hoveredMarkerIdx = -1;
    int hoveredOpacityIdx = -1;
    int hoveredCurveSegmentIdx = -1;
    juce::Point<float> mousePos { -1.0f, -1.0f };

    std::string draggedClipId;
    TimelineClipZone dragZone = TimelineClipZone::none;
    int draggedMarkerIdx = -1;
    int draggedOpacityIdx = -1;
    int draggedCurveSegmentIdx = -1;
    juce::Point<float> dragStartPos;

    std::vector<OpacityPoint> startOpacityPoints;

    bool hasDragSnapshot = false;
    ImageClipUIState snapshotUIState;

    bool isHoveringDivider = false;
    bool isResizingLaneHeight = false;
    int resizeLaneStartY = 0;
    int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
