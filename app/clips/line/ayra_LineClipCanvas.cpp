/** @file ayra_LineClipCanvas.cpp
    Adapter del Lab per una lane di `LineClipComponent` (CC, ModWheel, Pitch
   Bend, Sustain, Parametri). Implementa:
    - Isolamento rigoroso fuori clip (nessuna rampa tra clip distanti);
    - Supporto modalita' Continuous Curve e Gate/Block (es. Sustain CC64);
    - Split quadratico esatto di De Casteljau;
    - Mapping bidirezionale source <-> warped (inclusa l'inversa per editing
   sotto warp);
    - Ciclo dei 3 livelli di zoom verticale tramite doppio click sul titolo;
    - Rimozione totale del tasto '1' (Edge tools spaziali sempre attivi);
    - Rollback deterministico della gesture tramite tasto Esc. */

namespace ayra::lab {
namespace {

using clips::ClipFeatures;
using clips::computeClipFeaturesLOD;
using clips::CrossfadeInteractionKind;
using clips::defaultLineCrossfadeSolver;
using clips::LineClipComponent;
using clips::LineClipEndBehavior;
using clips::LineClipRenderMode;
using clips::LineClipVisual;
using clips::LineCrossfadeContext;
using clips::LineCrossfadeResult;
using clips::LineCrossfadeSolver;
using clips::makeBoundedCurvePixelSegment;
using clips::MidiEnvelopePoint;
using clips::TimelineClipZone;
namespace clip_height_law = clips::clip_height_law;

struct LineWarpPoint {
  double sourceTimeSec = 0.0;
  double warpedTimeSec = 0.0;
};

struct LineClipUIState {
  LineClipRenderMode renderMode = LineClipRenderMode::continuousCurve;
  LineClipEndBehavior endBehavior = LineClipEndBehavior::holdLastOrSilent;
  ayra::curves::CurveBoundarySpec boundary;

  float defaultNormValue = 0.0f;
  float minDisplayValue = 0.0f;
  float maxDisplayValue = 127.0f;
  juce::String valueUnit;

  std::vector<MidiEnvelopePoint> points;
  std::vector<LineWarpPoint> warpPoints;
  std::vector<float> warpMarkers;
  std::vector<float> origWarpMarkers;

  double origClipLength = 0.0;
  std::int64_t stretchTicks = 0;
  bool isStretching = false;

  double sourceTotalDurationSec = 0.0;
  double sourceOffsetSec = 0.0;
  double defaultSourceOffsetSec = 0.0;
  double startSourceOffsetSec = 0.0;
  std::int64_t slipDeltaTicks = 0;
  bool isSlipping = false;
};

static void syncLineWarpNorms(LineClipUIState &ui, double clipLength,
                              double sourceOffset) {
  ui.warpMarkers.clear();
  ui.origWarpMarkers.clear();
  ui.warpMarkers.reserve(ui.warpPoints.size());
  ui.origWarpMarkers.reserve(ui.warpPoints.size());

  const double len = std::max(0.05, clipLength);
  const bool isStretched =
      (ui.isStretching || ui.stretchTicks != 0) && (ui.origClipLength > 0.05);

  for (const auto &wp : ui.warpPoints) {
    float curNorm = 0.0f;
    float origNorm = 0.0f;

    if (isStretched) {
      origNorm = static_cast<float>(
          (wp.sourceTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
      curNorm = static_cast<float>(
          (wp.warpedTimeSec - ui.startSourceOffsetSec) / ui.origClipLength);
    } else {
      curNorm = static_cast<float>((wp.warpedTimeSec - sourceOffset) / len);
      origNorm = static_cast<float>((wp.sourceTimeSec - sourceOffset) / len);
    }

    ui.warpMarkers.push_back(curNorm);
    ui.origWarpMarkers.push_back(origNorm);
  }
}

class LineClipCanvas final : public juce::Component,
                             public juce::TooltipClient,
                             public clips::LineClipPresentationProvider {
public:
  CanvasViewportBinding panelView;

  // The identity lens uses the exact clip/edit hit regions (including rails
  // and handles), not an approximate lane-wide temporal rectangle.
  bool containsPanelClipAt(juce::Point<float> point) const
  {
      return getLocalBounds().toFloat().contains(point) && hitTestZones(point).clip != nullptr;
  }


  void includePanelPartners(std::set<std::string>& visible)const
  {
      bool changed=true;
      while(changed){changed=false;for(const auto& pair:xfadePairs)
          if(visible.contains(pair.first)||visible.contains(pair.second))
          {changed|=visible.insert(pair.first).second;changed|=visible.insert(pair.second).second;}}
  }
  std::shared_ptr<void> capturePanelState() const
  {
      using State = std::tuple<decltype(uiStates),decltype(xfadePairs)>;
      return std::make_shared<State>(uiStates,xfadePairs);
  }
  void restorePanelState(const std::shared_ptr<void>& state)
  {
      if(!state)return;
      using State = std::tuple<decltype(uiStates),decltype(xfadePairs)>;
      const auto& saved=*std::static_pointer_cast<State>(state);
      uiStates=std::get<0>(saved); xfadePairs=std::get<1>(saved);
  }


  // Crossfade Compensation Solver per LineClip
  LineCrossfadeSolver crossfadeSolver;

  void setCrossfadeSolver(LineCrossfadeSolver solver) {
    crossfadeSolver = std::move(solver);
  }

  [[nodiscard]] LineCrossfadeResult
  solveLineCrossfade(const LineCrossfadeContext &ctx) const {
    if (crossfadeSolver != nullptr)
      return crossfadeSolver(ctx);
    return defaultLineCrossfadeSolver(ctx);
  }

  [[nodiscard]] const Clip *findClip(const std::string &clipId) const noexcept {
    for (const auto &c : data.clips)
      if (c.id == clipId)
        return &c;
    return nullptr;
  }

  struct CrossfadePartnerLookup {
    bool found = false;
    std::string partnerClipId;
    double overlapDuration = 0.0;
  };

  [[nodiscard]] CrossfadePartnerLookup
  findActiveCrossfadeForFadeIn(const std::string &clipId) const {
    for (const auto &pair : xfadePairs) {
      if (pair.second == clipId) {
        const auto *left = findClip(pair.first);
        const auto *right = findClip(pair.second);
        if (left != nullptr && right != nullptr) {
          if (clips::isClipSubInterval(left->start, left->length, right->start,
                                       right->length))
            continue;
          const double ovStart = std::max(left->start, right->start);
          const double ovEnd = std::min(left->start + left->length,
                                        right->start + right->length);
          return {true, pair.first, std::max(0.001, ovEnd - ovStart)};
        }
      }
    }
    return {};
  }

  [[nodiscard]] CrossfadePartnerLookup
  findActiveCrossfadeForFadeOut(const std::string &clipId) const {
    for (const auto &pair : xfadePairs) {
      if (pair.first == clipId) {
        const auto *left = findClip(pair.first);
        const auto *right = findClip(pair.second);
        if (left != nullptr && right != nullptr) {
          if (clips::isClipSubInterval(left->start, left->length, right->start,
                                       right->length))
            continue;
          const double ovStart = std::max(left->start, right->start);
          const double ovEnd = std::min(left->start + left->length,
                                        right->start + right->length);
          return {true, pair.second, std::max(0.001, ovEnd - ovStart)};
        }
      }
    }
    return {};
  }

  //==============================================================================
  // Crossfade Interaction Dispatchers (SSOT per Drag & Doppio Click)
  //==============================================================================

  void applyCrossfadeCurvature(const std::string &clipId, bool isFadeIn,
                               const ayra::curves::QuadraticControl &control) {
    if (!uiStates.contains(clipId))
      return;
    auto &ui = uiStates[clipId];

    if (isFadeIn) {
      const auto partner = findActiveCrossfadeForFadeIn(clipId);
      if (partner.found && uiStates.contains(partner.partnerClipId)) {
        auto &leftUi = uiStates[partner.partnerClipId];
        LineCrossfadeContext ctx;
        ctx.interaction = CrossfadeInteractionKind::curvatureChange;
        ctx.modifiedClipId = clipId;
        ctx.isLeftClipModified = false;
        ctx.leftClipId = partner.partnerClipId;
        ctx.rightClipId = clipId;
        ctx.currentLeftValue = leftUi.points.back().normValue;
        ctx.currentRightValue = ui.points.front().normValue;
        ctx.leftFadeOutControl = leftUi.boundary.fadeOutControl;
        ctx.rightFadeInControl = control;
        ctx.requestedControl = control;
        ctx.overlapDurationSec = partner.overlapDuration;

        const auto res = solveLineCrossfade(ctx);
        ui.boundary.fadeInControl = res.rightFadeInControl;
        leftUi.boundary.fadeOutControl = res.leftFadeOutControl;
      } else {
        ui.boundary.fadeInControl = control;
      }
    } else // fadeOut
    {
      const auto partner = findActiveCrossfadeForFadeOut(clipId);
      if (partner.found && uiStates.contains(partner.partnerClipId)) {
        auto &rightUi = uiStates[partner.partnerClipId];
        LineCrossfadeContext ctx;
        ctx.interaction = CrossfadeInteractionKind::curvatureChange;
        ctx.modifiedClipId = clipId;
        ctx.isLeftClipModified = true;
        ctx.leftClipId = clipId;
        ctx.rightClipId = partner.partnerClipId;
        ctx.currentLeftValue = ui.points.back().normValue;
        ctx.currentRightValue = rightUi.points.front().normValue;
        ctx.leftFadeOutControl = control;
        ctx.rightFadeInControl = rightUi.boundary.fadeInControl;
        ctx.requestedControl = control;
        ctx.overlapDurationSec = partner.overlapDuration;

        const auto res = solveLineCrossfade(ctx);
        ui.boundary.fadeOutControl = res.leftFadeOutControl;
        rightUi.boundary.fadeInControl = res.rightFadeInControl;
      } else {
        ui.boundary.fadeOutControl = control;
      }
    }

    syncActiveLineCrossfades(clipId, false);
    repaint();
  }

  void applyCrossfadeLevel(const std::string &clipId, bool isFadeIn,
                           float requestedNormValue) {
    if (!uiStates.contains(clipId))
      return;
    auto &ui = uiStates[clipId];

    if (isFadeIn) {
      if (!ui.points.empty())
        ui.points.front().normValue = requestedNormValue;
      const auto partner = findActiveCrossfadeForFadeIn(clipId);
      if (partner.found && uiStates.contains(partner.partnerClipId)) {
        auto &leftUi = uiStates[partner.partnerClipId];
        LineCrossfadeContext ctx;
        ctx.interaction = CrossfadeInteractionKind::levelChange;
        ctx.modifiedClipId = clipId;
        ctx.isLeftClipModified = false;
        ctx.leftClipId = partner.partnerClipId;
        ctx.rightClipId = clipId;
        ctx.currentLeftValue = leftUi.points.back().normValue;
        ctx.currentRightValue = requestedNormValue;
        ctx.requestedValue = requestedNormValue;
        ctx.leftFadeOutControl = leftUi.boundary.fadeOutControl;
        ctx.rightFadeInControl = ui.boundary.fadeInControl;
        ctx.overlapDurationSec = partner.overlapDuration;

        const auto res = solveLineCrossfade(ctx);
        ui.boundary.fadeInControl = res.rightFadeInControl;
        leftUi.boundary.fadeOutControl = res.leftFadeOutControl;
      }
    } else // fadeOut
    {
      if (!ui.points.empty())
        ui.points.back().normValue = requestedNormValue;
      const auto partner = findActiveCrossfadeForFadeOut(clipId);
      if (partner.found && uiStates.contains(partner.partnerClipId)) {
        auto &rightUi = uiStates[partner.partnerClipId];
        LineCrossfadeContext ctx;
        ctx.interaction = CrossfadeInteractionKind::levelChange;
        ctx.modifiedClipId = clipId;
        ctx.isLeftClipModified = true;
        ctx.leftClipId = clipId;
        ctx.rightClipId = partner.partnerClipId;
        ctx.currentLeftValue = requestedNormValue;
        ctx.currentRightValue = rightUi.points.front().normValue;
        ctx.requestedValue = requestedNormValue;
        ctx.leftFadeOutControl = ui.boundary.fadeOutControl;
        ctx.rightFadeInControl = rightUi.boundary.fadeInControl;
        ctx.overlapDurationSec = partner.overlapDuration;

        const auto res = solveLineCrossfade(ctx);
        ui.boundary.fadeOutControl = res.leftFadeOutControl;
        rightUi.boundary.fadeInControl = res.rightFadeInControl;
      }
    }

    syncActiveLineCrossfades(clipId, true);
    repaint();
  }

  explicit LineClipCanvas(
      LabComponent &h,
      const timeline_visual_style::CurveLineColourVariant colourVariant =
          timeline_visual_style::CurveLineColourVariant::light,
      const TimelineCurveUnderlayStyle underlayStyle =
          TimelineCurveUnderlayStyle::darkBelow)
      : host(h),
        curveColour(timeline_visual_style::curveLineColour(colourVariant)),
        curveUnderlayStyle(underlayStyle) {
    setWantsKeyboardFocus(true);
    marqueeSelector.setFillColour(juce::Colour(0x3000d084));
    marqueeSelector.setOutlineColour(juce::Colour(0xe000d084));

    marqueeSelector.onPerformMarqueeSelection =
        [this](const juce::Rectangle<int> &area,
               ayra::MarqueeSelector::ModifierMode mode) {
          const auto pps = panelView.pixelsPerSecond(getWidth());
          bool anyChanged = false;

          for (const auto &c : data.clips) {
            const auto b = clipBounds(c, pps);
            auto &ui = uiStates[c.id];

            constexpr float kTitleAreaHeight = 16.0f;
            const float bodyH =
                juce::jmax(10.0f, b.getHeight() - kTitleAreaHeight);

            for (int pointIndex = 0;
                 pointIndex < static_cast<int>(ui.points.size());
                 ++pointIndex) {
              auto &pt = ui.points[static_cast<size_t>(pointIndex)];
              const float px = clips::automationPointRenderX(
                  b, pointIndex, static_cast<int>(ui.points.size()), pt.normX);
              const float py = b.getBottom() -
                               juce::jlimit(0.0f, 1.0f, pt.normValue) * bodyH;
              const bool inside =
                  area.contains(juce::roundToInt(px), juce::roundToInt(py));

              if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
                pt.selected = inside;
              else if (mode == ayra::MarqueeSelector::ModifierMode::Add &&
                       inside)
                pt.selected = true;
              else if (mode == ayra::MarqueeSelector::ModifierMode::Subtract &&
                       inside)
                pt.selected = false;
              else if (mode == ayra::MarqueeSelector::ModifierMode::Toggle &&
                       inside)
                pt.selected = !pt.selected;

              anyChanged = true;
            }
          }

          if (anyChanged)
            repaint();
        };

    marqueeSelector.onPerformClickSelection =
        [this](juce::Point<int>, ayra::MarqueeSelector::ModifierMode mode) {
          if (mode == ayra::MarqueeSelector::ModifierMode::Replace) {
            for (auto &[cid, ui] : uiStates)
              for (auto &pt : ui.points)
                pt.selected = false;
          }
        };
  }

  void setCurveUnderlayStyle(const TimelineCurveUnderlayStyle style) noexcept {
    if (curveUnderlayStyle == style) {
      return;
    }
    curveUnderlayStyle = style;
    for (auto &[id, component] : clipComponents) {
      juce::ignoreUnused(id);
      component->repaint();
    }
  }

  juce::String getTooltip() override {
    switch (activeZone) {
    case TimelineClipZone::edgeLeftLoop:
    case TimelineClipZone::edgeRightLoop:
      return "Loop / Duplica clip (Repeat Sizing)\nTrascina verso l'interno "
             "per trimmare, verso l'esterno per creare copie concatenate della "
             "clip (stile Cubase)";
    case TimelineClipZone::edgeLeftSlip:
    case TimelineClipZone::edgeRightSlip:
      return "Slip Contenuto LineClip (Sizing Moves Contents)\nTrascina per "
             "scorrere la finestra sulla curva mantenendo ancorati i nodi";
    case TimelineClipZone::edgeLeftTrim:
    case TimelineClipZone::edgeRightTrim:
      return "Trim LineClip (Normal Sizing)\nRiduce o estende i confini di "
             "attivita' della clip (fuori dalla clip il parametro non genera "
             "rampe)";
    case TimelineClipZone::edgeLeftStretch:
    case TimelineClipZone::edgeRightStretch:
      return "Time Stretch LineClip\nTrascina per scalare temporalmente la "
             "curva (doppio click per reset 1.0x)";
    case TimelineClipZone::gainPointDashedLine:
      return "Posizione Temporale Nodo\nTrascina orizzontalmente per muovere "
             "il nodo nel tempo senza alterarne il valore";
    case TimelineClipZone::gainEnvelopePoint:
      return "Nodo LineClip\nTrascina per modificare valore e tempo; primo e "
             "ultimo sono permanenti, Alt+Click elimina solo i nodi interni";
    case TimelineClipZone::gainSegmentCurveHandle:
      return "Curvatura Segmento\nTrascina in orizzontale e verticale per "
             "modellare la curva (doppio click per lineare)";
    case TimelineClipZone::fadeInCurveHandle:
      return "Curvatura Fade In LineClip\nTrascina dentro il rettangolo tra "
             "anchor iniziale e primo nodo";
    case TimelineClipZone::fadeOutCurveHandle:
      return "Curvatura Fade Out LineClip\nTrascina dentro il rettangolo tra "
             "ultimo nodo e anchor finale";
    case TimelineClipZone::gainEnvelopeLine:
      return "Segmento Curva LineClip\nDoppio click per inserire un nuovo "
             "nodo; trascina in verticale per alzare/abbassare il segmento";
    case TimelineClipZone::topWarpRail:
      return "Binario Warp Superiore\nClicca per inserire un Warp Marker sulla "
             "LineClip";
    case TimelineClipZone::warpMarker:
      return "Warp Marker LineClip\nTrascina per deformare localmente la curva "
             "(Doppio click reset, Alt+Click elimina)";
    case TimelineClipZone::bottomCutRail:
      return "Binario di Taglio (Forbici)\nClicca per dividere la LineClip "
             "preservando esattamente la curva quadratica (De Casteljau)";
    case TimelineClipZone::clipTitleMove:
      return "Intestazione LineClip\nTrascina per spostare la clip; Doppio "
             "click per ciclare i 3 livelli di zoom verticale";
    default:
      if (!panelView.enabled && isHoveringDivider)
        return "Separatore Traccia\nTrascina verticalmente per ridimensionare "
               "l'altezza";
      if (!hoveredClipId.empty())
        return "Corpo LineClip\nTrascina per selezione marquee dei nodi";
      break;
    }
    return {};
  }

  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override {
        if(panelView.enabled && (e.mods.isCommandDown()||e.mods.isCtrlDown()||e.mods.isShiftDown()))
        { Component::mouseWheelMove(e,wheel);return; }

    if (e.mods.isAltDown() || e.mods.isCommandDown() || e.mods.isCtrlDown() ||
        e.mods.isShiftDown()) {
      const float delta =
          (std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX);
      if (std::abs(delta) > 0.001f) {
        host.adjustVerticalZoom(delta > 0.0f ? 0.08f : -0.08f);
        return;
      }
    }
    if (auto *vp = findParentComponentOfClass<juce::Viewport>())
      vp->mouseWheelMove(e, wheel);
    else
      Component::mouseWheelMove(e, wheel);
  }

  void duplicateLineClipLoopUI(const std::string &origId,
                               const std::vector<std::string> &newIds) {
    if (!uiStates.contains(origId))
      return;
    const auto &orig = uiStates[origId];
    for (const auto &newId : newIds) {
      LineClipUIState dup = orig;
      dup.isStretching = false;
      dup.isSlipping = false;
      uiStates[newId] = std::move(dup);
    }
  }

  void splitLineClipUI(const std::string &origId, const std::string &newId,
                       float splitFrac) {
    if (!uiStates.contains(origId) || !std::isfinite(splitFrac) ||
        splitFrac <= 0.0f || splitFrac >= 1.0f)
      return;

    auto &orig = uiStates[origId];
    LineClipUIState next = orig;

    const double origLenBefore =
        orig.origClipLength > 0.0 ? orig.origClipLength : 4.0;
    const double splitSourceTime =
        orig.sourceOffsetSec + static_cast<double>(splitFrac) * origLenBefore;

    // 1. Partiziona i Warp Points
    std::vector<LineWarpPoint> origWps, nextWps;
    for (const auto &wp : orig.warpPoints) {
      if (wp.sourceTimeSec < splitSourceTime - 0.005)
        origWps.push_back(wp);
      else if (wp.sourceTimeSec > splitSourceTime + 0.005)
        nextWps.push_back(wp);
    }
    orig.warpPoints = std::move(origWps);
    next.warpPoints = std::move(nextWps);

    // 2. Partiziona la curva con la policy canonica. Anche il taglio che
    // coincide con un punto conserva il controllo del segmento destro.
    auto curveSplit = clips::splitAutomationPointSequence(
        orig.points, orig.defaultNormValue, splitFrac, orig.boundary);
    if (curveSplit.left.empty() || curveSplit.right.empty()) {
      return;
    }
    orig.points = std::move(curveSplit.left);
    orig.boundary = curveSplit.leftBoundary;
    next.points = std::move(curveSplit.right);
    next.boundary = curveSplit.rightBoundary;

    next.origClipLength = origLenBefore * (1.0 - splitFrac);
    orig.origClipLength = origLenBefore * splitFrac;
    next.sourceTotalDurationSec =
        orig.sourceTotalDurationSec > 0.0 ? orig.sourceTotalDurationSec : 16.0;
    next.sourceOffsetSec = splitSourceTime;
    next.defaultSourceOffsetSec = splitSourceTime;
    next.startSourceOffsetSec = next.sourceOffsetSec;

    syncLineWarpNorms(orig, orig.origClipLength, orig.sourceOffsetSec);
    syncLineWarpNorms(next, next.origClipLength, next.sourceOffsetSec);

    uiStates[newId] = std::move(next);
  }

  void setModel(const Track &t, int laneHeight) {
    data = t;
    height = laneHeight;
    programAudible = audibility(host.store().state(), data).program;

    std::set<std::string> live;
    for (const auto &c : t.clips) {
      live.insert(c.id);
      auto component = clipComponents.find(c.id);
      if (component == clipComponents.end()) {
        auto &provider =
            static_cast<clips::LineClipPresentationProvider &>(*this);
        auto lineClip = std::make_unique<LineClipComponent>(c.id, provider);
        addAndMakeVisible(*lineClip);
        component = clipComponents.emplace(c.id, std::move(lineClip)).first;
      }
      component->second->toFront(false);

      if (!uiStates.contains(c.id)) {
        LineClipUIState s;
        s.origClipLength = c.length;
        s.sourceTotalDurationSec = c.length + 8.0;
        s.sourceOffsetSec = 4.0;
        s.defaultSourceOffsetSec = s.sourceOffsetSec;
        s.startSourceOffsetSec = s.sourceOffsetSec;

        const juce::String clipName(c.name);
        if (clipName.containsIgnoreCase("sustain") ||
            clipName.containsIgnoreCase("cc64")) {
          s.renderMode = LineClipRenderMode::gateBlock;
          s.endBehavior = LineClipEndBehavior::autoReturnToNeutral;
          s.defaultNormValue = 0.0f;
          s.boundary.startValue = s.defaultNormValue;
          s.boundary.endValue = s.defaultNormValue;
          s.points = {{0.0f, 1.0f, 0.5f, false}, {1.0f, 1.0f, 0.5f, false}};
        } else if (clipName.containsIgnoreCase("pitch") ||
                   clipName.containsIgnoreCase("bend")) {
          s.renderMode = LineClipRenderMode::continuousCurve;
          s.endBehavior = LineClipEndBehavior::autoReturnToNeutral;
          s.defaultNormValue = 0.5f;
          s.boundary.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
          s.boundary.startValue = s.defaultNormValue;
          s.boundary.endValue = s.defaultNormValue;
          s.minDisplayValue = -12.0f;
          s.maxDisplayValue = 12.0f;
          s.valueUnit = " st";
          s.points = {{0.08f, 0.50f, 0.5f, false},
                      {0.45f, 0.78f, 0.35f, false},
                      {0.92f, 0.50f, 0.5f, false}};
        } else {
          s.renderMode = LineClipRenderMode::continuousCurve;
          s.endBehavior = LineClipEndBehavior::holdLastOrSilent;
          s.defaultNormValue = 0.0f;
          s.boundary.startValue = s.defaultNormValue;
          s.boundary.endValue = s.defaultNormValue;
          s.points = {{0.0f, 0.20f, 0.5f, false},
                      {0.50f, 0.85f, 0.35f, false},
                      {1.0f, 0.30f, 0.5f, false}};
        }

        syncLineWarpNorms(s, c.length, s.sourceOffsetSec);
        uiStates[c.id] = std::move(s);
      } else {
        syncLineWarpNorms(uiStates[c.id], c.length,
                          uiStates[c.id].sourceOffsetSec);
      }
    }

    for (auto i = uiStates.begin(); i != uiStates.end();)
      if (!live.contains(i->first) && (!panelView.enabled || !panelView.containsCanonicalClip(i->first)))
        i = uiStates.erase(i);
      else
        ++i;
    for (auto i = clipComponents.begin(); i != clipComponents.end();)
      if (!live.contains(i->first))
        i = clipComponents.erase(i);
      else
        ++i;

    layoutClipComponents();
    repaint();
  }

  juce::Rectangle<float> clipBounds(const Clip &c, double pps) const {
    const float topMargin = clipFeatures.warpEnabled ? 12.0f : 4.0f;
    const float bottomMargin = clipFeatures.cutEnabled ? 14.0f : 4.0f;
    const float bodyHeight = juce::jmax(10.0f, static_cast<float>(height) -
                                                   (topMargin + bottomMargin));
    return {static_cast<float>(panelView.timeToX(c.start,getWidth())), topMargin,
            static_cast<float>(c.length * pps), bodyHeight};
  }

  void resized() override { layoutClipComponents(); }

  void paint(juce::Graphics &g) override {
    const auto pps = panelView.pixelsPerSecond(getWidth());
    auto &laf = host.look();
    panelView.paintGrid(g,laf,getLocalBounds(),pps);

    if (data.clips.empty()) {
      g.setColour(laf.colours().secondary);
      g.setFont(juce::Font(juce::FontOptions().withHeight(12)));
      g.drawText("LineClip Lane (CC / ModWheel / Sustain / Parametri)", 25, 35,
                 getWidth() - 40, 20, juce::Justification::centredLeft);
    }
  }

  void paintOverChildren(juce::Graphics &g) override {
    if (loopPreview.active && loopPreview.duplicateCount > 0 &&
        !draggedClipId.empty()) {
      const auto pps = panelView.pixelsPerSecond(getWidth());
      const Clip *cl = nullptr;
      for (const auto &c : data.clips)
        if (c.id == draggedClipId) {
          cl = &c;
          break;
        }
      if (cl != nullptr) {
        const auto origBounds = clipBounds(*cl, pps);
        const auto tint = juce::Colour(data.colour);
        for (int k = 0; k < loopPreview.duplicateCount; ++k) {
          const auto dupBounds = clips::computeLoopDuplicateBounds(
              origBounds, loopPreview.isRightEdge, k);
          clips::drawLoopDuplicatePreview(
              g, dupBounds, cl->name.empty() ? cl->id : cl->name, k + 1,
              loopPreview.isRightEdge, tint, nullptr);
        }
      }
    }

    marqueeSelector.drawMarquee(g);
  }

private:
  struct OverlapHitInfo {
    bool inOverlap = false;
    bool inCrossfade = false;
    std::string leftClipId;
    std::string rightClipId;
    double startSec = 0.0;
    double endSec = 0.0;
    bool isSubsumed = false;
    std::string partnerClipId;
  };

  [[nodiscard]] std::vector<const Clip *> getClipsSortedByZOrder() const {
    std::vector<const Clip *> sorted;
    sorted.reserve(data.clips.size());
    for (const auto &c : data.clips)
      sorted.push_back(&c);
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Clip *a, const Clip *b) {
                       if (std::abs(a->start - b->start) > 1.0e-4)
                         return a->start < b->start;
                       return a->length > b->length;
                     });
    return sorted;
  }

  [[nodiscard]] OverlapHitInfo
  findOverlapAtTime(double timeSec, const std::string &refClipId = {}) const {
    const auto sorted = getClipsSortedByZOrder();
    for (size_t i = 0; i + 1 < sorted.size(); ++i) {
      for (size_t j = i + 1; j < sorted.size(); ++j) {
        const auto *cA = sorted[i];
        const auto *cB = sorted[j];
        const auto *left = (cA->start <= cB->start) ? cA : cB;
        const auto *right = (cA->start <= cB->start) ? cB : cA;
        const double ovStart = std::max(left->start, right->start);
        const double ovEnd =
            std::min(left->start + left->length, right->start + right->length);
        if (ovEnd > ovStart + 1.0e-3 && timeSec >= ovStart - 1.0e-3 &&
            timeSec <= ovEnd + 1.0e-3) {
          if (!refClipId.empty() && left->id != refClipId &&
              right->id != refClipId)
            continue;
          const bool isSubsumed = clips::isClipSubInterval(
              left->start, left->length, right->start, right->length);
          const bool inXfade = xfadePairs.contains({left->id, right->id});
          const std::string partner =
              (left->id == refClipId) ? right->id : left->id;
          return {true,    inXfade, left->id,   right->id,
                  ovStart, ovEnd,   isSubsumed, partner};
        }
      }
    }
    return {};
  }

  [[nodiscard]] OverlapHitInfo
  findOverlapForTopClip(const std::string &topClipId) const {
    const auto sorted = getClipsSortedByZOrder();
    for (size_t i = 0; i + 1 < sorted.size(); ++i) {
      for (size_t j = i + 1; j < sorted.size(); ++j) {
        const auto *cA = sorted[i];
        const auto *cB = sorted[j];
        if (cB->id != topClipId)
          continue;
        const auto *left = (cA->start <= cB->start) ? cA : cB;
        const auto *right = (cA->start <= cB->start) ? cB : cA;
        const double ovStart = std::max(left->start, right->start);
        const double ovEnd =
            std::min(left->start + left->length, right->start + right->length);
        if (ovEnd > ovStart + 1.0e-3) {
          const bool isSubsumed = clips::isClipSubInterval(
              left->start, left->length, right->start, right->length);
          const bool inXfade = xfadePairs.contains({left->id, right->id});
          return {true,    inXfade, left->id,   right->id,
                  ovStart, ovEnd,   isSubsumed, cA->id};
        }
      }
    }
    return {};
  }

  void syncActiveLineCrossfades(const std::string &modifiedClipId = {},
                                bool snapEndpointsToOverlap = true) {
    const auto sorted = getClipsSortedByZOrder();
    std::set<std::pair<std::string, std::string>> validPairs;

    for (size_t i = 0; i + 1 < sorted.size(); ++i) {
      for (size_t j = i + 1; j < sorted.size(); ++j) {
        const auto *left = sorted[i];
        const auto *right = sorted[j];
        const double leftEnd = left->start + left->length;
        const double rightEnd = right->start + right->length;
        const double ovStart = std::max(left->start, right->start);
        const double ovEnd = std::min(leftEnd, rightEnd);
        const std::pair<std::string, std::string> key{left->id, right->id};

        if (ovEnd <= ovStart + 0.01 || !uiStates.contains(left->id) ||
            !uiStates.contains(right->id))
          continue;

        auto &leftUi = uiStates[left->id];
        auto &rightUi = uiStates[right->id];
        if (leftUi.points.size() < 2 || rightUi.points.size() < 2)
          continue;

        const bool isContained = (left->start < right->start - 0.001) &&
                                 (rightEnd < leftEnd - 0.001);
        const bool hasManualFades =
            (rightUi.points.front().normX > 0.005f) ||
            (isContained && rightUi.points.back().normX < 0.995f);

        if (xfadePairs.contains(key) || hasManualFades) {
          if (xfadePairs.contains(key))
            validPairs.insert(key);

          if (isContained) {
            rightUi.boundary.mode =
                ayra::curves::CurveBoundaryMode::fadeEndpoints;
            if (snapEndpointsToOverlap && xfadePairs.contains(key)) {
              if (rightUi.points.front().normX <= 0.01f)
                rightUi.points.front().normX = 0.22f;
              if (rightUi.points.back().normX >= 0.99f)
                rightUi.points.back().normX = 0.78f;
            }
            const float leftNorm0 =
                juce::jlimit(0.0f, 1.0f,
                             static_cast<float>((ovStart - left->start) /
                                                std::max(0.05, left->length)));
            const float leftNorm1 =
                juce::jlimit(0.0f, 1.0f,
                             static_cast<float>((ovEnd - left->start) /
                                                std::max(0.05, left->length)));
            rightUi.boundary.startValue = clips::evaluateAutomationCurveAt(
                leftUi.points.data(), static_cast<int>(leftUi.points.size()),
                leftUi.defaultNormValue, leftUi.boundary, leftNorm0);
            rightUi.boundary.endValue = clips::evaluateAutomationCurveAt(
                leftUi.points.data(), static_cast<int>(leftUi.points.size()),
                leftUi.defaultNormValue, leftUi.boundary, leftNorm1);
          } else {
            leftUi.boundary.mode =
                ayra::curves::CurveBoundaryMode::fadeEndpoints;
            rightUi.boundary.mode =
                ayra::curves::CurveBoundaryMode::fadeEndpoints;

            const float leftMinX =
                leftUi.points.size() > 2
                    ? (leftUi.points[leftUi.points.size() - 2].normX + 0.01f)
                    : 0.02f;
            const float rightMaxX = rightUi.points.size() > 2
                                        ? (rightUi.points[1].normX - 0.01f)
                                        : 0.98f;

            leftUi.points.back().normX =
                juce::jlimit(leftMinX, 0.99f,
                             static_cast<float>((ovStart - left->start) /
                                                std::max(0.05, left->length)));
            if (snapEndpointsToOverlap)
              rightUi.points.front().normX = juce::jlimit(
                  0.01f, rightMaxX,
                  static_cast<float>((ovEnd - right->start) /
                                     std::max(0.05, right->length)));

            leftUi.boundary.endValue = rightUi.points.front().normValue;
            rightUi.boundary.startValue = leftUi.points.back().normValue;

            if (modifiedClipId == left->id)
              rightUi.boundary.fadeInControl = leftUi.boundary.fadeOutControl;
            else if (modifiedClipId == right->id)
              leftUi.boundary.fadeOutControl = rightUi.boundary.fadeInControl;
          }
        }
      }
    }

    for (const auto &oldPair : xfadePairs) {
      // Offscreen is not deletion: keep the relation and endpoint state.
      if(panelView.enabled&&panelView.containsCanonicalClip(oldPair.first)&&panelView.containsCanonicalClip(oldPair.second)
         &&(findClip(oldPair.first)==nullptr||findClip(oldPair.second)==nullptr))
      {validPairs.insert(oldPair);continue;}
      if (!validPairs.contains(oldPair)) {
        if (uiStates.contains(oldPair.first) &&
            !uiStates[oldPair.first].points.empty()) {
          uiStates[oldPair.first].boundary.mode =
              ayra::curves::CurveBoundaryMode::pinnedEndpoints;
          uiStates[oldPair.first].points.back().normX = 1.0f;
        }
        if (uiStates.contains(oldPair.second) &&
            !uiStates[oldPair.second].points.empty()) {
          uiStates[oldPair.second].boundary.mode =
              ayra::curves::CurveBoundaryMode::pinnedEndpoints;
          uiStates[oldPair.second].points.front().normX = 0.0f;
          uiStates[oldPair.second].points.back().normX = 1.0f;
        }
      }
    }
    xfadePairs = std::move(validPairs);
  }

  [[nodiscard]] LineClipVisual
  makeLineClipVisual(const std::string &clipId,
                     const juce::Rectangle<float> localBodyBounds,
                     const juce::Point<float> parentOrigin) override {
    LineClipVisual visual;
    visual.bounds = localBodyBounds;

    const auto clip = std::find_if(
        data.clips.begin(), data.clips.end(),
        [&clipId](const Clip &candidate) { return candidate.id == clipId; });
    const auto state = uiStates.find(clipId);
    if (clip == data.clips.end() || state == uiStates.end())
      return visual;

    auto &ui = state->second;
    syncLineWarpNorms(ui, clip->length, ui.sourceOffsetSec);

    visual.title = juce::String::fromUTF8(clip->name.c_str());
    visual.tint = juce::Colour(data.colour);
    visual.curveColour = curveColour;
    visual.curveUnderlayStyle = curveUnderlayStyle;
    visual.selected = (clipId == host.store().state().selectedClip);
    visual.suspended = !programAudible;
    visual.boundaryPolicy = ui.boundary.mode;
    visual.boundaryStartNormValue = static_cast<float>(ui.boundary.startValue);
    visual.boundaryEndNormValue = static_cast<float>(ui.boundary.endValue);
    visual.fadeInControl = ui.boundary.fadeInControl;
    visual.fadeOutControl = ui.boundary.fadeOutControl;
    visual.renderMode = ui.renderMode;
    visual.endBehavior = ui.endBehavior;
    visual.defaultNormValue = ui.defaultNormValue;
    visual.minDisplayValue = ui.minDisplayValue;
    visual.maxDisplayValue = ui.maxDisplayValue;
    visual.valueUnit = ui.valueUnit;

    visual.points = ui.points.data();
    visual.numPoints = static_cast<int>(ui.points.size());

    visual.warpMarkersNorm = ui.warpMarkers.data();
    visual.origWarpMarkersNorm = ui.origWarpMarkers.data();
    visual.numWarpMarkers = static_cast<int>(ui.warpMarkers.size());
    visual.clipLengthSec = clip->length;

    visual.stretchDeltaTicks = ui.stretchTicks;
    visual.isStretchActive = ui.isStretching;
    visual.slipDeltaTicks = ui.slipDeltaTicks;
    visual.isSlipActive = ui.isSlipping;

    const bool isDraggedClip = (draggedClipId == clipId);
    const bool isDraggingWarp =
        isDraggedClip && (dragZone == TimelineClipZone::warpMarker ||
                          dragZone == TimelineClipZone::topWarpRail);
    const bool isDraggingEnv =
        isDraggedClip &&
        (dragZone == TimelineClipZone::gainEnvelopePoint ||
         dragZone == TimelineClipZone::gainEnvelopeLine ||
         dragZone == TimelineClipZone::gainSegmentCurveHandle ||
         dragZone == TimelineClipZone::fadeInCurveHandle ||
         dragZone == TimelineClipZone::fadeOutCurveHandle ||
         dragZone == TimelineClipZone::gainPointDashedLine);

    visual.features =
        computeClipFeaturesLOD(clipFeatures, localBodyBounds.getHeight(),
                               isDraggingWarp, isDraggingEnv);

    if (hoveredClipId == clipId ||
        (dragZone != TimelineClipZone::none && draggedClipId == clipId)) {
      const float localMouseX = mousePos.x - parentOrigin.x;
      visual.hoveredZone =
          (dragZone != TimelineClipZone::none && draggedClipId == clipId)
              ? dragZone
              : activeZone;
      visual.hoveredMarkerIdx =
          (dragZone != TimelineClipZone::none && draggedClipId == clipId)
              ? draggedMarkerIdx
              : hoveredMarkerIdx;
      visual.hoveredPointIdx =
          (dragZone != TimelineClipZone::none && draggedClipId == clipId)
              ? draggedPointIdx
              : hoveredPointIdx;
      visual.hoveredCurveSegmentIdx =
          (dragZone != TimelineClipZone::none && draggedClipId == clipId)
              ? draggedCurveSegmentIdx
              : hoveredCurveSegmentIdx;
      visual.hoverMouseX = localMouseX;
      visual.showWarpAddGhost = visual.features.showWarpRail &&
                                activeZone == TimelineClipZone::topWarpRail &&
                                hoveredMarkerIdx < 0;
      visual.warpAddGhostX = localMouseX;
      visual.isDraggingAllPoints = false;
    }

    const auto ovInfo = findOverlapForTopClip(clipId);
    if (ovInfo.inOverlap && clip->length > 0.001) {
      visual.overlap.hasOverlap = true;
      visual.overlap.isTopClip = true;
      visual.overlap.startNormX = juce::jlimit(
          0.0f, 1.0f,
          static_cast<float>((ovInfo.startSec - clip->start) / clip->length));
      visual.overlap.endNormX = juce::jlimit(
          0.0f, 1.0f,
          static_cast<float>((ovInfo.endSec - clip->start) / clip->length));
      visual.overlap.partnerTint = visual.tint;

      const auto leftClipIt = std::find_if(
          data.clips.begin(), data.clips.end(),
          [&ovInfo](const Clip &c) { return c.id == ovInfo.leftClipId; });
      if (leftClipIt != data.clips.end() &&
          uiStates.contains(ovInfo.leftClipId) && leftClipIt->length > 0.001) {
        const auto &leftUi = uiStates.at(ovInfo.leftClipId);
        const bool isContained =
            (leftClipIt->start < clip->start - 0.001) &&
            ((clip->start + clip->length) <
             (leftClipIt->start + leftClipIt->length) - 0.001);
        visual.overlap.isContainedInPartner = isContained;

        const float leftNorm0 = juce::jlimit(
            0.0f, 1.0f,
            static_cast<float>((ovInfo.startSec - leftClipIt->start) /
                               leftClipIt->length));
        const float leftNorm1 = juce::jlimit(
            0.0f, 1.0f,
            static_cast<float>((ovInfo.endSec - leftClipIt->start) /
                               leftClipIt->length));
        const float val0 = clips::evaluateAutomationCurveAt(
            leftUi.points.data(), static_cast<int>(leftUi.points.size()),
            leftUi.defaultNormValue, leftUi.boundary, leftNorm0);
        const float val1 = clips::evaluateAutomationCurveAt(
            leftUi.points.data(), static_cast<int>(leftUi.points.size()),
            leftUi.defaultNormValue, leftUi.boundary, leftNorm1);

        if (isContained) {
          const bool hasLeft =
              ovInfo.inCrossfade ||
              (!ui.points.empty() && ui.points.front().normX > 0.005f);
          const bool hasRight =
              ovInfo.inCrossfade ||
              (!ui.points.empty() && ui.points.back().normX < 0.995f);
          visual.overlap.isCrossfade = hasLeft || hasRight;
          visual.overlap.hasLeftCrossfade = hasLeft;
          visual.overlap.hasRightCrossfade = hasRight;
          visual.overlap.showPartnerCurve = !(hasLeft || hasRight);
          visual.overlap.showPartnerDotOnly = (hasLeft || hasRight);
          visual.overlap.partnerStartNormY = val0;
          visual.overlap.partnerEndNormY = val1;
          visual.overlap.partnerRightEndNormY = val1;
          visual.overlap.showStepJumpAtStart = !(hasLeft || hasRight);
          visual.overlap.topClipStartNormY = ui.points.empty()
                                                 ? ui.defaultNormValue
                                                 : ui.points.front().normValue;
          visual.overlap.topClipEndNormY = ui.points.empty()
                                               ? ui.defaultNormValue
                                               : ui.points.back().normValue;
        } else {
          const bool hasXFade =
              ovInfo.inCrossfade ||
              (!ui.points.empty() && ui.points.front().normX > 0.005f);
          visual.overlap.isCrossfade = hasXFade;
          if (hasXFade && !leftUi.points.empty()) {
            const double leftPtSec =
                leftClipIt->start +
                static_cast<double>(leftUi.points.back().normX) *
                    leftClipIt->length;
            visual.overlap.partnerFadeNormX = juce::jlimit(
                0.0f, 1.0f,
                static_cast<float>((leftPtSec - clip->start) / clip->length));
            visual.overlap.showPartnerCurve = false;
            visual.overlap.showPartnerDotOnly = true;
            visual.overlap.partnerStartNormY = leftUi.points.back().normValue;
            visual.overlap.partnerPointSelected = leftUi.points.back().selected;
            visual.overlap.partnerPointHovered =
                (hoveredClipId == ovInfo.leftClipId &&
                 (activeZone == TimelineClipZone::gainEnvelopePoint ||
                  activeZone == TimelineClipZone::gainPointDashedLine) &&
                 hoveredPointIdx == static_cast<int>(leftUi.points.size()) - 1);
          } else {
            visual.overlap.showPartnerCurve = true;
            visual.overlap.partnerStartNormY = val0;
            visual.overlap.partnerEndNormY = val1;
            visual.overlap.showStepJumpAtStart = true;
            visual.overlap.topClipStartNormY =
                ui.points.empty() ? ui.defaultNormValue
                                  : ui.points.front().normValue;
          }
        }
      }
    }

    return visual;
  }

  void layoutClipComponents() {
    if (getWidth() <= 0)
      return;

    const auto pixelsPerSecond = panelView.pixelsPerSecond(getWidth());
    const float topOverflow = clipFeatures.warpEnabled ? 8.0f : 0.0f;
    const float bottomOverflow = clipFeatures.cutEnabled ? 6.0f : 0.0f;

    syncActiveLineCrossfades(draggedClipId);
    for (const auto *clipPtr : getClipsSortedByZOrder()) {
      const auto &clip = *clipPtr;
      if (const auto component = clipComponents.find(clip.id);
          component != clipComponents.end()) {
        component->second->toFront(false);
        component->second->setDecorationOverflows(topOverflow, bottomOverflow,
                                                  7.0f);
        component->second->setBodyBoundsInParent(
            clipBounds(clip, pixelsPerSecond));
      }
    }
  }

  struct HitResult {
    const Clip *clip = nullptr;
    TimelineClipZone zone = TimelineClipZone::none;
    int markerIdx = -1;
    int pointIdx = -1;
    int curveSegmentIdx = -1;
    juce::Rectangle<float> bounds;
  };

  HitResult hitTestZones(juce::Point<float> p) const {
    HitResult res;
    const auto pps = panelView.pixelsPerSecond(getWidth());
    const bool isDraggingWarp = (activeZone == TimelineClipZone::warpMarker ||
                                 activeZone == TimelineClipZone::topWarpRail);
    const bool isDraggingEnv =
        (activeZone == TimelineClipZone::gainEnvelopePoint ||
         activeZone == TimelineClipZone::gainEnvelopeLine ||
         activeZone == TimelineClipZone::gainSegmentCurveHandle ||
         activeZone == TimelineClipZone::fadeInCurveHandle ||
         activeZone == TimelineClipZone::fadeOutCurveHandle ||
         activeZone == TimelineClipZone::gainPointDashedLine);

    const auto sortedClips = getClipsSortedByZOrder();
    for (auto it = sortedClips.rbegin(); it != sortedClips.rend(); ++it) {
      const auto &c = **it;
      const auto b = clipBounds(c, pps);
      const auto feat = computeClipFeaturesLOD(clipFeatures, b.getHeight(),
                                               isDraggingWarp, isDraggingEnv);

      if (p.x >= b.getX() - 6.0f && p.x <= b.getRight() + 6.0f) {
        if (feat.showCutRail && p.y >= b.getBottom() &&
            p.y <= b.getBottom() + 8.0f) {
          res.clip = &c;
          res.bounds = b;
          res.zone = TimelineClipZone::bottomCutRail;
          return res;
        }

        // B) Top Warp Rail / Header Top Area
        if (p.y >= 0.0f && p.y < b.getY()) {
          res.clip = &c;
          res.bounds = b;
          res.zone = feat.showWarpRail ? TimelineClipZone::topWarpRail
                                       : TimelineClipZone::clipTitleMove;

          if (feat.showWarpRail) {
            const auto &ui = uiStates.at(c.id);
            for (int m = 0; m < static_cast<int>(ui.warpMarkers.size()); ++m) {
              const float curNorm = ui.warpMarkers[static_cast<size_t>(m)];
              if (curNorm < -0.001f || curNorm > 1.001f)
                continue;
              const float wx = b.getX() + curNorm * b.getWidth();
              if (std::abs(p.x - wx) <= 8.0f) {
                res.zone = TimelineClipZone::warpMarker;
                res.markerIdx = m;
                break;
              }
            }
          }
          return res;
        }

        if (p.y >= b.getY() && p.y < b.getBottom()) {
          res.clip = &c;
          res.bounds = b;
          const auto &ui = uiStates.at(c.id);

          constexpr float kTitleAreaHeight = 16.0f;
          const float bodyH =
              juce::jmax(10.0f, b.getHeight() - kTitleAreaHeight);
          const juce::Rectangle<float> curveBounds(
              b.getX(), b.getBottom() - bodyH, b.getWidth(), bodyH);
          auto normToY = [&](float normVal) -> float {
            return b.getBottom() - juce::jlimit(0.0f, 1.0f, normVal) * bodyH;
          };

          // 1. Edge Tools (4 quadranti di 1/4 ciascuno: Loop / Slip / Trim /
          // Stretch)
          if (p.x <= b.getX() + 2.5f) {
            const float relY = (p.y - b.getY()) / b.getHeight();
            res.zone = clips::computeClipEdgeZone(true, relY);
            return res;
          }
          if (p.x >= b.getRight() - 2.5f) {
            const float relY = (p.y - b.getY()) / b.getHeight();
            res.zone = clips::computeClipEdgeZone(false, relY);
            return res;
          }

          // 2. Titolo superiore
          if (p.y <= b.getY() + kTitleAreaHeight) {
            res.zone = TimelineClipZone::clipTitleMove;
            return res;
          }

          // 3. Punti e segmenti della curva
          if (feat.showGainEnvelope) {
            const auto &pts = ui.points;

            const auto boundaryGeometry =
                clips::computeAutomationBoundaryGeometry(
                    curveBounds, pts.data(), static_cast<int>(pts.size()),
                    ui.boundary);
            if (boundaryGeometry.hasFadeIn &&
                boundaryGeometry.fadeIn.control.getDistanceFrom(p) <= 8.5f) {
              res.zone = TimelineClipZone::fadeInCurveHandle;
              return res;
            }
            if (boundaryGeometry.hasFadeOut &&
                boundaryGeometry.fadeOut.control.getDistanceFrom(p) <= 8.5f) {
              res.zone = TimelineClipZone::fadeOutCurveHandle;
              return res;
            }

            // Rilevamento della curva partner visibile nell'area di overlap
            // della top clip
            const auto ovInfo = findOverlapForTopClip(c.id);
            if (ovInfo.inCrossfade && !ovInfo.leftClipId.empty()) {
              const auto leftClipIt = std::find_if(
                  data.clips.begin(), data.clips.end(),
                  [&](const Clip &clp) { return clp.id == ovInfo.leftClipId; });
              const auto leftUiIt = uiStates.find(ovInfo.leftClipId);
              if (leftClipIt != data.clips.end() &&
                  leftUiIt != uiStates.end() &&
                  !leftUiIt->second.points.empty()) {
                const float cx0 =
                    curveBounds.getX() +
                    juce::jlimit(0.0f, 1.0f,
                                 static_cast<float>(
                                     (ovInfo.startSec - c.start) / c.length)) *
                        curveBounds.getWidth();
                const float cx1 =
                    curveBounds.getX() +
                    juce::jlimit(0.0f, 1.0f,
                                 static_cast<float>((ovInfo.endSec - c.start) /
                                                    c.length)) *
                        curveBounds.getWidth();
                const float py0 =
                    curveBounds.getBottom() -
                    juce::jlimit(0.0f, 1.0f,
                                 leftUiIt->second.points.back().normValue) *
                        curveBounds.getHeight();
                const float py1 = curveBounds.getBottom();
                const auto partnerSeg = makeBoundedCurvePixelSegment(
                    {cx0, py0}, {cx1, py1},
                    leftUiIt->second.boundary.fadeOutControl);

                if (partnerSeg.control.getDistanceFrom(p) <= 8.5f) {
                  res.clip = &(*leftClipIt);
                  res.zone = TimelineClipZone::fadeOutCurveHandle;
                  return res;
                }
              }
            }

            if (pts.size() >= 2 &&
                ui.renderMode == LineClipRenderMode::continuousCurve) {
              for (size_t s = 0; s + 1 < pts.size(); ++s) {
                const auto &n0 = pts[s];
                const auto &n1 = pts[s + 1];
                const auto handle = clips::automationControlPixelPosition(
                    curveBounds, n0, n1, static_cast<int>(s),
                    static_cast<int>(pts.size()));

                if (handle.getDistanceFrom(p) <= 8.5f) {
                  res.zone = TimelineClipZone::gainSegmentCurveHandle;
                  res.curveSegmentIdx = static_cast<int>(s);
                  return res;
                }
              }
            }

            for (int ep = 0; ep < static_cast<int>(pts.size()); ++ep) {
              const auto &pt = pts[static_cast<size_t>(ep)];
              const auto point = clips::automationPointPixelPosition(
                  curveBounds, pt, ep, static_cast<int>(pts.size()));

              if (point.getDistanceFrom(p) <= 8.5f) {
                res.zone = TimelineClipZone::gainEnvelopePoint;
                res.pointIdx = ep;
                return res;
              }
            }

            // Il tratteggio appartiene a ogni nodo che la policy permette
            // di muovere nel tempo, inclusi gli endpoint in modalita' fade.
            for (int ep = 0; ep < static_cast<int>(pts.size()); ++ep) {
              if (!clips::canAutomationPointMoveHorizontally(
                      ui.boundary.mode, ep, static_cast<int>(pts.size())))
                continue;
              const auto &pt = pts[static_cast<size_t>(ep)];
              const float px = clips::automationPointRenderX(
                  curveBounds, ep, static_cast<int>(pts.size()), pt.normX);
              const float py = normToY(pt.normValue);
              if (p.y >= py + 6.0f && p.y <= b.getBottom() - 2.0f &&
                  std::abs(p.x - px) <= 7.0f) {
                res.zone = TimelineClipZone::gainPointDashedLine;
                res.pointIdx = ep;
                return res;
              }
            }

            if (pts.size() >= 2) {
              for (size_t s = 0; s + 1 < pts.size(); ++s) {
                const auto &n0 = pts[s];
                const auto &n1 = pts[s + 1];
                const float x0 = clips::automationPointRenderX(
                    curveBounds, static_cast<int>(s),
                    static_cast<int>(pts.size()), n0.normX);
                const float x1 = clips::automationPointRenderX(
                    curveBounds, static_cast<int>(s + 1),
                    static_cast<int>(pts.size()), n1.normX);
                if (p.x >= x0 && p.x <= x1) {
                  const float span = std::max(0.001f, x1 - x0);
                  const float u = juce::jlimit(0.0f, 1.0f, (p.x - x0) / span);
                  const float expectedY =
                      normToY(clips::evaluateAutomationSegment(
                          n0, n1, n0.normX + u * (n1.normX - n0.normX)));

                  if (std::abs(p.y - expectedY) <= 7.5f) {
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

  void mouseMove(const juce::MouseEvent &e) override {
    mousePos = e.position;
    const auto hit = hitTestZones(e.position);

    if (hit.clip != nullptr) {
      hoveredClipId = hit.clip->id;
      activeZone = hit.zone;
      hoveredMarkerIdx = hit.markerIdx;
      hoveredPointIdx = hit.pointIdx;
      hoveredCurveSegmentIdx = hit.curveSegmentIdx;

      const auto &ui = uiStates.at(hit.clip->id);
      const bool canMoveX = clips::canAutomationPointMoveHorizontally(
          ui.boundary.mode, hoveredPointIdx,
          static_cast<int>(ui.points.size()));
      const bool canErase =
          clips::isClipDeletionZone(activeZone) ||
          activeZone == TimelineClipZone::warpMarker ||
          (activeZone == TimelineClipZone::gainEnvelopePoint &&
           clips::canAutomationPointBeRemoved(
               hoveredPointIdx, static_cast<int>(ui.points.size())));
      setMouseCursor(
          clips::timelineMouseCursor(clips::resolveTimelineCursorRole(
              {activeZone, e.mods.isAltDown(), e.mods.isShiftDown(), canMoveX,
               canErase})));
    } else {
      hoveredClipId.clear();
      activeZone = TimelineClipZone::none;
      hoveredMarkerIdx = -1;
      hoveredPointIdx = -1;
      hoveredCurveSegmentIdx = -1;
      isHoveringDivider = !panelView.enabled && (e.position.y >= getHeight() - 6.0f);
      setMouseCursor(isHoveringDivider ? juce::MouseCursor::UpDownResizeCursor
                                       : juce::MouseCursor::NormalCursor);
    }
    repaint();
  }

  void modifierKeysChanged(const juce::ModifierKeys &modifiers) override {
    const auto active =
        (dragZone != TimelineClipZone::none) ? dragZone : activeZone;
    const auto cid =
        (dragZone != TimelineClipZone::none) ? draggedClipId : hoveredClipId;
    const int ptIdx = (dragZone != TimelineClipZone::none) ? draggedPointIdx
                                                           : hoveredPointIdx;

    bool canMoveX = true;
    bool canErase = clips::isClipDeletionZone(active) ||
                    active == TimelineClipZone::warpMarker;
    if (uiStates.contains(cid)) {
      const auto &ui = uiStates.at(cid);
      canMoveX = clips::canAutomationPointMoveHorizontally(
          ui.boundary.mode, ptIdx, static_cast<int>(ui.points.size()));
      canErase = canErase || (active == TimelineClipZone::gainEnvelopePoint &&
                              clips::canAutomationPointBeRemoved(
                                  ptIdx, static_cast<int>(ui.points.size())));
    }
    setMouseCursor(clips::timelineMouseCursor(clips::resolveTimelineCursorRole(
        {active, modifiers.isAltDown(), modifiers.isShiftDown(), canMoveX,
         canErase})));
  }

  void mouseExit(const juce::MouseEvent &) override {
    hoveredClipId.clear();
    activeZone = TimelineClipZone::none;
    hoveredMarkerIdx = -1;
    hoveredPointIdx = -1;
    hoveredCurveSegmentIdx = -1;
    isHoveringDivider = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const auto hit = hitTestZones(e.position);

    if (hit.clip != nullptr && e.mods.isPopupMenu()) {
      auto context = clips::makeTimelinePointerContext(
          data.id, hit.clip->id, clips::TimelinePointerTarget::clip,
          clips::ClipKind::lineClip, clips::semanticClipZone(hit.zone),
          e.position,
          panelView.xToTime(e.position.x,getWidth()), e.mods);
      context.markerIndex = hit.markerIdx;
      context.pointIndex = hit.pointIdx;
      context.segmentIndex = hit.curveSegmentIdx;
      const auto ovInfo =
          findOverlapAtTime(context.timelineSeconds, hit.clip->id);
      if (ovInfo.inOverlap) {
        context.isInClipOverlap = true;
        context.isInCrossfade = ovInfo.inCrossfade;
        context.overlappedPartnerClipId = (ovInfo.leftClipId == hit.clip->id)
                                              ? ovInfo.rightClipId
                                              : ovInfo.leftClipId;
        context.overlapStartSeconds = ovInfo.startSec;
        context.overlapEndSeconds = ovInfo.endSec;
      }
      host.requestTimelineContextMenu(context);
      return;
    }

    if (hit.clip != nullptr && e.mods.isAltDown() &&
        e.mods.isLeftButtonDown() && clips::isClipDeletionZone(hit.zone)) {
      std::ignore = host.deleteClip(data.id, hit.clip->id);
      return;
    }

    if (!hit.clip || hit.zone == TimelineClipZone::none) {
      if (e.mods.isLeftButtonDown() && e.position.y >= getHeight() - 6.0f) {
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
    draggedPointIdx = hit.pointIdx;
    draggedCurveSegmentIdx = hit.curveSegmentIdx;
    dragStartPos = e.position;

    auto &ui = uiStates[draggedClipId];

    // Snapshot completo per rollback deterministico tramite tasto Esc
    hasDragSnapshot = true;
    snapshotUIState = ui;
    startClipStart = hit.clip->start;
    startClipLength = hit.clip->length;

    if (e.mods.isAltDown() && !e.mods.isPopupMenu()) {
      if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 &&
          hit.markerIdx < static_cast<int>(ui.warpPoints.size())) {
        ui.warpPoints.erase(ui.warpPoints.begin() + hit.markerIdx);
        syncLineWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
        resetDragState();
        repaint();
        return;
      }
      if (hit.zone == TimelineClipZone::gainEnvelopePoint) {
        if (clips::canAutomationPointBeRemoved(
                hit.pointIdx, static_cast<int>(ui.points.size()))) {
          ui.points.erase(ui.points.begin() + hit.pointIdx);
        }
        resetDragState();
        repaint();
        return;
      }
    }

    if (!e.mods.isLeftButtonDown()) {
      resetDragState();
      return;
    }

    if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.pointIdx >= 0 &&
        hit.pointIdx < static_cast<int>(ui.points.size())) {
      if (e.mods.isShiftDown())
        ui.points[static_cast<size_t>(hit.pointIdx)].selected = true;
      else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        ui.points[static_cast<size_t>(hit.pointIdx)].selected =
            !ui.points[static_cast<size_t>(hit.pointIdx)].selected;
      else if (!ui.points[static_cast<size_t>(hit.pointIdx)].selected) {
        for (auto &pt : ui.points)
          pt.selected = false;
        ui.points[static_cast<size_t>(hit.pointIdx)].selected = true;
      }
    }

    if (hit.zone == TimelineClipZone::clipTitleMove)
      return;

    if (dragZone == TimelineClipZone::topWarpRail && hit.markerIdx < 0) {
      const float normX = juce::jlimit(0.01f, 0.99f,
                                       (e.position.x - hit.bounds.getX()) /
                                           hit.bounds.getWidth());
      const double sampleTime =
          (ui.isStretching || ui.stretchTicks != 0)
              ? (ui.startSourceOffsetSec +
                 static_cast<double>(normX) * ui.origClipLength)
              : (ui.sourceOffsetSec +
                 static_cast<double>(normX) * hit.clip->length);

      const double minDistance = std::max(0.005, hit.clip->length * 0.015);
      const auto dup = std::find_if(
          ui.warpPoints.begin(), ui.warpPoints.end(),
          [sampleTime, minDistance](const LineWarpPoint &m) {
            return std::abs(m.sourceTimeSec - sampleTime) < minDistance;
          });
      if (dup == ui.warpPoints.end()) {
        ui.warpPoints.push_back({sampleTime, sampleTime});
        std::sort(ui.warpPoints.begin(), ui.warpPoints.end(),
                  [](const LineWarpPoint &a, const LineWarpPoint &b) {
                    return a.sourceTimeSec < b.sourceTimeSec;
                  });
        syncLineWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
      }
      resetDragState();
      repaint();
      return;
    }

    if (dragZone == TimelineClipZone::bottomCutRail) {
      const auto pps = panelView.pixelsPerSecond(getWidth());
      const double splitTime = panelView.xToTime(e.position.x,getWidth());
      constexpr double minimumSegmentLength = 0.2;
      std::vector<std::pair<std::string, float>> clipsToSplit;
      for (const auto &candidate : data.clips) {
        const double rel = splitTime - candidate.start;
        if (rel > minimumSegmentLength &&
            rel < candidate.length - minimumSegmentLength)
          clipsToSplit.emplace_back(candidate.id,
                                    static_cast<float>(rel / candidate.length));
      }
      for (const auto &[targetId, splitFrac] : clipsToSplit) {
        const auto newId = host.splitClip(data.id, targetId, splitTime);
        if (!newId.empty())
          splitLineClipUI(targetId, newId, splitFrac);
      }
      syncActiveLineCrossfades();
      resetDragState();
      repaint();
      return;
    }

    if (e.mods.isShiftDown() &&
        hit.zone == TimelineClipZone::gainEnvelopeLine) {
      draggedCurveSegmentIdx = clips::pinAutomationSegmentForVerticalDrag(
          ui.points, hit.curveSegmentIdx);
    }

    startEnvelopePoints = ui.points;
    startBoundary = ui.boundary;
    if (ui.origClipLength <= 0.001)
      ui.origClipLength = hit.clip->length;
    if (ui.sourceTotalDurationSec <= 0.001) {
      ui.sourceTotalDurationSec = hit.clip->length + 8.0;
      ui.sourceOffsetSec = 4.0;
    }
    startSourceOffsetSec = ui.sourceOffsetSec;

    loopPreview = {};
    if (hit.zone == TimelineClipZone::edgeLeftLoop ||
        hit.zone == TimelineClipZone::edgeRightLoop) {
      loopPreview.active = true;
      loopPreview.clipId = hit.clip->id;
      loopPreview.isRightEdge = (hit.zone == TimelineClipZone::edgeRightLoop);
      loopPreview.originalStart = startClipStart;
      loopPreview.originalLength = startClipLength;
      loopPreview.duplicateCount = 0;
      loopPreview.trimDelta = 0.0;
    }

    if (hit.zone == TimelineClipZone::edgeLeftStretch ||
        hit.zone == TimelineClipZone::edgeRightStretch) {
      ui.origClipLength = hit.clip->length;
      ui.startSourceOffsetSec = ui.sourceOffsetSec;
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    mousePos = e.position;

    if (isResizingLaneHeight) {
      const int deltaY = e.getScreenY() - resizeLaneStartY;
      const int newH =
          std::clamp(resizeStartLaneHeight + deltaY, minimumHeight(data), 1200);
      if (e.mods.isShiftDown() || e.mods.isAltDown()) {
        const float factor =
            static_cast<float>(newH) /
            static_cast<float>(juce::jmax(1, resizeStartLaneHeight));
        host.adjustVerticalZoom(factor - 1.0f);
      } else {
        host.store().resize(data.id, newH);
      }
      return;
    }

    if (isMarqueeActive) {
      marqueeSelector.handleMouseDrag(e);
      return;
    }

    if (draggedClipId.empty() || !uiStates.contains(draggedClipId)) {
      repaint();
      return;
    }

    auto &ui = uiStates[draggedClipId];
    const auto pps = panelView.pixelsPerSecond(getWidth());
    Clip *cl = nullptr;
    for (auto &c : data.clips)
      if (c.id == draggedClipId) {
        cl = &c;
        break;
      }
    if (!cl)
      return;
    const auto b = clipBounds(*cl, pps);
    constexpr float kTitleAreaHeight = 16.0f;
    const float bodyH = juce::jmax(10.0f, b.getHeight() - kTitleAreaHeight);

    if (dragZone == TimelineClipZone::clipTitleMove) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      cl->start = std::max(0.0, startClipStart + deltaSec);
      layoutClipComponents();
      repaint();
      return;
    }

    if (dragZone == TimelineClipZone::gainPointDashedLine &&
        clips::canAutomationPointMoveHorizontally(
            ui.boundary.mode, draggedPointIdx,
            static_cast<int>(ui.points.size()))) {
      setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
      const float currentNormX =
          juce::jlimit(0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
      const float minX =
          draggedPointIdx > 0
              ? ui.points[static_cast<size_t>(draggedPointIdx - 1)].normX +
                    0.01f
              : 0.0f;
      const float maxX =
          draggedPointIdx + 1 < static_cast<int>(ui.points.size())
              ? ui.points[static_cast<size_t>(draggedPointIdx + 1)].normX -
                    0.01f
              : 1.0f;
      ui.points[static_cast<size_t>(draggedPointIdx)].normX =
          juce::jlimit(minX, maxX, currentNormX);
      repaint();
      return;
    }

    if (dragZone == TimelineClipZone::gainEnvelopeLine &&
        draggedCurveSegmentIdx >= 0) {
      if (draggedCurveSegmentIdx + 1 < static_cast<int>(ui.points.size()) &&
          draggedCurveSegmentIdx + 1 <
              static_cast<int>(startEnvelopePoints.size())) {
        const float deltaNorm = -(e.position.y - dragStartPos.y) / bodyH;
        const size_t s0 = static_cast<size_t>(draggedCurveSegmentIdx);
        const size_t s1 = s0 + 1;
        ui.points[s0].normValue = juce::jlimit(
            0.0f, 1.0f, startEnvelopePoints[s0].normValue + deltaNorm);
        ui.points[s1].normValue = juce::jlimit(
            0.0f, 1.0f, startEnvelopePoints[s1].normValue + deltaNorm);
        repaint();
        return;
      }
    } else if (dragZone == TimelineClipZone::gainEnvelopePoint &&
               draggedPointIdx >= 0 &&
               draggedPointIdx < static_cast<int>(ui.points.size())) {
      const float currentNormY =
          juce::jlimit(0.0f, 1.0f, (b.getBottom() - e.position.y) / bodyH);
      const float currentNormX =
          juce::jlimit(0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
      const float startNormY =
          juce::jlimit(0.0f, 1.0f, (b.getBottom() - dragStartPos.y) / bodyH);
      const float startNormX =
          juce::jlimit(0.0f, 1.0f, (dragStartPos.x - b.getX()) / b.getWidth());

      const float deltaY = currentNormY - startNormY;
      const float deltaX =
          e.mods.isShiftDown() ? 0.0f : (currentNormX - startNormX);

      const bool canMoveX = clips::canAutomationPointMoveHorizontally(
          ui.boundary.mode, draggedPointIdx,
          static_cast<int>(ui.points.size()));
      if (!canMoveX) {
        // In pinnedEndpoints: endpoints cannot move horizontally
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        ui.points[static_cast<size_t>(draggedPointIdx)].normValue =
            currentNormY;
        clips::enforceAutomationBoundaryPositions(
            ui.points.data(), static_cast<int>(ui.points.size()),
            ui.boundary.mode);

        // Notifica il solver di crossfade per ricalcolare la maniglia coniugata
        // della clip partner
        if (draggedPointIdx == 0) {
          const auto partner = findActiveCrossfadeForFadeIn(draggedClipId);
          if (partner.found && uiStates.contains(partner.partnerClipId)) {
            auto &leftUi = uiStates[partner.partnerClipId];
            LineCrossfadeContext ctx;
            ctx.interaction = CrossfadeInteractionKind::levelChange;
            ctx.modifiedClipId = draggedClipId;
            ctx.isLeftClipModified = false;
            ctx.leftClipId = partner.partnerClipId;
            ctx.rightClipId = draggedClipId;
            ctx.currentLeftValue = leftUi.points.back().normValue;
            ctx.currentRightValue = currentNormY;
            ctx.requestedValue = currentNormY;
            ctx.leftFadeOutControl = leftUi.boundary.fadeOutControl;
            ctx.rightFadeInControl = ui.boundary.fadeInControl;
            ctx.overlapDurationSec = partner.overlapDuration;

            const auto res = solveLineCrossfade(ctx);
            ui.boundary.fadeInControl = res.rightFadeInControl;
            leftUi.boundary.fadeOutControl = res.leftFadeOutControl;
          }
        } else if (draggedPointIdx == static_cast<int>(ui.points.size()) - 1) {
          const auto partner = findActiveCrossfadeForFadeOut(draggedClipId);
          if (partner.found && uiStates.contains(partner.partnerClipId)) {
            auto &rightUi = uiStates[partner.partnerClipId];
            LineCrossfadeContext ctx;
            ctx.interaction = CrossfadeInteractionKind::levelChange;
            ctx.modifiedClipId = draggedClipId;
            ctx.isLeftClipModified = true;
            ctx.leftClipId = draggedClipId;
            ctx.rightClipId = partner.partnerClipId;
            ctx.currentLeftValue = currentNormY;
            ctx.currentRightValue = rightUi.points.front().normValue;
            ctx.requestedValue = currentNormY;
            ctx.leftFadeOutControl = ui.boundary.fadeOutControl;
            ctx.rightFadeInControl = rightUi.boundary.fadeInControl;
            ctx.overlapDurationSec = partner.overlapDuration;

            const auto res = solveLineCrossfade(ctx);
            ui.boundary.fadeOutControl = res.leftFadeOutControl;
            rightUi.boundary.fadeInControl = res.rightFadeInControl;
          }
        }

        syncActiveLineCrossfades(draggedClipId, true);
        repaint();
        return;
      }

      if (e.mods.isShiftDown())
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
      else
        setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);

      const bool isMulti =
          ui.points[static_cast<size_t>(draggedPointIdx)].selected;
      if (isMulti) {
        float minimumDeltaX = -1.0f;
        float maximumDeltaX = 1.0f;
        for (size_t k = 0; k < startEnvelopePoints.size(); ++k) {
          if (!startEnvelopePoints[k].selected ||
              !clips::canAutomationPointMoveHorizontally(
                  ui.boundary.mode, static_cast<int>(k),
                  static_cast<int>(startEnvelopePoints.size())))
            continue;

          const float minDelta =
              k == 0 ? -startEnvelopePoints[k].normX
                     : (!startEnvelopePoints[k - 1].selected
                            ? startEnvelopePoints[k - 1].normX + 0.01f -
                                  startEnvelopePoints[k].normX
                            : -1.0f);
          const float maxDelta =
              k + 1 == startEnvelopePoints.size()
                  ? 1.0f - startEnvelopePoints[k].normX
                  : (!startEnvelopePoints[k + 1].selected
                         ? startEnvelopePoints[k + 1].normX - 0.01f -
                               startEnvelopePoints[k].normX
                         : 1.0f);
          minimumDeltaX = std::max(minimumDeltaX, minDelta);
          maximumDeltaX = std::min(maximumDeltaX, maxDelta);
        }
        const float boundedDeltaX =
            juce::jlimit(minimumDeltaX, maximumDeltaX, deltaX);

        for (size_t k = 0;
             k < ui.points.size() && k < startEnvelopePoints.size(); ++k) {
          if (startEnvelopePoints[k].selected) {
            ui.points[k].normValue = juce::jlimit(
                0.0f, 1.0f, startEnvelopePoints[k].normValue + deltaY);
            if (clips::canAutomationPointMoveHorizontally(
                    ui.boundary.mode, static_cast<int>(k),
                    static_cast<int>(ui.points.size()))) {
              ui.points[k].normX = juce::jlimit(
                  0.0f, 1.0f, startEnvelopePoints[k].normX + boundedDeltaX);
            }
          }
        }
      } else {
        const float minX =
            draggedPointIdx > 0
                ? ui.points[static_cast<size_t>(draggedPointIdx - 1)].normX +
                      0.01f
                : 0.0f;
        const float maxX =
            draggedPointIdx + 1 < static_cast<int>(ui.points.size())
                ? ui.points[static_cast<size_t>(draggedPointIdx + 1)].normX -
                      0.01f
                : 1.0f;
        if (!e.mods.isShiftDown())
          ui.points[static_cast<size_t>(draggedPointIdx)].normX =
              juce::jlimit(minX, maxX, currentNormX);
        ui.points[static_cast<size_t>(draggedPointIdx)].normValue =
            currentNormY;
      }
      clips::enforceAutomationBoundaryPositions(
          ui.points.data(), static_cast<int>(ui.points.size()),
          ui.boundary.mode);
      repaint();
      return;
    } else if (dragZone == TimelineClipZone::gainSegmentCurveHandle &&
               draggedCurveSegmentIdx >= 0) {
      if (draggedCurveSegmentIdx < static_cast<int>(ui.points.size()) - 1) {
        auto &start = ui.points[static_cast<size_t>(draggedCurveSegmentIdx)];
        const auto &end =
            ui.points[static_cast<size_t>(draggedCurveSegmentIdx + 1)];
        clips::setAutomationControlPoint(
            start, end,
            juce::jlimit(0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth()),
            juce::jlimit(0.0f, 1.0f, (b.getBottom() - e.position.y) / bodyH));
      }
      repaint();
      return;
    } else if (dragZone == TimelineClipZone::fadeInCurveHandle ||
               dragZone == TimelineClipZone::fadeOutCurveHandle) {
      const juce::Rectangle<float> curveBounds(b.getX(), b.getBottom() - bodyH,
                                               b.getWidth(), bodyH);
      const auto geometry = clips::computeAutomationBoundaryGeometry(
          curveBounds, ui.points.data(), static_cast<int>(ui.points.size()),
          ui.boundary);
      const bool fadeIn = dragZone == TimelineClipZone::fadeInCurveHandle;
      const auto &segment = fadeIn ? geometry.fadeIn : geometry.fadeOut;
      const auto control = ayra::curves::makeBoundedQuadraticControl(
          {segment.start.x, segment.start.y, segment.end.x, segment.end.y},
          e.position.x, e.position.y);

      applyCrossfadeCurvature(draggedClipId, fadeIn, control);
      return;
    }

    // Edge Tools: Loop / Slip / Trim / Time-Stretch
    if (dragZone == TimelineClipZone::edgeLeftLoop) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      if (deltaSec >= 0.0) {
        const double maxDelta = startClipLength - 0.125;
        const double clampedDelta = std::clamp(deltaSec, 0.0, maxDelta);
        cl->start = startClipStart + clampedDelta;
        cl->length = startClipLength - clampedDelta;
        ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
        ui.origClipLength = cl->length;
        ui.stretchTicks = 0;
        ui.isStretching = false;
        loopPreview.duplicateCount = 0;
        loopPreview.trimDelta = clampedDelta;
      } else {
        cl->start = startClipStart;
        cl->length = startClipLength;
        ui.sourceOffsetSec = startSourceOffsetSec;
        ui.origClipLength = cl->length;
        ui.stretchTicks = 0;
        ui.isStretching = false;
        loopPreview.duplicateCount = clips::computeLoopDuplicateCount(
            false, deltaSec, startClipStart, startClipLength);
        loopPreview.trimDelta = 0.0;
      }
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightLoop) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      if (deltaSec <= 0.0) {
        const double minDelta = -(startClipLength - 0.125);
        const double clampedDelta = std::clamp(deltaSec, minDelta, 0.0);
        cl->length = startClipLength + clampedDelta;
        ui.origClipLength = cl->length;
        ui.stretchTicks = 0;
        ui.isStretching = false;
        loopPreview.duplicateCount = 0;
        loopPreview.trimDelta = clampedDelta;
      } else {
        cl->start = startClipStart;
        cl->length = startClipLength;
        ui.origClipLength = cl->length;
        ui.stretchTicks = 0;
        ui.isStretching = false;
        loopPreview.duplicateCount = clips::computeLoopDuplicateCount(
            true, deltaSec, startClipStart, startClipLength);
        loopPreview.trimDelta = 0.0;
      }
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeLeftSlip) {
      // Maniglia 2 (Trim 2) a sinistra: il mouse muove il bordo sinistro,
      // ma sulla curva si lavora come se stessimo trimmando con Trim 1 a
      // destra.
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double clampedDelta =
          std::clamp(deltaSec, -startClipStart, startClipLength - 0.125);
      cl->start = startClipStart + clampedDelta;
      cl->length = startClipLength - clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      ui.isSlipping = true;
      const double T_left = startClipStart;
      const double T_right = startClipStart + cl->length;
      ui.points = clips::trimAutomationPointsWindow(
          startEnvelopePoints, startBoundary, ui.defaultNormValue,
          startClipStart, startClipLength, T_left, T_right);
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightSlip) {
      // Maniglia 2 (Trim 2) a destra: il mouse muove il bordo destro,
      // ma sulla curva si lavora come se stessimo trimmando con Trim 1 a
      // sinistra.
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double clampedDelta =
          std::max(-(startClipLength - 0.125), deltaSec);
      cl->length = startClipLength + clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      ui.isSlipping = true;
      const double T_right = startClipStart + startClipLength;
      const double T_left = T_right - cl->length;
      ui.points = clips::trimAutomationPointsWindow(
          startEnvelopePoints, startBoundary, ui.defaultNormValue,
          startClipStart, startClipLength, T_left, T_right);
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeLeftTrim) {
      // Maniglia 3 (Trim 1) a sinistra: il mouse muove il bordo sinistro
      // e trimma/estende l'estremità sinistra della curva.
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double clampedDelta =
          std::clamp(deltaSec, -startClipStart, startClipLength - 0.125);
      cl->start = startClipStart + clampedDelta;
      cl->length = startClipLength - clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      const double T_right = startClipStart + startClipLength;
      const double T_left = cl->start;
      ui.points = clips::trimAutomationPointsWindow(
          startEnvelopePoints, startBoundary, ui.defaultNormValue,
          startClipStart, startClipLength, T_left, T_right);
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightTrim) {
      // Maniglia 3 (Trim 1) a destra: il mouse muove il bordo destro
      // e trimma/estende l'estremità destra della curva.
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double clampedDelta =
          std::max(-(startClipLength - 0.125), deltaSec);
      cl->length = startClipLength + clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      const double T_left = startClipStart;
      const double T_right = startClipStart + cl->length;
      ui.points = clips::trimAutomationPointsWindow(
          startEnvelopePoints, startBoundary, ui.defaultNormValue,
          startClipStart, startClipLength, T_left, T_right);
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeLeftStretch) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double fixedEnd = startClipStart + startClipLength;
      const double newStart =
          std::clamp(startClipStart + deltaSec, 0.0, fixedEnd - 0.125);
      cl->start = newStart;
      cl->length = fixedEnd - newStart;
      ui.stretchTicks = static_cast<std::int64_t>(
          std::round((cl->length - ui.origClipLength) * 960.0));
      ui.isStretching = true;
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightStretch) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      cl->length = std::max(0.125, startClipLength + deltaSec);
      ui.stretchTicks = static_cast<std::int64_t>(
          std::round((cl->length - ui.origClipLength) * 960.0));
      ui.isStretching = true;
      layoutClipComponents();
      repaint();
    } else if (dragZone == TimelineClipZone::warpMarker &&
               draggedMarkerIdx >= 0) {
      if (draggedMarkerIdx < static_cast<int>(ui.warpPoints.size())) {
        float leftLimit = 0.01f;
        if (draggedMarkerIdx > 0 &&
            draggedMarkerIdx - 1 < static_cast<int>(ui.warpMarkers.size())) {
          const float prevNorm =
              ui.warpMarkers[static_cast<size_t>(draggedMarkerIdx - 1)];
          if (prevNorm >= 0.0f && prevNorm <= 1.0f)
            leftLimit = prevNorm + 0.015f;
        }
        float rightLimit = 0.99f;
        if (draggedMarkerIdx < static_cast<int>(ui.warpPoints.size()) - 1 &&
            draggedMarkerIdx + 1 < static_cast<int>(ui.warpMarkers.size())) {
          const float nextNorm =
              ui.warpMarkers[static_cast<size_t>(draggedMarkerIdx + 1)];
          if (nextNorm >= 0.0f && nextNorm <= 1.0f)
            rightLimit = nextNorm - 0.015f;
        }

        const float prevMarkerNorm =
            ui.warpMarkers[static_cast<size_t>(draggedMarkerIdx)];
        const float rawNorm = (e.position.x - b.getX()) / b.getWidth();
        const float clampedNorm = juce::jlimit(leftLimit, rightLimit, rawNorm);

        if (ui.isStretching || ui.stretchTicks != 0)
          ui.warpPoints[static_cast<size_t>(draggedMarkerIdx)].warpedTimeSec =
              ui.startSourceOffsetSec +
              static_cast<double>(clampedNorm) * ui.origClipLength;
        else
          ui.warpPoints[static_cast<size_t>(draggedMarkerIdx)].warpedTimeSec =
              ui.sourceOffsetSec +
              static_cast<double>(clampedNorm) * cl->length;

        if (std::abs(clampedNorm - prevMarkerNorm) > 0.0001f) {
          auto warpNormCoord = [&](float x) -> float {
            if (x <= leftLimit || x >= rightLimit)
              return x;
            if (x < prevMarkerNorm) {
              const float span = std::max(0.001f, prevMarkerNorm - leftLimit);
              return leftLimit +
                     ((x - leftLimit) / span) * (clampedNorm - leftLimit);
            }
            const float span = std::max(0.001f, rightLimit - prevMarkerNorm);
            return clampedNorm +
                   ((x - prevMarkerNorm) / span) * (rightLimit - clampedNorm);
          };

          for (size_t k = 0; k < ui.points.size(); ++k) {
            if (!clips::canAutomationPointMoveHorizontally(
                    ui.boundary.mode, static_cast<int>(k),
                    static_cast<int>(ui.points.size())))
              continue;
            const float minX = k > 0 ? ui.points[k - 1].normX + 0.01f : 0.0f;
            const float maxX = k + 1 < ui.points.size()
                                   ? ui.points[k + 1].normX - 0.01f
                                   : 1.0f;
            ui.points[k].normX =
                juce::jlimit(minX, maxX, warpNormCoord(ui.points[k].normX));
          }
        }

        syncLineWarpNorms(ui, cl->length, ui.sourceOffsetSec);
        repaint();
      }
    }
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (isResizingLaneHeight) {
      isResizingLaneHeight = false;
      resetDragState();
      return;
    }
    if (isMarqueeActive) {
      marqueeSelector.handleMouseUp(e);
      isMarqueeActive = false;
      resetDragState();
      return;
    }

    if (dragZone == TimelineClipZone::clipTitleMove && !draggedClipId.empty()) {
      for (const auto &c : data.clips)
        if (c.id == draggedClipId) {
          host.moveClip(data.id, draggedClipId, c.start);
          break;
        }
    }

    if (dragZone == TimelineClipZone::edgeLeftLoop ||
        dragZone == TimelineClipZone::edgeRightLoop) {
      if (loopPreview.active && !draggedClipId.empty()) {
        if (loopPreview.duplicateCount > 0) {
          const auto created = host.duplicateClipLoop(
              data.id, draggedClipId, loopPreview.duplicateCount,
              loopPreview.isRightEdge);
          duplicateLineClipLoopUI(draggedClipId, created);
        } else {
          for (const auto &c : data.clips) {
            if (c.id == draggedClipId) {
              host.resizeClip(data.id, c.id, c.start, c.length);
              break;
            }
          }
        }
      }
      loopPreview = {};
    }

    if (dragZone == TimelineClipZone::edgeLeftTrim ||
        dragZone == TimelineClipZone::edgeRightTrim ||
        dragZone == TimelineClipZone::edgeLeftStretch ||
        dragZone == TimelineClipZone::edgeRightStretch ||
        dragZone == TimelineClipZone::edgeLeftSlip ||
        dragZone == TimelineClipZone::edgeRightSlip) {
      for (const auto &c : data.clips)
        if (c.id == draggedClipId) {
          host.resizeClip(data.id, c.id, c.start, c.length);
          break;
        }
    }

    if (uiStates.contains(draggedClipId)) {
      auto &ui = uiStates[draggedClipId];
      ui.isStretching = false;
      ui.isSlipping = false;
      std::sort(ui.points.begin(), ui.points.end(),
                [](const MidiEnvelopePoint &a, const MidiEnvelopePoint &b) {
                  return a.normX < b.normX;
                });
    }

    resetDragState();
    mouseMove(e);
    repaint();
  }

  bool keyPressed(const juce::KeyPress &key) override {
    // Rollback deterministico della gesture attiva con Escape
    if (key.isKeyCode(juce::KeyPress::escapeKey) && !draggedClipId.empty() &&
        hasDragSnapshot) {
      uiStates[draggedClipId] = snapshotUIState;
      for (auto &c : data.clips) {
        if (c.id == draggedClipId) {
          c.start = startClipStart;
          c.length = startClipLength;
          break;
        }
      }
      layoutClipComponents();
      resetDragState();
      repaint();
      return true;
    }

    if (key.getKeyCode() == juce::KeyPress::backspaceKey ||
        key.getKeyCode() == juce::KeyPress::deleteKey) {
      bool anyChanged = false;
      for (auto &[cid, ui] : uiStates) {
        if (ui.points.size() > 2) {
          auto it = ui.points.begin() + 1;
          while (it != ui.points.end() - 1) {
            if (it->selected) {
              it = ui.points.erase(it);
              anyChanged = true;
            } else
              ++it;
          }
        }
      }
      if (anyChanged) {
        repaint();
        return true;
      }
    }
    return false;
  }

  void mouseDoubleClick(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu())
      return;

    const auto hit = hitTestZones(e.position);

    // Doppio click sul titolo o header della clip (o intera clip se compressa):
    // cicla le 3 altezze canoniche condivise (96 -> 256 -> 400 -> 96) tenendo
    // conto dello zoom verticale
    if (hit.clip != nullptr &&
        clip_height_law::isHeaderAreaOrClosedClip(
            hit.zone, hit.bounds.getHeight(), hit.markerIdx)) {
      const int modelHeight = clip_height_law::nextModelHeight(
          static_cast<float>(height), host.getVerticalZoom());
      draggedClipId.clear();
      dragZone = TimelineClipZone::none;
      host.store().resize(data.id, modelHeight);
      return;
    }

    if (!panelView.enabled && e.position.y >= getHeight() - 6.0f) {
      const int modelHeight = clip_height_law::nextModelHeight(
          static_cast<float>(height), host.getVerticalZoom());
      host.store().resize(data.id, modelHeight);
      return;
    }

    if (hit.clip == nullptr)
      return;
    auto &ui = uiStates[hit.clip->id];

    const double clickSec =
        panelView.xToTime(e.position.x,getWidth());
    const auto ovInfo = findOverlapAtTime(clickSec, hit.clip->id);
    if (ovInfo.inOverlap && (hit.zone == TimelineClipZone::none ||
                             hit.zone == TimelineClipZone::clipBody)) {
      const std::pair<std::string, std::string> pairKey{ovInfo.leftClipId,
                                                        ovInfo.rightClipId};
      if (xfadePairs.contains(pairKey))
        xfadePairs.erase(pairKey);
      else
        xfadePairs.insert(pairKey);
      syncActiveLineCrossfades({}, true);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::edgeLeftStretch ||
        hit.zone == TimelineClipZone::edgeRightStretch) {
      if (ui.origClipLength > 0.125) {
        ui.stretchTicks = 0;
        ui.isStretching = false;
        host.resizeClip(data.id, hit.clip->id, hit.clip->start,
                        ui.origClipLength);
        repaint();
        return;
      }
    }

    if (hit.zone == TimelineClipZone::edgeLeftSlip ||
        hit.zone == TimelineClipZone::edgeRightSlip) {
      ui.sourceOffsetSec = ui.defaultSourceOffsetSec;
      ui.slipDeltaTicks = 0;
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::gainEnvelopePoint && hit.pointIdx >= 0 &&
        hit.pointIdx < static_cast<int>(ui.points.size())) {
      ui.points[static_cast<size_t>(hit.pointIdx)].normValue =
          ui.defaultNormValue;
      clips::enforceAutomationBoundaryPositions(
          ui.points.data(), static_cast<int>(ui.points.size()),
          ui.boundary.mode);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 &&
        hit.markerIdx < static_cast<int>(ui.warpPoints.size())) {
      ui.warpPoints[static_cast<size_t>(hit.markerIdx)].warpedTimeSec =
          ui.warpPoints[static_cast<size_t>(hit.markerIdx)].sourceTimeSec;
      syncLineWarpNorms(ui, hit.clip->length, ui.sourceOffsetSec);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::gainEnvelopeLine) {
      constexpr float kTitleAreaHeight = 16.0f;
      const float bodyH =
          juce::jmax(10.0f, hit.bounds.getHeight() - kTitleAreaHeight);
      const float normX = juce::jlimit(0.01f, 0.99f,
                                       (e.position.x - hit.bounds.getX()) /
                                           hit.bounds.getWidth());
      const float normY = juce::jlimit(
          0.0f, 1.0f, (hit.bounds.getBottom() - e.position.y) / bodyH);

      for (auto &pt : ui.points)
        pt.selected = false;
      ui.points.push_back({normX, normY, 0.5f, true});
      std::sort(ui.points.begin(), ui.points.end(),
                [](const MidiEnvelopePoint &a, const MidiEnvelopePoint &b) {
                  return a.normX < b.normX;
                });
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::gainSegmentCurveHandle &&
        hit.curveSegmentIdx >= 0 &&
        hit.curveSegmentIdx < static_cast<int>(ui.points.size()) - 1) {
      clips::resetAutomationControlPoint(
          ui.points[static_cast<size_t>(hit.curveSegmentIdx)],
          ui.points[static_cast<size_t>(hit.curveSegmentIdx + 1)]);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::fadeInCurveHandle) {
      applyCrossfadeCurvature(hit.clip->id, true, {0.5, 0.5, -1.0});
      return;
    }

    if (hit.zone == TimelineClipZone::fadeOutCurveHandle) {
      applyCrossfadeCurvature(hit.clip->id, false, {0.5, 0.5, -1.0});
      return;
    }
  }

private:
  void resetDragState() noexcept {
    draggedClipId.clear();
    dragZone = TimelineClipZone::none;
    draggedMarkerIdx = -1;
    draggedPointIdx = -1;
    draggedCurveSegmentIdx = -1;
    hasDragSnapshot = false;
    loopPreview = {};
  }

  ClipFeatures clipFeatures;
  LabComponent &host;
  const juce::Colour curveColour;
  TimelineCurveUnderlayStyle curveUnderlayStyle;
  Track data;
  int height = 132;
  bool programAudible = true;

  std::map<std::string, LineClipUIState> uiStates;
  std::set<std::pair<std::string, std::string>> xfadePairs;
  std::map<std::string, std::unique_ptr<LineClipComponent>> clipComponents;

  ayra::MarqueeSelector marqueeSelector{*this};
  bool isMarqueeActive = false;

  double startClipStart = 0.0;
  double startClipLength = 0.0;
  double startSourceOffsetSec = 0.0;
  clips::ClipLoopDragState loopPreview;

  std::string hoveredClipId;
  TimelineClipZone activeZone = TimelineClipZone::none;
  int hoveredMarkerIdx = -1;
  int hoveredPointIdx = -1;
  int hoveredCurveSegmentIdx = -1;
  juce::Point<float> mousePos{-1.0f, -1.0f};

  std::string draggedClipId;
  TimelineClipZone dragZone = TimelineClipZone::none;
  int draggedMarkerIdx = -1;
  int draggedPointIdx = -1;
  int draggedCurveSegmentIdx = -1;
  juce::Point<float> dragStartPos;

  std::vector<MidiEnvelopePoint> startEnvelopePoints;
  ayra::curves::CurveBoundarySpec startBoundary;

  bool hasDragSnapshot = false;
  LineClipUIState snapshotUIState;

  bool isHoveringDivider = false;
  bool isResizingLaneHeight = false;
  int resizeLaneStartY = 0;
  int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
