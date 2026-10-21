/** @file ayra_MidiClipCanvas.cpp
    Adapter del Lab per una lane di clip MIDI / Instrument.
    Contiene orchestrazione Message Thread, hit-test multi-livello (LOD),
    warp piecewise-linear (Linked Note+CC o Unlinked Note-Only), split con CC
   Chase e in-place editing delle note ad alto zoom verticale. */

namespace ayra::lab {
namespace {

using clips::ClipFeatures;
using clips::computeClipFeaturesLOD;
using clips::computeClipOverflows;
using clips::MidiCcGhostLaneVisual;
using clips::MidiClipComponent;
using clips::MidiClipVisual;
using clips::MidiEnvelopePoint;
using clips::MidiNoteVisual;
using clips::MidiOverlayTarget;
using clips::MidiProgramChangeVisual;
using clips::TimelineClipZone;
namespace clip_midi_law = clips::clip_midi_law;
namespace clip_height_law = clips::clip_height_law;

struct MidiWarpPoint {
  double sourceTimeSec = 0.0;
  double warpedTimeSec = 0.0;
};

struct MidiSourceNote {
  double startSec =
      0.0; // Tempo sorgente (in secondi/beat relativi alla sorgente)
  double lengthSec = 0.25;
  std::uint8_t pitch = 60;
  std::uint8_t velocity = 100;
  std::uint8_t channel = 1;
  bool selected = false;
  bool muted = false;
};

struct MidiSourceProgramChange {
  float normX = 0.0f; // Posizione normalizzata nella clip [0..1]
  std::uint8_t program = 0;
  juce::String name;
};

struct MidiClipUIState {
  bool isDrumMode = false;
  int transposeSemitones = 0;
  int velocityOffset = 0;

  MidiOverlayTarget activeOverlay = MidiOverlayTarget::velocityEnvelope;
  std::map<MidiOverlayTarget, std::vector<MidiEnvelopePoint>> ccLanes;

  std::vector<MidiSourceNote> notes;
  std::vector<MidiSourceProgramChange> programChanges;

  std::vector<MidiWarpPoint> warpPoints;
  std::vector<float> warpMarkers;
  std::vector<float> origWarpMarkers;
  bool isUnlinkedNoteWarp = false;

  double origClipLength = 0.0;
  std::int64_t stretchTicks = 0;
  bool isStretching = false;

  double sourceTotalDurationSec = 0.0;
  double sourceOffsetSec = 0.0;
  double defaultSourceOffsetSec = 0.0;
  double startSourceOffsetSec = 0.0;
  std::int64_t slipDeltaTicks = 0;
  bool isSlipping = false;

  // Cache visuale rigenerata per il paint a 0 allocazioni durante il frame
  std::vector<MidiNoteVisual> cachedVisualNotes;
  std::vector<MidiNoteVisual> cachedPartnerOverlapNotes;
  std::vector<MidiProgramChangeVisual> cachedVisualPcs;
  std::vector<juce::Path> cachedGhostPaths;
  std::vector<MidiCcGhostLaneVisual> cachedGhostVisuals;
  std::uint8_t displayMinPitch = 48;
  std::uint8_t displayMaxPitch = 72;
  clip_midi_law::ZoomLod zoomLod{clip_midi_law::ZoomLod::preview};
  bool pitchViewportInitialised = false;
  bool pitchViewportManual = false;
  bool freezePitchViewport = false;
  bool pitchScrollbarVisible = false;
  bool pitchScrollbarOnLeft = false;
  juce::uint32 pitchScrollbarHideTimeMs = 0;
};

static void setPitchViewportCentre(MidiClipUIState &ui, const int centrePitch,
                                   const bool markManual) noexcept {
  const auto range = clip_midi_law::pitchRangeCentredAt(centrePitch);
  ui.displayMinPitch = range.minPitch;
  ui.displayMaxPitch = range.maxPitch;
  ui.pitchViewportInitialised = true;
  ui.pitchViewportManual = markManual;
}

static void
setPitchViewportFromScrollbarY(MidiClipUIState &ui,
                               const juce::Rectangle<float> clipBounds,
                               const bool onLeft, const float mouseY) noexcept {
  const auto scrollbar = clips::computeMidiPitchScrollbarGeometry(
      clipBounds, ui.zoomLod, ui.displayMinPitch, ui.displayMaxPitch, onLeft);
  if (scrollbar.trackBounds.isEmpty()) {
    return;
  }

  const int visibleCount = static_cast<int>(ui.displayMaxPitch) -
                           static_cast<int>(ui.displayMinPitch) + 1;
  const float travel =
      scrollbar.trackBounds.getHeight() - scrollbar.thumbBounds.getHeight();
  const float topNorm =
      travel > 0.0f ? juce::jlimit(0.0f, 1.0f,
                                   (mouseY - scrollbar.trackBounds.getY() -
                                    scrollbar.thumbBounds.getHeight() * 0.5f) /
                                       travel)
                    : 0.0f;
  const int maxPossibleMin = juce::jmax(0, 128 - visibleCount);
  const int minPitch =
      juce::roundToInt((1.0f - topNorm) * static_cast<float>(maxPossibleMin));
  const int maxPitch = juce::jmin(127, minPitch + visibleCount - 1);
  ui.displayMinPitch = static_cast<std::uint8_t>(minPitch);
  ui.displayMaxPitch = static_cast<std::uint8_t>(maxPitch);
  ui.pitchViewportInitialised = true;
  ui.pitchViewportManual = true;
}

static void syncMidiWarpNorms(MidiClipUIState &ui, double clipLength,
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

class MidiClipCanvas final : public juce::Component,
                             public juce::TooltipClient,
                             public clips::MidiClipPresentationProvider,
                             private juce::Timer {
public:
  CanvasViewportBinding panelView;

  // The identity lens uses the exact clip/edit hit regions (including rails
  // and handles), not an approximate lane-wide temporal rectangle.
  bool containsPanelClipAt(juce::Point<float> point) const
  {
      return getLocalBounds().toFloat().contains(point) && hitTestZones(point).clip != nullptr;
  }


  std::shared_ptr<void> capturePanelState() const
  {
      using State = std::tuple<decltype(uiStates)>;
      return std::make_shared<State>(uiStates);
  }
  void restorePanelState(const std::shared_ptr<void>& state)
  {
      if(!state)return;
      using State = std::tuple<decltype(uiStates)>;
      const auto& saved=*std::static_pointer_cast<State>(state);
      uiStates=std::get<0>(saved);
  }


  explicit MidiClipCanvas(LabComponent &h) : host(h) {
    clipFeatures.gainEnabled = false;
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

            if (ui.zoomLod != clip_midi_law::ZoomLod::preview) {
              for (const auto &vn : ui.cachedVisualNotes) {
                if (vn.sourceIndex < 0 ||
                    vn.sourceIndex >= static_cast<int>(ui.notes.size()) ||
                    vn.endNormX < 0.0f || vn.startNormX > 1.0f)
                  continue;

                const auto noteGeometry = clips::computeMidiNoteGeometry(
                    vn, b, ui.displayMinPitch, ui.displayMaxPitch,
                    ui.isDrumMode, ui.zoomLod);
                const bool inside =
                    area.intersects(noteGeometry.bounds.toNearestInt());
                auto &sourceNote =
                    ui.notes[static_cast<size_t>(vn.sourceIndex)];
                if (mode == ayra::MarqueeSelector::ModifierMode::Replace)
                  sourceNote.selected = inside;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Add &&
                         inside)
                  sourceNote.selected = true;
                else if (mode ==
                             ayra::MarqueeSelector::ModifierMode::Subtract &&
                         inside)
                  sourceNote.selected = false;
                else if (mode == ayra::MarqueeSelector::ModifierMode::Toggle &&
                         inside)
                  sourceNote.selected = !sourceNote.selected;

                anyChanged = true;
              }
            }
            rebuildClipVisualCache(c, ui);
          }

          if (anyChanged)
            repaint();
        };

    marqueeSelector.onPerformClickSelection =
        [this](juce::Point<int>, ayra::MarqueeSelector::ModifierMode mode) {
          if (mode == ayra::MarqueeSelector::ModifierMode::Replace) {
            for (auto &[cid, ui] : uiStates) {
              for (auto &[target, pts] : ui.ccLanes)
                for (auto &pt : pts)
                  pt.selected = false;
              for (auto &n : ui.notes)
                n.selected = false;
            }
          }
        };
  }

  ~MidiClipCanvas() override { stopTimer(); }

  void timerCallback() override {
    const auto now = juce::Time::getMillisecondCounter();
    bool anyRunning = false;
    bool needsRepaint = false;

    for (auto &[cid, ui] : uiStates) {
      if (ui.pitchScrollbarVisible) {
        if (draggedClipId == cid &&
            dragZone == TimelineClipZone::midiPitchScrollbar) {
          anyRunning = true;
          continue;
        }

        if (hoveredClipId == cid) {
          anyRunning = true;
          continue;
        }

        if (ui.pitchScrollbarHideTimeMs != 0) {
          if (now >= ui.pitchScrollbarHideTimeMs) {
            ui.pitchScrollbarVisible = false;
            ui.pitchScrollbarHideTimeMs = 0;
            needsRepaint = true;
          } else {
            anyRunning = true;
          }
        }
      }
    }

    if (!anyRunning)
      stopTimer();

    if (needsRepaint) {
      for (auto &[id, comp] : clipComponents)
        if (comp != nullptr)
          comp->repaint();
      repaint();
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
      return "Slip Contenuto MIDI (Sizing Moves Contents)\nTrascina per "
             "scorrere la finestra sulle note mantenendo ancorato il pattern "
             "MIDI";
    case TimelineClipZone::edgeLeftTrim:
    case TimelineClipZone::edgeRightTrim:
      return "Trim Clip MIDI (Normal Sizing)\nNasconde o rivela le note oltre "
             "il bordo senza eliminarle (mostra indicatore overhang se una "
             "nota prosegue)";
    case TimelineClipZone::edgeLeftStretch:
    case TimelineClipZone::edgeRightStretch:
      return "Time Stretch MIDI\nTrascina per scalare temporalmente note e "
             "automazioni CC (doppio click per reset 1.0x)";
    case TimelineClipZone::midiNoteBody:
      return "Nota MIDI In-Place\nTrascina per muovere Pitch/Tempo, Shift+Drag "
             "orizzontale per Velocity, Alt+Click per eliminare";
    case TimelineClipZone::midiNoteLeftEdge:
    case TimelineClipZone::midiNoteRightEdge:
      return "Durata Nota MIDI\nTrascina il bordo per allungare o accorciare "
             "la nota";
    case TimelineClipZone::midiPitchScrollbar:
      return "Pitch Scroll\nTrascina per vedere altre ottave; doppio click per "
             "centrare C3";
    case TimelineClipZone::midiVelocityStem:
      return "Velocity\nTrascina lo stelo per modificare la velocity (Shift "
             "per snap a 10). Piu' note selezionate mantengono i loro offset";
    case TimelineClipZone::midiVelocityLane:
      return "Velocity Lane\nDrag libero; Shift+Drag crea una rampa; Alt+Drag "
             "livella";
    case TimelineClipZone::topWarpRail:
      return "Binario Warp MIDI Superiore\nClicca per inserire un Warp Marker "
             "(con auto-snap magnetico all'attacco nota piu' vicino)";
    case TimelineClipZone::warpMarker:
      return "Warp Marker MIDI\nTrascina per deformare Note + CC assieme; "
             "Shift+Trascina per muovere SOLO le Note lasciando fermi i CC "
             "(Doppio click reset, Alt+Click elimina)";
    case TimelineClipZone::bottomCutRail:
      return "Binario di Taglio MIDI (Forbici)\nClicca per dividere la clip "
             "con CC Chase automatico e taglio note (Alt+Click mantiene le "
             "note intere in overhang)";
    case TimelineClipZone::clipTitleMove:
      return "Intestazione Clip MIDI\nTrascina per spostare la clip; doppio "
             "click cambia livello di zoom";
    default:
      if (!panelView.enabled && isHoveringDivider)
        return "Separatore Traccia\nTrascina verticalmente per ridimensionare "
               "l'altezza";
      if (!hoveredClipId.empty())
        return "Corpo Clip MIDI\nTrascina per selezione marquee; doppio click "
               "crea una nota nei livelli editabili";
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

    // Se il cursore si trova sopra la scrollbar verticale del pitch,
    // lo scorrimento del mouse/trackpad naviga fluidamente il pitch viewport
    const auto hit = hitTestZones(e.position);
    if (hit.clip != nullptr &&
        hit.zone == TimelineClipZone::midiPitchScrollbar) {
      auto &ui = uiStates[hit.clip->id];
      ui.pitchScrollbarVisible = true;
      ui.pitchScrollbarHideTimeMs = 0;

      const float delta =
          (std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX);
      if (std::abs(delta) > 0.0005f) {
        const int pitchDelta = juce::roundToInt(delta * 14.0f);
        const int step =
            (pitchDelta != 0) ? pitchDelta : (delta > 0.0f ? 1 : -1);

        const int currentMin = static_cast<int>(ui.displayMinPitch);
        const int currentMax = static_cast<int>(ui.displayMaxPitch);
        const int span = currentMax - currentMin;

        const int newMin = juce::jlimit(0, 127 - span, currentMin + step);
        const int newMax = newMin + span;

        if (newMin != currentMin) {
          ui.displayMinPitch = static_cast<std::uint8_t>(newMin);
          ui.displayMaxPitch = static_cast<std::uint8_t>(newMax);
          ui.pitchViewportInitialised = true;
          ui.pitchViewportManual = true;
          updateMidiClip(hit.clip->id);
          for (auto &[id, comp] : clipComponents)
            if (comp != nullptr)
              comp->repaint();
          repaint();
        }
        return;
      }
    }

    if (auto *vp = findParentComponentOfClass<juce::Viewport>())
      vp->mouseWheelMove(e, wheel);
    else
      Component::mouseWheelMove(e, wheel);
  }

  // Mappa un tempo sorgente (secondi nella sorgente MIDI) in tempo warppato
  // visibile
  static double mapSourceSecToWarpedSec(double sourceSec,
                                        const MidiClipUIState &ui,
                                        double totalDur) {
    if (ui.warpPoints.empty())
      return sourceSec;

    if (sourceSec <= ui.warpPoints.front().sourceTimeSec) {
      const double s0 = 0.0, w0 = 0.0;
      const double s1 = ui.warpPoints.front().sourceTimeSec;
      const double w1 = ui.warpPoints.front().warpedTimeSec;
      if (s1 <= s0 + 0.0001)
        return w1;
      const double frac = std::clamp((sourceSec - s0) / (s1 - s0), 0.0, 1.0);
      return w0 + frac * (w1 - w0);
    }

    for (size_t k = 0; k + 1 < ui.warpPoints.size(); ++k) {
      const auto &wp0 = ui.warpPoints[k];
      const auto &wp1 = ui.warpPoints[k + 1];
      if (sourceSec <= wp1.sourceTimeSec) {
        const double span = wp1.sourceTimeSec - wp0.sourceTimeSec;
        if (span <= 0.0001)
          return wp0.warpedTimeSec;
        const double frac =
            std::clamp((sourceSec - wp0.sourceTimeSec) / span, 0.0, 1.0);
        return wp0.warpedTimeSec +
               frac * (wp1.warpedTimeSec - wp0.warpedTimeSec);
      }
    }

    const double s0 = ui.warpPoints.back().sourceTimeSec;
    const double w0 = ui.warpPoints.back().warpedTimeSec;
    const double s1 = std::max(s0 + 0.001, totalDur);
    const double w1 = s1;
    const double frac = std::clamp((sourceSec - s0) / (s1 - s0), 0.0, 1.0);
    return w0 + frac * (w1 - w0);
  }

  // Inversa piecewise-linear del mapping sopra. Le gesture lavorano nello
  // spazio visibile e tornano nello spazio sorgente soltanto al commit locale.
  static double mapWarpedSecToSourceSec(double warpedSec,
                                        const MidiClipUIState &ui,
                                        double totalDur) {
    if (ui.warpPoints.empty())
      return warpedSec;

    if (warpedSec <= ui.warpPoints.front().warpedTimeSec) {
      const double w1 = ui.warpPoints.front().warpedTimeSec;
      const double s1 = ui.warpPoints.front().sourceTimeSec;
      if (w1 <= 0.0001) {
        return s1;
      }
      const double fraction = std::clamp(warpedSec / w1, 0.0, 1.0);
      return fraction * s1;
    }

    for (size_t index = 0; index + 1 < ui.warpPoints.size(); ++index) {
      const auto &p0 = ui.warpPoints[index];
      const auto &p1 = ui.warpPoints[index + 1];
      if (warpedSec <= p1.warpedTimeSec) {
        const double span = p1.warpedTimeSec - p0.warpedTimeSec;
        if (span <= 0.0001) {
          return p0.sourceTimeSec;
        }
        const double fraction =
            std::clamp((warpedSec - p0.warpedTimeSec) / span, 0.0, 1.0);
        return p0.sourceTimeSec +
               fraction * (p1.sourceTimeSec - p0.sourceTimeSec);
      }
    }

    const double w0 = ui.warpPoints.back().warpedTimeSec;
    const double s0 = ui.warpPoints.back().sourceTimeSec;
    const double s1 = std::max(s0 + 0.001, totalDur);
    const double w1 = s1;
    const double span = w1 - w0;
    if (span <= 0.0001) {
      return s0;
    }
    const double fraction = std::clamp((warpedSec - w0) / span, 0.0, 1.0);
    return s0 + fraction * (s1 - s0);
  }

  // Valuta una curva di inviluppo MIDI in un dato normX [0..1]
  static float evaluateOverlayAt(const std::vector<MidiEnvelopePoint> &pts,
                                 float normX, float defaultVal) {
    if (pts.empty())
      return defaultVal;
    if (normX <= pts.front().normX)
      return pts.front().normValue;
    if (normX >= pts.back().normX)
      return pts.back().normValue;

    size_t s = 0;
    while (s + 1 < pts.size() && pts[s + 1].normX < normX)
      ++s;
    if (s + 1 >= pts.size())
      return pts.back().normValue;

    const auto &p0 = pts[s];
    const auto &p1 = pts[s + 1];
    return clips::evaluateMidiEnvelopeSegment(p0, p1, normX);
  }

  static void rebuildClipVisualCache(const Clip &c, MidiClipUIState &ui) {
    ui.cachedVisualNotes.clear();
    ui.cachedVisualPcs.clear();
    ui.cachedGhostPaths.clear();
    ui.cachedGhostVisuals.clear();

    const double totalDur = std::max(0.1, ui.sourceTotalDurationSec > 0.0
                                              ? ui.sourceTotalDurationSec
                                              : (c.length + 8.0));
    const double srcOffset = std::clamp(ui.sourceOffsetSec, 0.0, totalDur);
    const double effectiveLen =
        (ui.isStretching || ui.stretchTicks != 0) && ui.origClipLength > 0.05
            ? ui.origClipLength
            : std::max(0.05, c.length);

    int minP = 127;
    int maxP = 0;

    for (size_t noteIndex = 0; noteIndex < ui.notes.size(); ++noteIndex) {
      const auto &n = ui.notes[noteIndex];
      const double warpedStart =
          mapSourceSecToWarpedSec(n.startSec, ui, totalDur);
      const double warpedEnd =
          mapSourceSecToWarpedSec(n.startSec + n.lengthSec, ui, totalDur);

      const float startNorm =
          static_cast<float>((warpedStart - srcOffset) / effectiveLen);
      const float endNorm =
          static_cast<float>((warpedEnd - srcOffset) / effectiveLen);

      if (endNorm < -0.05f || startNorm > 1.05f)
        continue;

      const int transposedPitch = juce::jlimit(
          0, 127, static_cast<int>(n.pitch) + ui.transposeSemitones);
      minP = std::min(minP, transposedPitch);
      maxP = std::max(maxP, transposedPitch);

      const int scaledVel = juce::jlimit(
          1, 127, static_cast<int>(n.velocity) + ui.velocityOffset);

      MidiNoteVisual vn;
      vn.sourceIndex = static_cast<int>(noteIndex);
      vn.startNormX = startNorm;
      vn.endNormX = std::max(startNorm + 0.01f, endNorm);
      vn.pitch = static_cast<std::uint8_t>(transposedPitch);
      vn.velocity = static_cast<std::uint8_t>(scaledVel);
      vn.channel = n.channel;
      vn.selected = n.selected;
      vn.muted = n.muted;
      vn.overhangsRight = (endNorm > 1.002f);
      vn.overhangsLeft = (startNorm < -0.002f);
      ui.cachedVisualNotes.push_back(vn);
    }

    if (minP > maxP) {
      minP = 48;
      maxP = 72;
    } else {
      // Garantisce almeno 12 semitoni di escursione e 2 semitoni di respiro
      // sopra/sotto
      minP = std::max(0, minP - 2);
      maxP = std::min(127, maxP + 2);
      if (maxP - minP < 12) {
        const int centrePitch = (minP + maxP) / 2;
        minP = std::clamp(centrePitch - 6, 0, 115);
        maxP = minP + 12;
      }
    }
    if (ui.zoomLod == clip_midi_law::ZoomLod::preview) {
      ui.displayMinPitch = static_cast<std::uint8_t>(minP);
      ui.displayMaxPitch = static_cast<std::uint8_t>(maxP);
    } else if (!ui.pitchViewportInitialised ||
               (!ui.pitchViewportManual && !ui.freezePitchViewport)) {
      setPitchViewportCentre(ui, (minP + maxP) / 2, false);
    }
  }

  void updateMidiClip(const std::string &clipId) {
    for (const auto &c : data.clips) {
      if (c.id == clipId && uiStates.contains(clipId)) {
        syncMidiWarpNorms(uiStates[clipId], c.length,
                          uiStates[clipId].sourceOffsetSec);
        rebuildClipVisualCache(c, uiStates[clipId]);
        break;
      }
    }
    layoutClipComponents();
  }

  void duplicateMidiClipLoopUI(const std::string &origId,
                               const std::vector<std::string> &newIds) {
    if (!uiStates.contains(origId))
      return;
    const auto &orig = uiStates[origId];
    for (const auto &newId : newIds) {
      MidiClipUIState dup = orig;
      dup.isStretching = false;
      dup.isSlipping = false;
      dup.isUnlinkedNoteWarp = false;
      dup.freezePitchViewport = false;
      uiStates[newId] = std::move(dup);
      updateMidiClip(newId);
    }
  }

  // Taglio clip MIDI con CC Chase / Latching su tutte le lane + divisione
  // intelligente delle note
  void splitMidiClipUI(const std::string &origId, const std::string &newId,
                       float splitFrac, bool keepOverhangNotes) {
    if (!uiStates.contains(origId))
      return;

    auto &orig = uiStates[origId];
    MidiClipUIState next;
    next.isDrumMode = orig.isDrumMode;
    next.transposeSemitones = orig.transposeSemitones;
    next.velocityOffset = orig.velocityOffset;
    next.activeOverlay = orig.activeOverlay;

    const double origLenBefore =
        orig.origClipLength > 0.0 ? orig.origClipLength : 4.0;
    const double totalDuration = std::max(0.1, orig.sourceTotalDurationSec > 0.0
                                                   ? orig.sourceTotalDurationSec
                                                   : origLenBefore + 8.0);
    const double splitWarpedTime =
        orig.sourceOffsetSec + static_cast<double>(splitFrac) * origLenBefore;
    const double splitSourceTime =
        mapWarpedSecToSourceSec(splitWarpedTime, orig, totalDuration);

    // 1. Partiziona i Warp Points
    std::vector<MidiWarpPoint> origWps, nextWps;
    for (const auto &wp : orig.warpPoints) {
      if (wp.sourceTimeSec < splitSourceTime - 0.005)
        origWps.push_back(wp);
      else if (wp.sourceTimeSec > splitSourceTime + 0.005)
        nextWps.push_back(wp);
    }
    orig.warpPoints = std::move(origWps);
    next.warpPoints = std::move(nextWps);

    // 2. Partiziona o taglia le Note MIDI che attraversano la forbice
    std::vector<MidiSourceNote> leftNotes, rightNotes;
    for (const auto &n : orig.notes) {
      const double noteEnd = n.startSec + n.lengthSec;
      if (noteEnd <= splitSourceTime + 0.002) {
        leftNotes.push_back(n);
      } else if (n.startSec >= splitSourceTime - 0.002) {
        rightNotes.push_back(n);
      } else {
        // La nota attraversa il taglio!
        if (keepOverhangNotes || orig.isDrumMode) {
          leftNotes.push_back(
              n); // Rimane intera in overhang oltre il bordo destro
        } else {
          MidiSourceNote nLeft = n;
          nLeft.lengthSec = std::max(0.04, splitSourceTime - n.startSec);
          leftNotes.push_back(nLeft);

          MidiSourceNote nRight = n;
          nRight.startSec = splitSourceTime;
          nRight.lengthSec = std::max(0.04, noteEnd - splitSourceTime);
          rightNotes.push_back(nRight);
        }
      }
    }
    orig.notes = std::move(leftNotes);
    next.notes = std::move(rightNotes);

    next.origClipLength = origLenBefore * (1.0 - splitFrac);
    orig.origClipLength = origLenBefore * splitFrac;
    next.sourceTotalDurationSec =
        orig.sourceTotalDurationSec > 0.0 ? orig.sourceTotalDurationSec : 16.0;
    next.sourceOffsetSec = splitSourceTime;
    next.defaultSourceOffsetSec = splitSourceTime;
    next.startSourceOffsetSec = next.sourceOffsetSec;

    syncMidiWarpNorms(orig, orig.origClipLength, orig.sourceOffsetSec);
    syncMidiWarpNorms(next, next.origClipLength, next.sourceOffsetSec);

    uiStates[newId] = std::move(next);
  }

  static std::vector<MidiSourceNote>
  makeSyntheticMidiNotes(int seed, double clipOffsetSec, double clipLenSec,
                         bool drumMode) {
    std::vector<MidiSourceNote> result;
    juce::Random rng(seed);

    const int totalSteps =
        std::max(8, static_cast<int>(std::round(clipLenSec * 4.0)));
    const double stepDur = clipLenSec / static_cast<double>(totalSteps);
    const int minorScale[] = {0, 2, 3, 5, 7, 8, 10, 12, 14, 15};

    // Generiamo anche note prima e dopo i bordi della clip (per testare Trim e
    // Slip!)
    for (int step = -8; step < totalSteps + 8; ++step) {
      const double t = clipOffsetSec + static_cast<double>(step) * stepDur;
      if (t < 0.0)
        continue;

      if (drumMode) {
        // Kick / Snare / Hat pattern
        if (step % 4 == 0)
          result.push_back({t, stepDur * 0.8, 36, 118, 10, false, false});
        if (step % 4 == 2)
          result.push_back({t, stepDur * 0.8, 38, 112, 10, false, false});
        if (rng.nextFloat() > 0.2f)
          result.push_back({t, stepDur * 0.45, 42,
                            static_cast<std::uint8_t>(70 + rng.nextInt(45)), 10,
                            false, false});
      } else {
        // Riff / Arpeggio melodico + accordi
        if (step % 4 == 0) {
          const std::uint8_t root = static_cast<std::uint8_t>(
              48 + minorScale[(std::abs(step / 4)) % 5]);
          result.push_back({t, stepDur * 3.6, root, 108, 1, false, false});
          result.push_back({t, stepDur * 3.6,
                            static_cast<std::uint8_t>(root + 7), 96, 1, false,
                            false});
        }
        if (rng.nextFloat() > 0.25f) {
          const int deg = rng.nextInt(10);
          const std::uint8_t leadPitch =
              static_cast<std::uint8_t>(60 + minorScale[deg]);
          const std::uint8_t vel =
              static_cast<std::uint8_t>(68 + rng.nextInt(58));
          result.push_back({t, stepDur * (rng.nextFloat() > 0.6f ? 1.8 : 0.85),
                            leadPitch, vel, 1, false, false});
        }
      }
    }
    return result;
  }

  void setModel(const Track &t, int midiHeight) {
    data = t;
    height = midiHeight;
    programAudible = audibility(host.store().state(), data).program;

    std::set<std::string> live;
    for (const auto &c : t.clips) {
      live.insert(c.id);

      auto component = clipComponents.find(c.id);
      if (component == clipComponents.end()) {
        auto &provider =
            static_cast<clips::MidiClipPresentationProvider &>(*this);
        auto midiClip = std::make_unique<MidiClipComponent>(c.id, provider);
        addAndMakeVisible(*midiClip);
        component = clipComponents.emplace(c.id, std::move(midiClip)).first;
      }
      component->second->toFront(false);

      if (!uiStates.contains(c.id)) {
        MidiClipUIState s;
        s.zoomLod = clip_midi_law::zoomLodForTrackHeight(
            static_cast<float>(midiHeight));
        s.origClipLength = c.length;
        s.sourceTotalDurationSec = c.length + 8.0;
        s.sourceOffsetSec = 4.0;
        s.defaultSourceOffsetSec = s.sourceOffsetSec;
        s.startSourceOffsetSec = s.sourceOffsetSec;
        s.isDrumMode = juce::String(c.name).containsIgnoreCase("drum") ||
                       juce::String(c.name).containsIgnoreCase("kit");

        s.notes = makeSyntheticMidiNotes(c.seed, s.sourceOffsetSec, c.length,
                                         s.isDrumMode);

        if (!juce::String(c.id).contains("_cut")) {
          for (float frac : {0.25f, 0.75f}) {
            const double sampleTime =
                s.sourceOffsetSec + static_cast<double>(frac) * c.length;
            s.warpPoints.push_back({sampleTime, sampleTime});
          }
        }

        syncMidiWarpNorms(s, c.length, s.sourceOffsetSec);
        rebuildClipVisualCache(c, s);
        uiStates[c.id] = std::move(s);
      } else {
        auto &s = uiStates[c.id];
        const auto nextLod = clip_midi_law::zoomLodForTrackHeight(
            static_cast<float>(midiHeight));
        if (s.zoomLod != nextLod) {
          s.zoomLod = nextLod;
          if (nextLod == clip_midi_law::ZoomLod::preview)
            s.pitchViewportInitialised = false;
        }
        syncMidiWarpNorms(s, c.length, s.sourceOffsetSec);
        rebuildClipVisualCache(c, s);
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

  void setMidiOverlapMode(const clips::MidiOverlapMode mode) {
    if (midiOverlapMode != mode) {
      midiOverlapMode = mode;
      layoutClipComponents();
      repaint();
    }
  }
  [[nodiscard]] clips::MidiOverlapMode getMidiOverlapMode() const noexcept {
    return midiOverlapMode;
  }

  void setWarpFeatureEnabled(const bool enabled) {
    if (clipFeatures.warpEnabled != enabled) {
      clipFeatures.warpEnabled = enabled;
      layoutClipComponents();
      repaint();
    }
  }

  [[nodiscard]] bool isWarpFeatureEnabled() const noexcept {
    return clipFeatures.warpEnabled;
  }

  void setOverlayFeatureEnabled(const bool enabled) {
    if (clipFeatures.gainEnabled != enabled) {
      clipFeatures.gainEnabled = enabled;
      layoutClipComponents();
      repaint();
    }
  }

  [[nodiscard]] bool isOverlayFeatureEnabled() const noexcept {
    return clipFeatures.gainEnabled;
  }

  [[nodiscard]] const ClipFeatures &getClipFeatures() const noexcept {
    return clipFeatures;
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
      g.drawText("Ingresso MIDI / Instrument Lane", 25, 35, getWidth() - 40, 20,
                 juce::Justification::centredLeft);
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
    std::string bottomClipId;
    std::string topClipId;
    double startSec = 0.0;
    double endSec = 0.0;
  };

  [[nodiscard]] std::vector<const Clip *> getClipsSortedByZOrder() const {
    std::vector<const Clip *> sorted;
    sorted.reserve(data.clips.size());
    for (const auto &c : data.clips)
      sorted.push_back(&c);

    // Di base vince sempre la clip successiva (start maggiore)
    std::stable_sort(
        sorted.begin(), sorted.end(),
        [](const Clip *a, const Clip *b) { return a->start < b->start; });

    // In modalita' MERGE, la clip sopra l'altra (ed editabile nel range di
    // overlap) e' l'ultima selezionata!
    if (midiOverlapMode == clips::MidiOverlapMode::merge) {
      const std::string &activeSelectedId =
          !lastSelectedClipId.empty() ? lastSelectedClipId
                                      : host.store().state().selectedClip;
      auto it = std::find_if(sorted.begin(), sorted.end(),
                             [&activeSelectedId](const Clip *c) {
                               return c->id == activeSelectedId;
                             });
      if (it != sorted.end()) {
        const Clip *topClip = *it;
        sorted.erase(it);
        sorted.push_back(topClip);
      }
    }
    return sorted;
  }

  [[nodiscard]] OverlapHitInfo
  findOverlapAtTime(double timeSec, const std::string &refClipId = {}) const {
    const auto sorted = getClipsSortedByZOrder();
    for (size_t i = 0; i + 1 < sorted.size(); ++i) {
      for (size_t j = i + 1; j < sorted.size(); ++j) {
        const auto *bottom = sorted[i];
        const auto *top = sorted[j];
        const double ovStart = std::max(bottom->start, top->start);
        const double ovEnd =
            std::min(bottom->start + bottom->length, top->start + top->length);
        if (ovEnd > ovStart + 1.0e-3 && timeSec >= ovStart - 1.0e-3 &&
            timeSec <= ovEnd + 1.0e-3) {
          if (!refClipId.empty() && bottom->id != refClipId &&
              top->id != refClipId)
            continue;
          return {true, bottom->id, top->id, ovStart, ovEnd};
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
        const auto *bottom = sorted[i];
        const auto *top = sorted[j];
        if (top->id != topClipId)
          continue;
        const double ovStart = std::max(bottom->start, top->start);
        const double ovEnd =
            std::min(bottom->start + bottom->length, top->start + top->length);
        if (ovEnd > ovStart + 1.0e-3)
          return {true, bottom->id, top->id, ovStart, ovEnd};
      }
    }
    return {};
  }

  MidiClipVisual
  makeMidiClipVisual(const std::string &clipId,
                     const juce::Rectangle<float> localBodyBounds,
                     const juce::Point<float> parentOrigin) override {
    MidiClipVisual visual;
    visual.bounds = localBodyBounds;

    const auto clip = std::find_if(
        data.clips.begin(), data.clips.end(),
        [&clipId](const Clip &candidate) { return candidate.id == clipId; });
    const auto state = uiStates.find(clipId);
    if (clip == data.clips.end() || state == uiStates.end())
      return visual;

    auto &ui = state->second;
    syncMidiWarpNorms(ui, clip->length, ui.sourceOffsetSec);

    visual.title = juce::String::fromUTF8(clip->name.c_str());
    visual.tint = juce::Colour(data.colour);
    visual.selected = (clipId == host.store().state().selectedClip);
    visual.suspended = !programAudible;
    visual.isDrumMode = ui.isDrumMode;
    visual.zoomLod = ui.zoomLod;

    visual.displayMinPitch = ui.displayMinPitch;
    visual.displayMaxPitch = ui.displayMaxPitch;
    visual.notes = ui.cachedVisualNotes.data();
    visual.numNotes = static_cast<int>(ui.cachedVisualNotes.size());

    visual.warpMarkersNorm = ui.warpMarkers.data();
    visual.origWarpMarkersNorm = ui.origWarpMarkers.data();
    visual.numWarpMarkers = static_cast<int>(ui.warpMarkers.size());
    visual.clipLengthSec = clip->length;
    visual.isUnlinkedNoteWarp = ui.isUnlinkedNoteWarp;

    visual.activeOverlay = ui.activeOverlay;
    visual.overlayPoints = nullptr;
    visual.numOverlayPoints = 0;

    visual.ghostLanes = nullptr;
    visual.numGhostLanes = 0;

    visual.programChanges = nullptr;
    visual.numProgramChanges = 0;

    visual.transposeSemitones = ui.transposeSemitones;
    visual.velocityOffset = ui.velocityOffset;
    visual.stretchDeltaTicks = ui.stretchTicks;
    visual.isStretchActive = ui.isStretching;
    visual.slipDeltaTicks = ui.slipDeltaTicks;
    visual.isSlipActive = ui.isSlipping;

    const bool isDraggedClip = (draggedClipId == clipId);
    const bool pitchScrollbarVisible =
        ui.pitchScrollbarVisible &&
        (ui.zoomLod != clip_midi_law::ZoomLod::preview);
    visual.showPitchScrollbar = pitchScrollbarVisible;
    visual.pitchScrollbarActive =
        isDraggedClip && dragZone == TimelineClipZone::midiPitchScrollbar;
    visual.pitchScrollbarOnLeft = ui.pitchScrollbarOnLeft;
    const bool isDraggingWarp =
        isDraggedClip && (dragZone == TimelineClipZone::warpMarker ||
                          dragZone == TimelineClipZone::topWarpRail);
    const bool isDraggingEnv = false;

    if (isDraggedClip) {
      visual.isDraggingPitchFeedback = isDraggingPitchFeedback;
      visual.draggedPitchValue = draggedPitchFeedbackValue;
      visual.pitchFeedbackPosition = draggedPitchFeedbackPos;

      visual.isDraggingVelocityFeedback = isDraggingVelocityFeedback;
      visual.draggedVelocityValue = draggedVelocityFeedbackValue;
      visual.velocityVisibleInNote = draggedVelocityFeedbackVisibleInNote;
      visual.velocityFeedbackPosition = draggedVelocityFeedbackPos;
    }

    visual.features =
        computeClipFeaturesLOD(clipFeatures, localBodyBounds.getHeight(),
                               isDraggingWarp, isDraggingEnv);

    if (hoveredClipId == clipId ||
        (isDraggedClip && dragZone != TimelineClipZone::none)) {
      const float localMouseX = mousePos.x - parentOrigin.x;
      visual.hoveredZone = isDraggedClip ? dragZone : activeZone;
      visual.hoveredMarkerIdx = hoveredMarkerIdx;
      visual.hoveredEnvelopePointIdx = -1;
      visual.hoveredCurveSegmentIdx = -1;
      visual.hoveredNoteIdx = hoveredNoteIdx;
      visual.hoveredProgramChangeIdx = -1;
      visual.hoverMouseX = localMouseX;
      visual.showWarpAddGhost = visual.features.showWarpRail &&
                                activeZone == TimelineClipZone::topWarpRail &&
                                hoveredMarkerIdx < 0;
      visual.warpAddGhostX = localMouseX;
      visual.isDraggingAllEnvelope = false;
    }

    const auto ovInfo = findOverlapForTopClip(clipId);
    if (ovInfo.inOverlap && clip->length > 0.001) {
      visual.overlap.hasOverlap = true;
      visual.overlap.isCrossfade = false;
      visual.overlap.isTopClip = true;
      visual.overlap.midiMode = midiOverlapMode;
      visual.overlap.startNormX = juce::jlimit(
          0.0f, 1.0f,
          static_cast<float>((ovInfo.startSec - clip->start) / clip->length));
      visual.overlap.endNormX = juce::jlimit(
          0.0f, 1.0f,
          static_cast<float>((ovInfo.endSec - clip->start) / clip->length));
      visual.overlap.partnerTint = visual.tint;

      const auto botClipIt = std::find_if(
          data.clips.begin(), data.clips.end(),
          [&ovInfo](const Clip &c) { return c.id == ovInfo.bottomClipId; });
      if (botClipIt != data.clips.end() &&
          uiStates.contains(ovInfo.bottomClipId) && botClipIt->length > 0.001) {
        const auto &botUi = uiStates.at(ovInfo.bottomClipId);
        ui.cachedPartnerOverlapNotes.clear();
        for (const auto &bn : botUi.cachedVisualNotes) {
          const double absNoteStart =
              botClipIt->start +
              static_cast<double>(bn.startNormX) * botClipIt->length;
          const double absNoteEnd =
              botClipIt->start +
              static_cast<double>(bn.endNormX) * botClipIt->length;
          if (absNoteEnd <= ovInfo.startSec || absNoteStart >= ovInfo.endSec)
            continue;

          MidiNoteVisual projected = bn;
          projected.startNormX =
              static_cast<float>((absNoteStart - clip->start) / clip->length);
          projected.endNormX =
              static_cast<float>((absNoteEnd - clip->start) / clip->length);
          projected.selected = false;
          ui.cachedPartnerOverlapNotes.push_back(projected);
        }
        visual.overlap.partnerNotes = ui.cachedPartnerOverlapNotes.data();
        visual.overlap.numPartnerNotes =
            static_cast<int>(ui.cachedPartnerOverlapNotes.size());
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

    for (const auto *clipPtr : getClipsSortedByZOrder()) {
      const auto &clip = *clipPtr;
      if (const auto component = clipComponents.find(clip.id);
          component != clipComponents.end()) {
        component->second->toFront(false);
        component->second->setDecorationOverflows(topOverflow, bottomOverflow,
                                                  16.0f);
        component->second->setBodyBoundsInParent(
            clipBounds(clip, pixelsPerSecond));
      }
    }
  }

  struct HitResult {
    const Clip *clip = nullptr;
    TimelineClipZone zone = TimelineClipZone::none;
    int markerIdx = -1;
    int envelopeIdx = -1;
    int curveSegmentIdx = -1;
    int noteIdx = -1;
    int pcIdx = -1;
    juce::Rectangle<float> bounds;
  };

  HitResult hitTestZones(juce::Point<float> p) const {
    HitResult res;
    const auto pps = panelView.pixelsPerSecond(getWidth());
    const bool isDraggingWarp = (activeZone == TimelineClipZone::warpMarker ||
                                 activeZone == TimelineClipZone::topWarpRail);
    const bool isDraggingEnv = false;

    const auto sortedClips = getClipsSortedByZOrder();
    for (auto it = sortedClips.rbegin(); it != sortedClips.rend(); ++it) {
      const auto &c = **it;
      const auto b = clipBounds(c, pps);
      const auto feat = computeClipFeaturesLOD(clipFeatures, b.getHeight(),
                                               isDraggingWarp, isDraggingEnv);

      if (p.x >= b.getX() - 16.0f && p.x <= b.getRight() + 16.0f) {
        // A) Barra inferiore di taglio
        if (feat.showCutRail && p.x >= b.getX() && p.x <= b.getRight() &&
            p.y >= b.getBottom() && p.y <= b.getBottom() + 8.0f) {
          res.clip = &c;
          res.bounds = b;
          res.zone = TimelineClipZone::bottomCutRail;
          return res;
        }

        // B) Top Warp Rail / Header Top Area
        if (p.x >= b.getX() && p.x <= b.getRight() && p.y >= 0.0f &&
            p.y < b.getY()) {
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

        // C) Scrollbar verticale del pitch (sul lato sinistro o destro)
        const auto &ui = uiStates.at(c.id);
        if (ui.zoomLod != clip_midi_law::ZoomLod::preview &&
            ui.pitchScrollbarVisible) {
          const auto scrollbar = clips::computeMidiPitchScrollbarGeometry(
              b, ui.zoomLod, ui.displayMinPitch, ui.displayMaxPitch,
              ui.pitchScrollbarOnLeft);
          if (scrollbar.trackBounds.expanded(2.0f, 1.0f).contains(p)) {
            res.clip = &c;
            res.bounds = b;
            res.zone = TimelineClipZone::midiPitchScrollbar;
            return res;
          }
        }

        // D) Corpo della Clip MIDI
        if (p.y >= b.getY() && p.y < b.getBottom() && p.x >= b.getX() &&
            p.x <= b.getRight()) {
          res.clip = &c;
          res.bounds = b;
          constexpr float kTitleAreaHeight = 16.0f;

          // 1. Bracket laterali (0..2.5px all'interno dei bordi)
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

          // 3. Steli della lane Velocity. Lo stesso sourceIndex usato
          // dalle note preserva l'identita' dopo trim e slip.
          const auto midiLayout = clips::computeMidiClipLayout(b, ui.zoomLod);
          if (ui.zoomLod == clip_midi_law::ZoomLod::noteAndVelocityEdit &&
              midiLayout.velocityLaneBounds.contains(p)) {
            for (int ni = static_cast<int>(ui.cachedVisualNotes.size()) - 1;
                 ni >= 0; --ni) {
              const auto &note = ui.cachedVisualNotes[static_cast<size_t>(ni)];
              if (note.startNormX < 0.0f || note.startNormX > 1.0f) {
                continue;
              }
              const float x = b.getX() + note.startNormX * b.getWidth();
              if (std::abs(p.x - x) <= 6.0f) {
                res.zone = TimelineClipZone::midiVelocityStem;
                res.noteIdx = note.sourceIndex;
                return res;
              }
            }
            res.zone = TimelineClipZone::midiVelocityLane;
            return res;
          }

          // 5. Editing note disponibile solo nei due LOD espliciti.
          if (ui.zoomLod != clip_midi_law::ZoomLod::preview &&
              !ui.cachedVisualNotes.empty()) {
            for (int ni = static_cast<int>(ui.cachedVisualNotes.size()) - 1;
                 ni >= 0; --ni) {
              const auto &vn = ui.cachedVisualNotes[static_cast<size_t>(ni)];
              if (vn.endNormX < 0.0f || vn.startNormX > 1.0f)
                continue;

              const auto noteGeometry = clips::computeMidiNoteGeometry(
                  vn, b, ui.displayMinPitch, ui.displayMaxPitch, ui.isDrumMode,
                  ui.zoomLod);
              if (noteGeometry.bounds.expanded(1.5f).contains(p)) {
                res.noteIdx = vn.sourceIndex;
                if (!ui.isDrumMode && noteGeometry.bounds.getWidth() >= 14.0f) {
                  if (p.x >= noteGeometry.bounds.getRight() - 4.5f) {
                    res.zone = TimelineClipZone::midiNoteRightEdge;
                    return res;
                  }
                  if (p.x <= noteGeometry.bounds.getX() + 4.5f) {
                    res.zone = TimelineClipZone::midiNoteLeftEdge;
                    return res;
                  }
                }
                res.zone = TimelineClipZone::midiNoteBody;
                return res;
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
    const auto pps = panelView.pixelsPerSecond(getWidth());
    const auto now = juce::Time::getMillisecondCounter();

    for (const auto &c : data.clips) {
      if (!uiStates.contains(c.id))
        continue;
      auto &ui = uiStates[c.id];

      if (ui.zoomLod == clip_midi_law::ZoomLod::preview) {
        ui.pitchScrollbarVisible = false;
        ui.pitchScrollbarHideTimeMs = 0;
        continue;
      }

      const auto b = clipBounds(c, pps);
      const bool mouseInsideClip =
          (e.position.x >= b.getX() && e.position.x <= b.getRight() &&
           e.position.y >= 0.0f && e.position.y <= b.getBottom() + 8.0f);

      if (mouseInsideClip) {
        ui.pitchScrollbarVisible = true;
        ui.pitchScrollbarHideTimeMs = 0;

        const float midX = b.getX() + b.getWidth() * 0.5f;
        ui.pitchScrollbarOnLeft = (e.position.x < midX);
      } else if (ui.pitchScrollbarVisible) {
        const auto scrollbar = clips::computeMidiPitchScrollbarGeometry(
            b, ui.zoomLod, ui.displayMinPitch, ui.displayMaxPitch,
            ui.pitchScrollbarOnLeft);
        const bool mouseOverScrollbar =
            scrollbar.trackBounds.expanded(2.0f, 1.0f).contains(e.position);

        if (mouseOverScrollbar) {
          ui.pitchScrollbarHideTimeMs = 0;
        } else if (ui.pitchScrollbarHideTimeMs == 0) {
          ui.pitchScrollbarHideTimeMs = now + 1000;
          startTimer(50);
        }
      }
    }

    const auto hit = hitTestZones(e.position);

    if (hit.clip != nullptr) {
      hoveredClipId = hit.clip->id;
      activeZone = hit.zone;
      hoveredMarkerIdx = hit.markerIdx;
      hoveredEnvelopeIdx = -1;
      hoveredCurveSegmentIdx = -1;
      hoveredNoteIdx = hit.noteIdx;
      hoveredPcIdx = -1;

      const bool isNote = activeZone == TimelineClipZone::midiNoteBody ||
                          activeZone == TimelineClipZone::midiNoteLeftEdge ||
                          activeZone == TimelineClipZone::midiNoteRightEdge;
      const bool canErase = clips::isClipDeletionZone(activeZone) ||
                            activeZone == TimelineClipZone::warpMarker ||
                            (isNote && hit.noteIdx >= 0);
      setMouseCursor(
          clips::timelineMouseCursor(clips::resolveTimelineCursorRole(
              {activeZone, e.mods.isAltDown(), e.mods.isShiftDown(), true,
               canErase})));
    } else {
      hoveredClipId.clear();
      activeZone = TimelineClipZone::none;
      hoveredMarkerIdx = -1;
      hoveredEnvelopeIdx = -1;
      hoveredCurveSegmentIdx = -1;
      hoveredNoteIdx = -1;
      hoveredPcIdx = -1;

      isHoveringDivider = !panelView.enabled && (e.position.y >= getHeight() - 6.0f);
      setMouseCursor(isHoveringDivider ? juce::MouseCursor::UpDownResizeCursor
                                       : juce::MouseCursor::NormalCursor);
    }

    for (auto &[id, comp] : clipComponents)
      if (comp != nullptr)
        comp->repaint();
    repaint();
  }

  void modifierKeysChanged(const juce::ModifierKeys &modifiers) override {
    const auto active =
        dragZone != TimelineClipZone::none ? dragZone : activeZone;
    const bool isNote = active == TimelineClipZone::midiNoteBody ||
                        active == TimelineClipZone::midiNoteLeftEdge ||
                        active == TimelineClipZone::midiNoteRightEdge;
    const bool canErase =
        clips::isClipDeletionZone(active) ||
        active == TimelineClipZone::warpMarker ||
        (isNote && (dragZone != TimelineClipZone::none ? draggedNoteIdx
                                                       : hoveredNoteIdx) >= 0);
    setMouseCursor(clips::timelineMouseCursor(clips::resolveTimelineCursorRole(
        {active, modifiers.isAltDown(), modifiers.isShiftDown(), true,
         canErase})));
  }

  void mouseExit(const juce::MouseEvent &) override {
    const auto now = juce::Time::getMillisecondCounter();
    for (auto &[cid, ui] : uiStates) {
      if (ui.pitchScrollbarVisible && ui.pitchScrollbarHideTimeMs == 0) {
        ui.pitchScrollbarHideTimeMs = now + 1000;
        startTimer(50);
      }
    }

    hoveredClipId.clear();
    activeZone = TimelineClipZone::none;
    hoveredMarkerIdx = -1;
    hoveredEnvelopeIdx = -1;
    hoveredCurveSegmentIdx = -1;
    hoveredNoteIdx = -1;
    hoveredPcIdx = -1;
    isHoveringDivider = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    for (auto &[id, comp] : clipComponents)
      if (comp != nullptr)
        comp->repaint();
    repaint();
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const auto hit = hitTestZones(e.position);

    if (hit.clip != nullptr && e.mods.isPopupMenu()) {
      auto context = clips::makeTimelinePointerContext(
          data.id, hit.clip->id, clips::TimelinePointerTarget::clip,
          clips::ClipKind::midiNote, clips::semanticClipZone(hit.zone),
          e.position,
          panelView.xToTime(e.position.x,getWidth()), e.mods);
      context.markerIndex = hit.markerIdx;
      context.pointIndex = hit.envelopeIdx;
      context.segmentIndex = hit.curveSegmentIdx;
      context.noteIndex = hit.noteIdx;
      context.eventIndex = hit.pcIdx;
      const auto ovInfo =
          findOverlapAtTime(context.timelineSeconds, hit.clip->id);
      context.midiOverlapMode = midiOverlapMode;
      if (ovInfo.inOverlap) {
        context.isInClipOverlap = true;
        context.isInCrossfade = false;
        context.overlappedPartnerClipId = (ovInfo.bottomClipId == hit.clip->id)
                                              ? ovInfo.topClipId
                                              : ovInfo.bottomClipId;
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

    if (!hit.clip) {
      if (e.mods.isLeftButtonDown() && e.position.y >= getHeight() - 6.0f) {
        isResizingLaneHeight = true;
        resizeLaneStartY = e.getScreenY();
        resizeStartLaneHeight = height;
        return;
      }
      isMarqueeActive = marqueeSelector.handleMouseDown(e);
      return;
    }

    lastSelectedClipId = hit.clip->id;
    host.selectClip(hit.clip->id);
    layoutClipComponents();
    grabKeyboardFocus();

    auto &ui = uiStates[hit.clip->id];

    if (hit.zone == TimelineClipZone::none) {
      isMarqueeActive = marqueeSelector.handleMouseDown(e);
      return;
    }

    if (e.mods.isLeftButtonDown()) {
      snapshotUIState = ui;
      snapshotClip = *hit.clip;
      hasDragSnapshot = true;
    }

    draggedClipId = hit.clip->id;
    dragZone = hit.zone;
    draggedMarkerIdx = hit.markerIdx;
    draggedEnvelopeIdx = -1;
    draggedCurveSegmentIdx = -1;
    draggedNoteIdx = hit.noteIdx;
    draggedPcIdx = -1;
    dragStartPos = e.position;

    if (hit.zone == TimelineClipZone::midiPitchScrollbar) {
      draggedScrollbarOnLeft = ui.pitchScrollbarOnLeft;
      ui.pitchScrollbarVisible = true;
      ui.pitchScrollbarHideTimeMs = 0;
      setPitchViewportFromScrollbarY(ui, hit.bounds, draggedScrollbarOnLeft,
                                     e.position.y);
      updateMidiClip(draggedClipId);
      for (auto &[id, comp] : clipComponents)
        if (comp != nullptr)
          comp->repaint();
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::midiVelocityStem && hit.noteIdx >= 0 &&
        hit.noteIdx < static_cast<int>(ui.notes.size())) {
      auto &clickedNote = ui.notes[static_cast<size_t>(hit.noteIdx)];

      if (!clickedNote.selected && !e.mods.isCommandDown() &&
          !e.mods.isCtrlDown() && !e.mods.isShiftDown()) {
        for (auto &n : ui.notes)
          n.selected = false;
        clickedNote.selected = true;
      } else if (!clickedNote.selected &&
                 (e.mods.isCommandDown() || e.mods.isCtrlDown())) {
        clickedNote.selected = true;
      }

      startNotesSnapshot = ui.notes;
      velocityDragIndices.clear();
      draggedNoteIdx = hit.noteIdx;

      const bool useSelection =
          std::any_of(ui.notes.begin(), ui.notes.end(),
                      [](const MidiSourceNote &note) { return note.selected; });
      if (useSelection) {
        for (size_t i = 0; i < ui.notes.size(); ++i)
          if (ui.notes[i].selected)
            velocityDragIndices.push_back(static_cast<int>(i));
      } else {
        const double chordTolerance =
            hit.clip->length * (6.0 / juce::jmax(1.0f, hit.bounds.getWidth()));
        for (size_t i = 0; i < ui.notes.size(); ++i)
          if (std::abs(ui.notes[i].startSec - clickedNote.startSec) <=
              chordTolerance)
            velocityDragIndices.push_back(static_cast<int>(i));
      }
      return;
    }

    if (hit.zone == TimelineClipZone::midiVelocityLane) {
      startNotesSnapshot = ui.notes;
      lastVelocityPaintX = e.position.x;
      return;
    }

    // Alt + Click: eliminazione rapida di Warp Marker o Nota MIDI
    if (e.mods.isAltDown() && !e.mods.isPopupMenu()) {
      if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 &&
          hit.markerIdx < static_cast<int>(ui.warpPoints.size())) {
        ui.warpPoints.erase(ui.warpPoints.begin() + hit.markerIdx);
        updateMidiClip(draggedClipId);
        resetDragState();
        repaint();
        return;
      }
      if ((hit.zone == TimelineClipZone::midiNoteBody ||
           hit.zone == TimelineClipZone::midiNoteLeftEdge ||
           hit.zone == TimelineClipZone::midiNoteRightEdge) &&
          hit.noteIdx >= 0 && hit.noteIdx < static_cast<int>(ui.notes.size())) {
        ui.notes.erase(ui.notes.begin() + hit.noteIdx);
        updateMidiClip(draggedClipId);
        resetDragState();
        repaint();
        return;
      }
    }

    if (!e.mods.isLeftButtonDown()) {
      resetDragState();
      return;
    }

    // Selezione Nota MIDI In-Place
    if ((hit.zone == TimelineClipZone::midiNoteBody ||
         hit.zone == TimelineClipZone::midiNoteLeftEdge ||
         hit.zone == TimelineClipZone::midiNoteRightEdge) &&
        hit.noteIdx >= 0 && hit.noteIdx < static_cast<int>(ui.notes.size())) {
      auto &note = ui.notes[static_cast<size_t>(hit.noteIdx)];
      if (e.mods.isShiftDown()) {
        note.selected = true;
      } else if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
        note.selected = !note.selected;
      } else if (!note.selected) {
        for (auto &n : ui.notes)
          n.selected = false;
        note.selected = true;
      }
      startNotesSnapshot = ui.notes;
      ui.freezePitchViewport = true;
      updateMidiClip(draggedClipId);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::clipTitleMove) {
      startClipStart = hit.clip->start;
      return;
    }

    // Click su Top Warp Rail: inserisce un Warp Marker (con auto-snap magnetico
    // all'attacco nota entro 15ms)
    if (dragZone == TimelineClipZone::topWarpRail && hit.markerIdx < 0) {
      const float normX = juce::jlimit(0.01f, 0.99f,
                                       (e.position.x - hit.bounds.getX()) /
                                           hit.bounds.getWidth());
      double sampleTime = (ui.isStretching || ui.stretchTicks != 0)
                              ? (ui.startSourceOffsetSec +
                                 static_cast<double>(normX) * ui.origClipLength)
                              : (ui.sourceOffsetSec +
                                 static_cast<double>(normX) * hit.clip->length);

      // Auto-snap magnetico all'attacco nota piu' vicino
      double bestDist = 0.08; // 80ms finestra magnetica
      for (const auto &n : ui.notes) {
        const double d = std::abs(n.startSec - sampleTime);
        if (d < bestDist) {
          bestDist = d;
          sampleTime = n.startSec;
        }
      }

      const double minimumMarkerDistance =
          std::max(0.005, hit.clip->length * 0.015);
      const auto duplicate = std::find_if(
          ui.warpPoints.begin(), ui.warpPoints.end(),
          [sampleTime, minimumMarkerDistance](const MidiWarpPoint &marker) {
            return std::abs(marker.sourceTimeSec - sampleTime) <
                   minimumMarkerDistance;
          });
      if (duplicate != ui.warpPoints.end()) {
        resetDragState();
        repaint();
        return;
      }

      ui.warpPoints.push_back({sampleTime, sampleTime});
      std::sort(ui.warpPoints.begin(), ui.warpPoints.end(),
                [](const MidiWarpPoint &a, const MidiWarpPoint &b) {
                  return a.sourceTimeSec < b.sourceTimeSec;
                });
      updateMidiClip(draggedClipId);
      resetDragState();
      repaint();
      return;
    }

    // Click su Bottom Cut Rail: taglia la clip MIDI con CC Chase e divisione
    // note
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
        if (!newId.empty()) {
          splitMidiClipUI(targetId, newId, splitFrac, e.mods.isAltDown());
          updateMidiClip(targetId);
          updateMidiClip(newId);
        }
      }
      resetDragState();
      repaint();
      return;
    }

    startClipStart = hit.clip->start;
    startClipLength = hit.clip->length;
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

    if (dragZone == TimelineClipZone::midiPitchScrollbar) {
      ui.pitchScrollbarVisible = true;
      ui.pitchScrollbarHideTimeMs = 0;
      ui.pitchScrollbarOnLeft = draggedScrollbarOnLeft;
      setPitchViewportFromScrollbarY(ui, b, draggedScrollbarOnLeft,
                                     e.position.y);
      updateMidiClip(draggedClipId);
      for (auto &[id, comp] : clipComponents)
        if (comp != nullptr)
          comp->repaint();
      repaint();
      return;
    }

    if (dragZone == TimelineClipZone::midiVelocityStem &&
        !velocityDragIndices.empty()) {
      const auto lane =
          clips::computeMidiClipLayout(b, ui.zoomLod).velocityLaneBounds;
      const float laneUsableH = juce::jmax(1.0f, lane.getHeight() - 10.0f);
      const int refIdx = (draggedNoteIdx >= 0 &&
                          draggedNoteIdx < static_cast<int>(ui.notes.size()))
                             ? draggedNoteIdx
                             : velocityDragIndices.front();
      const int origRefVel =
          (refIdx >= 0 && refIdx < static_cast<int>(startNotesSnapshot.size()))
              ? static_cast<int>(
                    startNotesSnapshot[static_cast<size_t>(refIdx)].velocity)
              : 100;

      const float rawNorm = juce::jlimit(
          0.0f, 1.0f, (lane.getBottom() - 4.0f - e.position.y) / laneUsableH);
      int rawTargetVel =
          juce::jlimit(1, 127, juce::roundToInt(rawNorm * 127.0f));

      // Snap ogni 10 (con max 127) tenendo premuto Shift
      if (e.mods.isShiftDown()) {
        if (rawTargetVel >= 124)
          rawTargetVel = 127;
        else
          rawTargetVel = juce::jlimit(
              10, 120, juce::roundToInt(rawTargetVel / 10.0f) * 10);
      }

      const int deltaVelocity = rawTargetVel - origRefVel;

      for (const int index : velocityDragIndices) {
        if (index < 0 || index >= static_cast<int>(ui.notes.size()) ||
            index >= static_cast<int>(startNotesSnapshot.size()))
          continue;

        const int origV = static_cast<int>(
            startNotesSnapshot[static_cast<size_t>(index)].velocity);
        const int nextVelocity =
            e.mods.isAltDown()
                ? rawTargetVel
                : (index == refIdx
                       ? rawTargetVel
                       : juce::jlimit(1, 127, origV + deltaVelocity));
        ui.notes[static_cast<size_t>(index)].velocity =
            static_cast<std::uint8_t>(nextVelocity);
      }

      // Feedback HUD visibile fuori dal cubetto (sopra la testa dello stelo)
      isDraggingVelocityFeedback = true;
      draggedVelocityFeedbackValue = static_cast<std::uint8_t>(rawTargetVel);
      draggedVelocityFeedbackVisibleInNote = false;

      float stemX = e.position.x;
      for (const auto &vn : ui.cachedVisualNotes) {
        if (vn.sourceIndex == refIdx) {
          stemX = b.getX() + vn.startNormX * b.getWidth();
          break;
        }
      }
      const float stemY =
          (lane.getBottom() - 4.0f) -
          (static_cast<float>(rawTargetVel) / 127.0f) * laneUsableH;
      draggedVelocityFeedbackPos = juce::Point<float>(stemX, stemY);
      isDraggingPitchFeedback = false;

      updateMidiClip(draggedClipId);
      repaint();
      return;
    }

    if (dragZone == TimelineClipZone::midiVelocityLane) {
      const auto lane =
          clips::computeMidiClipLayout(b, ui.zoomLod).velocityLaneBounds;
      if (lane.isEmpty()) {
        return;
      }

      const auto velocityAtY = [&lane, &e](const float y) noexcept {
        const float norm =
            juce::jlimit(0.0f, 1.0f,
                         (lane.getBottom() - 4.0f - y) /
                             juce::jmax(1.0f, lane.getHeight() - 10.0f));
        int vel = juce::jlimit(1, 127, juce::roundToInt(norm * 127.0f));
        if (e.mods.isShiftDown()) {
          if (vel >= 124)
            vel = 127;
          else
            vel = juce::jlimit(10, 120, juce::roundToInt(vel / 10.0f) * 10);
        }
        return vel;
      };

      const float x0 = juce::jmin(dragStartPos.x, e.position.x);
      const float x1 = juce::jmax(dragStartPos.x, e.position.x);
      const bool hasSelection =
          std::any_of(ui.notes.begin(), ui.notes.end(),
                      [](const MidiSourceNote &note) { return note.selected; });

      int lastTargetVel = 100;
      if (e.mods.isShiftDown() || e.mods.isAltDown()) {
        ui.notes = startNotesSnapshot;
        const int startVelocity = velocityAtY(dragStartPos.y);
        const int endVelocity = velocityAtY(e.position.y);
        lastTargetVel = endVelocity;
        const int leftVelocity =
            dragStartPos.x <= e.position.x ? startVelocity : endVelocity;
        const int rightVelocity =
            dragStartPos.x <= e.position.x ? endVelocity : startVelocity;
        const float span = juce::jmax(1.0f, x1 - x0);

        for (const auto &visualNote : ui.cachedVisualNotes) {
          const int index = visualNote.sourceIndex;
          if (index < 0 || index >= static_cast<int>(ui.notes.size())) {
            continue;
          }
          const float noteX = b.getX() + visualNote.startNormX * b.getWidth();
          if (noteX < x0 || noteX > x1) {
            continue;
          }
          if (hasSelection && !ui.notes[static_cast<size_t>(index)].selected) {
            continue;
          }

          const float t = juce::jlimit(0.0f, 1.0f, (noteX - x0) / span);
          const int velocity = e.mods.isAltDown()
                                   ? endVelocity
                                   : juce::roundToInt(juce::jmap(
                                         t, static_cast<float>(leftVelocity),
                                         static_cast<float>(rightVelocity)));
          ui.notes[static_cast<size_t>(index)].velocity =
              static_cast<std::uint8_t>(juce::jlimit(1, 127, velocity));
        }
      } else {
        const float paintMinX =
            juce::jmin(lastVelocityPaintX, e.position.x) - 3.0f;
        const float paintMaxX =
            juce::jmax(lastVelocityPaintX, e.position.x) + 3.0f;
        const auto velocity =
            static_cast<std::uint8_t>(velocityAtY(e.position.y));
        lastTargetVel = velocity;
        for (const auto &visualNote : ui.cachedVisualNotes) {
          const int index = visualNote.sourceIndex;
          if (index < 0 || index >= static_cast<int>(ui.notes.size())) {
            continue;
          }
          const float noteX = b.getX() + visualNote.startNormX * b.getWidth();
          if (noteX >= paintMinX && noteX <= paintMaxX)
            ui.notes[static_cast<size_t>(index)].velocity = velocity;
        }
        lastVelocityPaintX = e.position.x;
      }

      isDraggingVelocityFeedback = true;
      draggedVelocityFeedbackValue = static_cast<std::uint8_t>(lastTargetVel);
      draggedVelocityFeedbackVisibleInNote = false;
      draggedVelocityFeedbackPos = e.position;
      isDraggingPitchFeedback = false;

      updateMidiClip(draggedClipId);
      repaint();
      return;
    }

    // A) Spostamento clip tramite titolo
    if (dragZone == TimelineClipZone::clipTitleMove) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      cl->start = std::max(0.0, startClipStart + deltaSec);
      layoutClipComponents();
      repaint();
      return;
    }

    // C) Editing In-Place delle Note MIDI (Pitch, Velocity, Start, Durata)
    if ((dragZone == TimelineClipZone::midiNoteBody ||
         dragZone == TimelineClipZone::midiNoteLeftEdge ||
         dragZone == TimelineClipZone::midiNoteRightEdge) &&
        draggedNoteIdx >= 0 &&
        draggedNoteIdx < static_cast<int>(ui.notes.size())) {
      const double editableTimeSpan =
          (ui.isStretching || ui.stretchTicks != 0) && ui.origClipLength > 0.05
              ? ui.origClipLength
              : cl->length;
      const double deltaSec =
          ((e.position.x - dragStartPos.x) / b.getWidth()) * editableTimeSpan;
      const int pitchSpan =
          std::max(1, static_cast<int>(ui.displayMaxPitch) -
                          static_cast<int>(ui.displayMinPitch) + 1);
      const auto pianoBounds =
          clips::computeMidiClipLayout(b, ui.zoomLod).pianoRollBounds;
      const float rowH = std::max(2.0f, pianoBounds.getHeight() /
                                            static_cast<float>(pitchSpan));
      const int deltaPitch =
          juce::roundToInt(-(e.position.y - dragStartPos.y) / rowH);
      const int deltaVel =
          juce::roundToInt((e.position.x - dragStartPos.x) * 0.8f);
      const double totalDuration = std::max(0.1, ui.sourceTotalDurationSec > 0.0
                                                     ? ui.sourceTotalDurationSec
                                                     : cl->length + 8.0);

      for (size_t i = 0; i < ui.notes.size() && i < startNotesSnapshot.size();
           ++i) {
        if (!ui.notes[i].selected)
          continue;

        const auto &origN = startNotesSnapshot[i];
        if (dragZone == TimelineClipZone::midiNoteBody) {
          if (e.mods.isShiftDown()) {
            // Shift mantiene tempo e pitch: cambia solo la velocity con drag
            // orizzontale.
            ui.notes[i].velocity = static_cast<std::uint8_t>(juce::jlimit(
                1, 127, static_cast<int>(origN.velocity) + deltaVel));
          } else {
            const double originalWarpedStart =
                mapSourceSecToWarpedSec(origN.startSec, ui, totalDuration);
            ui.notes[i].startSec = std::max(
                0.0, mapWarpedSecToSourceSec(originalWarpedStart + deltaSec, ui,
                                             totalDuration));
            if (!ui.isDrumMode)
              ui.notes[i].pitch = static_cast<std::uint8_t>(juce::jlimit(
                  0, 127, static_cast<int>(origN.pitch) + deltaPitch));
          }
        } else if (dragZone == TimelineClipZone::midiNoteRightEdge) {
          const double originalEnd = origN.startSec + origN.lengthSec;
          const double originalWarpedEnd =
              mapSourceSecToWarpedSec(originalEnd, ui, totalDuration);
          const double newEnd = mapWarpedSecToSourceSec(
              originalWarpedEnd + deltaSec, ui, totalDuration);
          ui.notes[i].lengthSec = std::max(0.04, newEnd - origN.startSec);
        } else if (dragZone == TimelineClipZone::midiNoteLeftEdge) {
          const double endSec = origN.startSec + origN.lengthSec;
          const double latestStart = std::max(0.0, endSec - 0.04);
          const double originalWarpedStart =
              mapSourceSecToWarpedSec(origN.startSec, ui, totalDuration);
          const double mappedStart = mapWarpedSecToSourceSec(
              originalWarpedStart + deltaSec, ui, totalDuration);
          const double newStart = std::clamp(mappedStart, 0.0, latestStart);
          ui.notes[i].startSec = newStart;
          ui.notes[i].lengthSec = endSec - newStart;
        }
      }

      if (dragZone == TimelineClipZone::midiNoteBody) {
        const auto &targetNote = ui.notes[static_cast<size_t>(draggedNoteIdx)];
        juce::Rectangle<float> targetNoteRect;
        for (const auto &vn : ui.cachedVisualNotes) {
          if (vn.sourceIndex == draggedNoteIdx) {
            targetNoteRect = clips::computeMidiNoteGeometry(
                                 vn, b, ui.displayMinPitch, ui.displayMaxPitch,
                                 ui.isDrumMode, ui.zoomLod)
                                 .bounds;
            break;
          }
        }
        if (targetNoteRect.isEmpty()) {
          targetNoteRect = juce::Rectangle<float>(
              e.position.x - 15.0f, e.position.y - 7.0f, 30.0f, 14.0f);
        }

        if (e.mods.isShiftDown()) {
          setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
          isDraggingVelocityFeedback = true;
          draggedVelocityFeedbackValue = targetNote.velocity;
          draggedVelocityFeedbackVisibleInNote = false;
          draggedVelocityFeedbackPos = e.position;
          isDraggingPitchFeedback = false;
        } else {
          isDraggingPitchFeedback = true;
          draggedPitchFeedbackValue = targetNote.pitch;
          draggedPitchFeedbackPos = e.position;
          isDraggingVelocityFeedback = false;
        }
      }

      updateMidiClip(draggedClipId);
      repaint();
      return;
    }

    // E) Edge Tools: Loop / Slip / Trim / Time-Stretch
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
      updateMidiClip(draggedClipId);
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
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::edgeLeftSlip) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double totalDur = ui.sourceTotalDurationSec > 0.0
                                  ? ui.sourceTotalDurationSec
                                  : (startClipLength + 8.0);
      const double maxExpand =
          std::max(0.0, totalDur - (startSourceOffsetSec + startClipLength));
      const double clampedDelta =
          std::clamp(deltaSec, -std::min(maxExpand, startClipStart),
                     startClipLength - 0.125);
      cl->start = startClipStart + clampedDelta;
      cl->length = startClipLength - clampedDelta;
      ui.sourceOffsetSec = startSourceOffsetSec;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      ui.isSlipping = true;
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightSlip) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double maxExpand = std::max(0.0, startSourceOffsetSec);
      const double clampedDelta =
          std::clamp(deltaSec, -(startClipLength - 0.125), maxExpand);
      cl->length = startClipLength + clampedDelta;
      ui.sourceOffsetSec = startSourceOffsetSec - clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      ui.isSlipping = true;
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::edgeLeftTrim) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double maxExpand = std::max(0.0, startSourceOffsetSec);
      const double clampedDelta =
          std::clamp(deltaSec, -std::min(maxExpand, startClipStart),
                     startClipLength - 0.125);
      cl->start = startClipStart + clampedDelta;
      cl->length = startClipLength - clampedDelta;
      ui.sourceOffsetSec = startSourceOffsetSec + clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightTrim) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      const double totalDur = ui.sourceTotalDurationSec > 0.0
                                  ? ui.sourceTotalDurationSec
                                  : (startClipLength + 8.0);
      const double maxExpand =
          std::max(0.0, totalDur - (startSourceOffsetSec + startClipLength));
      const double clampedDelta =
          std::clamp(deltaSec, -(startClipLength - 0.125), maxExpand);
      cl->length = startClipLength + clampedDelta;
      ui.origClipLength = cl->length;
      ui.stretchTicks = 0;
      ui.isStretching = false;
      updateMidiClip(draggedClipId);
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
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::edgeRightStretch) {
      const double deltaSec = (e.position.x - dragStartPos.x) / pps;
      cl->length = std::max(0.125, startClipLength + deltaSec);
      ui.stretchTicks = static_cast<std::int64_t>(
          std::round((cl->length - ui.origClipLength) * 960.0));
      ui.isStretching = true;
      updateMidiClip(draggedClipId);
      repaint();
    } else if (dragZone == TimelineClipZone::warpMarker &&
               draggedMarkerIdx >= 0) {
      // F) Warp Marker: Linked (Note + CC) di default, oppure Unlinked (Solo
      // Note) con Shift!
      if (draggedMarkerIdx < static_cast<int>(ui.warpPoints.size())) {
        ui.isUnlinkedNoteWarp = e.mods.isShiftDown();

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

        updateMidiClip(draggedClipId);
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
      for (const auto &c : data.clips) {
        if (c.id == draggedClipId) {
          if (std::abs(c.start - startClipStart) > 0.000001)
            host.moveClip(data.id, draggedClipId, c.start);
          break;
        }
      }
    }

    if (dragZone == TimelineClipZone::edgeLeftLoop ||
        dragZone == TimelineClipZone::edgeRightLoop) {
      if (loopPreview.active && !draggedClipId.empty()) {
        if (loopPreview.duplicateCount > 0) {
          const auto created = host.duplicateClipLoop(
              data.id, draggedClipId, loopPreview.duplicateCount,
              loopPreview.isRightEdge);
          duplicateMidiClipLoopUI(draggedClipId, created);
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
      ui.isUnlinkedNoteWarp = false;
      if (ui.freezePitchViewport) {
        ui.freezePitchViewport = false;
        updateMidiClip(draggedClipId);
      }
    }

    if (dragZone == TimelineClipZone::midiPitchScrollbar &&
        !draggedClipId.empty()) {
      if (uiStates.contains(draggedClipId)) {
        auto &ui = uiStates[draggedClipId];
        const auto pps = panelView.pixelsPerSecond(getWidth());
        const Clip *cl = nullptr;
        for (const auto &c : data.clips)
          if (c.id == draggedClipId) {
            cl = &c;
            break;
          }
        if (cl != nullptr) {
          const auto b = clipBounds(*cl, pps);
          const bool insideClip =
              (e.position.x >= b.getX() && e.position.x <= b.getRight() &&
               e.position.y >= 0.0f && e.position.y <= b.getBottom() + 8.0f);
          const auto scrollbar = clips::computeMidiPitchScrollbarGeometry(
              b, ui.zoomLod, ui.displayMinPitch, ui.displayMaxPitch,
              ui.pitchScrollbarOnLeft);
          const bool overScrollbar =
              scrollbar.trackBounds.expanded(2.0f, 1.0f).contains(e.position);
          if (!insideClip && !overScrollbar) {
            ui.pitchScrollbarHideTimeMs =
                juce::Time::getMillisecondCounter() + 1000;
            startTimer(50);
          } else {
            ui.pitchScrollbarHideTimeMs = 0;
          }
        }
      }
    }

    resetDragState();
    for (auto &[id, comp] : clipComponents)
      if (comp != nullptr)
        comp->repaint();
    repaint();
  }

  bool keyPressed(const juce::KeyPress &key) override {
    if (key.isKeyCode(juce::KeyPress::escapeKey) && !draggedClipId.empty() &&
        hasDragSnapshot) {
      const auto clip = std::find_if(data.clips.begin(), data.clips.end(),
                                     [this](const Clip &candidate) {
                                       return candidate.id == draggedClipId;
                                     });
      if (clip != data.clips.end() && uiStates.contains(draggedClipId)) {
        uiStates[draggedClipId] = snapshotUIState;
        *clip = snapshotClip;
        updateMidiClip(draggedClipId);
      }
      resetDragState();
      repaint();
      return true;
    }

    if (key.getKeyCode() == juce::KeyPress::backspaceKey ||
        key.getKeyCode() == juce::KeyPress::deleteKey) {
      bool anyChanged = false;
      for (auto &[cid, ui] : uiStates) {

        for (auto it = ui.notes.begin(); it != ui.notes.end();) {
          if (it->selected) {
            it = ui.notes.erase(it);
            anyChanged = true;
          } else
            ++it;
        }
        if (anyChanged)
          updateMidiClip(cid);
      }
      if (anyChanged) {
        repaint();
        return true;
      }
    }
    return false;
  }

  void focusLost(juce::Component::FocusChangeType) override {
    if (!draggedClipId.empty() && hasDragSnapshot) {
      const auto clipId = draggedClipId;
      const auto clip = std::find_if(
          data.clips.begin(), data.clips.end(),
          [&clipId](const Clip &candidate) { return candidate.id == clipId; });
      if (clip != data.clips.end() && uiStates.contains(clipId)) {
        uiStates[clipId] = snapshotUIState;
        *clip = snapshotClip;
        updateMidiClip(clipId);
      }
    }
    resetDragState();
    repaint();
  }

  void mouseDoubleClick(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu())
      return;

    const auto hit = hitTestZones(e.position);


    if (!panelView.enabled && e.position.y >= getHeight() - 6.0f) {
      const int modelHeight = clip_height_law::nextModelHeight(
          static_cast<float>(height), host.getVerticalZoom(), true);
      host.store().resize(data.id, modelHeight);
      return;
    }

    if (hit.clip == nullptr)
      return;

    auto &ui = uiStates[hit.clip->id];

    if (hit.zone == TimelineClipZone::midiPitchScrollbar) {
      setPitchViewportCentre(ui, clip_midi_law::centreC3Pitch, true);
      updateMidiClip(hit.clip->id);
      repaint();
      return;
    }

    if (hit.zone == TimelineClipZone::midiVelocityStem && hit.noteIdx >= 0 &&
        hit.noteIdx < static_cast<int>(ui.notes.size())) {
      const auto clickedStart =
          ui.notes[static_cast<size_t>(hit.noteIdx)].startSec;
      const double chordTolerance =
          hit.clip->length * (6.0 / juce::jmax(1.0f, hit.bounds.getWidth()));
      for (auto &note : ui.notes)
        if (std::abs(note.startSec - clickedStart) <= chordTolerance)
          note.velocity = 100;
      updateMidiClip(hit.clip->id);
      repaint();
      return;
    }

    // Reset Stretch
    if (hit.zone == TimelineClipZone::edgeLeftStretch ||
        hit.zone == TimelineClipZone::edgeRightStretch) {
      if (ui.origClipLength > 0.125) {
        ui.stretchTicks = 0;
        ui.isStretching = false;
        host.resizeClip(data.id, hit.clip->id, hit.clip->start,
                        ui.origClipLength);
        updateMidiClip(hit.clip->id);
        repaint();
        return;
      }
    }

    // Reset Slip
    if (hit.zone == TimelineClipZone::edgeLeftSlip ||
        hit.zone == TimelineClipZone::edgeRightSlip) {
      ui.sourceOffsetSec = ui.defaultSourceOffsetSec;
      ui.slipDeltaTicks = 0;
      updateMidiClip(hit.clip->id);
      repaint();
      return;
    }

    // Reset Warp Marker a posizione originale
    if (hit.zone == TimelineClipZone::warpMarker && hit.markerIdx >= 0 &&
        hit.markerIdx < static_cast<int>(ui.warpPoints.size())) {
      ui.warpPoints[static_cast<size_t>(hit.markerIdx)].warpedTimeSec =
          ui.warpPoints[static_cast<size_t>(hit.markerIdx)].sourceTimeSec;
      updateMidiClip(hit.clip->id);
      repaint();
      return;
    }
    if (ui.zoomLod != clip_midi_law::ZoomLod::preview &&
        hit.zone == TimelineClipZone::none) {
      const auto pianoBounds =
          clips::computeMidiClipLayout(hit.bounds, ui.zoomLod).pianoRollBounds;
      if (!pianoBounds.contains(e.position)) {
        return;
      }
      const int pitchSpan =
          std::max(1, static_cast<int>(ui.displayMaxPitch) -
                          static_cast<int>(ui.displayMinPitch) + 1);
      const float relY = juce::jlimit(0.0f, 0.999f,
                                      (e.position.y - pianoBounds.getY()) /
                                          pianoBounds.getHeight());
      const int clickedPitch = juce::jlimit(
          0, 127,
          static_cast<int>(ui.displayMaxPitch) -
              static_cast<int>(relY * pitchSpan) - ui.transposeSemitones);

      const float normX = juce::jlimit(0.0f, 0.95f,
                                       (e.position.x - hit.bounds.getX()) /
                                           hit.bounds.getWidth());
      const double editableTimeSpan =
          (ui.isStretching || ui.stretchTicks != 0) && ui.origClipLength > 0.05
              ? ui.origClipLength
              : hit.clip->length;
      const double visibleTime =
          ui.sourceOffsetSec + static_cast<double>(normX) * editableTimeSpan;
      const double totalDuration = std::max(0.1, ui.sourceTotalDurationSec > 0.0
                                                     ? ui.sourceTotalDurationSec
                                                     : hit.clip->length + 8.0);
      const double startSec =
          mapWarpedSecToSourceSec(visibleTime, ui, totalDuration);

      ui.notes.push_back({startSec, 0.25,
                          static_cast<std::uint8_t>(clickedPitch), 104, 1, true,
                          false});
      updateMidiClip(hit.clip->id);
      repaint();
      return;
    }
    // Surface without a separate double-click action: cycle ONLY the lane's
    // presentation height. Other semantic targets retain their own handlers.
    if (hit.clip && clip_height_law::isUnclaimedDoubleClickZone(hit.zone,hit.markerIdx)) {
      if (panelView.enabled && panelView.cycleHeight) panelView.cycleHeight();
      else host.store().resize(data.id, clip_height_law::nextModelHeight(
          static_cast<float>(height), host.getVerticalZoom(), true));
      return;
    }

  }

private:
  void resetDragState() noexcept {
    draggedClipId.clear();
    dragZone = TimelineClipZone::none;
    draggedMarkerIdx = -1;
    draggedEnvelopeIdx = -1;
    draggedCurveSegmentIdx = -1;
    draggedNoteIdx = -1;
    draggedPcIdx = -1;
    velocityDragIndices.clear();
    isDraggingAllEnvelope = false;
    hasDragSnapshot = false;
    isDraggingPitchFeedback = false;
    isDraggingVelocityFeedback = false;
    loopPreview = {};
  }

  ClipFeatures clipFeatures;
  LabComponent &host;
  Track data;
  int height = 152;
  bool programAudible = true;

  std::map<std::string, MidiClipUIState> uiStates;
  clips::MidiOverlapMode midiOverlapMode = clips::MidiOverlapMode::replace;
  std::string lastSelectedClipId;
  std::map<std::string, std::unique_ptr<MidiClipComponent>> clipComponents;

  ayra::MarqueeSelector marqueeSelector{*this};
  bool isMarqueeActive = false;

  double startClipStart = 0.0;
  double startClipLength = 0.0;
  double startSourceOffsetSec = 0.0;
  clips::ClipLoopDragState loopPreview;
  std::string hoveredClipId;
  TimelineClipZone activeZone = TimelineClipZone::none;
  int hoveredMarkerIdx = -1;
  int hoveredEnvelopeIdx = -1;
  int hoveredCurveSegmentIdx = -1;
  int hoveredNoteIdx = -1;
  int hoveredPcIdx = -1;
  juce::Point<float> mousePos{-1.0f, -1.0f};

  std::string draggedClipId;
  TimelineClipZone dragZone = TimelineClipZone::none;
  int draggedMarkerIdx = -1;
  int draggedEnvelopeIdx = -1;
  int draggedCurveSegmentIdx = -1;
  int draggedNoteIdx = -1;
  int draggedPcIdx = -1;
  juce::Point<float> dragStartPos;

  std::vector<MidiEnvelopePoint> startEnvelopePoints;
  std::vector<MidiSourceNote> startNotesSnapshot;
  std::vector<int> velocityDragIndices;
  float lastVelocityPaintX = 0.0f;
  float startSegmentTension = 0.5f;
  bool isDraggingAllEnvelope = false;
  bool draggedScrollbarOnLeft = false;
  bool hasDragSnapshot = false;
  MidiClipUIState snapshotUIState;
  Clip snapshotClip;

  bool isDraggingPitchFeedback = false;
  std::uint8_t draggedPitchFeedbackValue = 0;
  juce::Point<float> draggedPitchFeedbackPos;

  bool isDraggingVelocityFeedback = false;
  std::uint8_t draggedVelocityFeedbackValue = 0;
  bool draggedVelocityFeedbackVisibleInNote = false;
  juce::Point<float> draggedVelocityFeedbackPos;

  bool isHoveringDivider = false;
  bool isResizingLaneHeight = false;
  int resizeLaneStartY = 0;
  int resizeStartLaneHeight = 0;
};

} // namespace
} // namespace ayra::lab
