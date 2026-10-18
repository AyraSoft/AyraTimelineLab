#include "LabComponent.h"
#include "TimelineVisualHarnessComponent.h"
#include <algorithm>
#include <set>
#include <tuple>
#include <cmath>
#include "clips/audio/ayra_AudioClipCanvas.cpp"
#include "clips/midi/ayra_MidiClipCanvas.cpp"
#include "clips/line/ayra_LineClipCanvas.cpp"
#include "clips/keyframe/ayra_KeyframeLaneCanvas.cpp"
#include "clips/continous/ayra_ContinuousLineCanvas.cpp"
#include "clips/image/ayra_ImageClipCanvas.cpp"

namespace ayra::lab
{
namespace
{
juce::String str(const std::string& x){return juce::String::fromUTF8(x.c_str());}
bool hasItem(const std::vector<std::string>& v,const std::string& x){return std::find(v.begin(),v.end(),x)!=v.end();}

class HorizontalTrackHeaderBar final : public juce::Component, public juce::Label::Listener, public juce::TooltipClient
{
public:
    static constexpr int kHeaderHeight = 26;
    enum class TrackLaneKind { audio, midi, lineClip, pointKeyframe, spanKeyframe, disjointSpanKeyframe, continuousLine, imageSequence };

    HorizontalTrackHeaderBar(LabComponent& h) : host(h)
    {
        nameLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
        nameLabel.setEditable(true, true, false); // click or double-click to edit
        nameLabel.addListener(this);
        nameLabel.setTooltip("Nome Traccia\nClicca per rinominare la traccia");
        addAndMakeVisible(nameLabel);

        modeButton.setTooltip("Modalita' Corsia\nClicca per commutare la corsia tra Audio, MIDI, Line, Marker, Span, Block, Tempo e Image");
        modeButton.onClick = [this]
        {
            switch (laneKind)
            {
                case TrackLaneKind::audio:                laneKind = TrackLaneKind::midi; break;
                case TrackLaneKind::midi:                 laneKind = TrackLaneKind::lineClip; break;
                case TrackLaneKind::lineClip:             laneKind = TrackLaneKind::pointKeyframe; break;
                case TrackLaneKind::pointKeyframe:        laneKind = TrackLaneKind::spanKeyframe; break;
                case TrackLaneKind::spanKeyframe:         laneKind = TrackLaneKind::disjointSpanKeyframe; break;
                case TrackLaneKind::disjointSpanKeyframe: laneKind = TrackLaneKind::continuousLine; break;
                case TrackLaneKind::continuousLine:       laneKind = TrackLaneKind::imageSequence; break;
                case TrackLaneKind::imageSequence:        laneKind = TrackLaneKind::audio; break;
            }
            updateModeButtonText();
            if (onLaneKindChanged)
                onLaneKindChanged(laneKind);
        };
        addAndMakeVisible(modeButton);
    }

    ~HorizontalTrackHeaderBar() override
    {
        nameLabel.removeListener(this);
    }

    juce::String getTooltip() override
    {
        return "Intestazione Traccia: " + str(track.name) + "\nRuolo: " +
               (track.role == Role::songTrack ? "Song Track (ST)" :
                track.role == Role::performerTrack ? "Performer Track (T)" :
                track.role == Role::performerMix ? "Performer Mix (M)" : "Song Main (S)") +
               "\nClicca per selezionare (Shift/Cmd per selezione multipla), doppio click per espandere/comprimere, trascina per riordinare";
    }

    void setModel(const Track& t, bool isSelected, TrackLaneKind kind)
    {
        track = t;
        selected = isSelected;
        laneKind = kind;
        if (!nameLabel.isBeingEdited())
            nameLabel.setText(str(track.name), juce::dontSendNotification);
        updateModeButtonText();
        repaint();
    }

    void updateModeButtonText()
    {
        switch (laneKind)
        {
            case TrackLaneKind::audio:                modeButton.setButtonText("AUDIO"); break;
            case TrackLaneKind::midi:                 modeButton.setButtonText("MIDI"); break;
            case TrackLaneKind::lineClip:             modeButton.setButtonText("LINE"); break;
            case TrackLaneKind::pointKeyframe:        modeButton.setButtonText("MARK"); break;
            case TrackLaneKind::spanKeyframe:         modeButton.setButtonText("SPAN"); break;
            case TrackLaneKind::disjointSpanKeyframe: modeButton.setButtonText("BLOCK"); break;
            case TrackLaneKind::continuousLine:       modeButton.setButtonText("TEMPO"); break;
            case TrackLaneKind::imageSequence:        modeButton.setButtonText("IMAGE"); break;
        }
    }

    std::function<void(TrackLaneKind)> onLaneKindChanged;

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds();
        const auto tint = juce::Colour(track.colour);
        auto& laf = host.look();

        // Sfondo barra header orizzontale
        const auto bg = selected ? laf.colours().raised.interpolatedWith(tint, 0.22f)
                                 : laf.colours().top.interpolatedWith(tint, 0.08f);
        g.fillAll(bg);

        // Barra accento colore a sinistra
        g.setColour(tint);
        g.fillRect(0, 0, selected ? 6 : 4, r.getHeight());

        // Badge ruolo sintetico
        auto badgeRect = juce::Rectangle<int>(12, 4, 26, r.getHeight() - 8);
        g.setColour(tint.withAlpha(0.20f));
        g.fillRoundedRectangle(badgeRect.toFloat(), 3.0f);
        g.setColour(tint);
        g.drawRoundedRectangle(badgeRect.toFloat().reduced(0.5f), 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
        const auto symbol = track.role == Role::songTrack ? "ST"
                          : track.role == Role::performerTrack ? "T"
                          : track.role == Role::performerMix ? "M" : "S";
        g.drawText(symbol, badgeRect, juce::Justification::centred);

        // Sottile linea di divisione orizzontale inferiore
        g.setColour(laf.colours().border.withAlpha(0.6f));
        g.drawHorizontalLine(r.getHeight() - 1, 0.0f, static_cast<float>(r.getWidth()));
    }

    void resized() override
    {
        modeButton.setBounds(getWidth() - 66, 3, 58, getHeight() - 6);
        nameLabel.setBounds(44, 2, std::max(60, getWidth() - 118), getHeight() - 4);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            const auto sel = e.mods.isShiftDown() ? Selection::range
                           : (e.mods.isCommandDown() || e.mods.isCtrlDown()) ? Selection::toggle
                           : Selection::replace;
            host.store().select(track.id, sel);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const int modelHeight = clips::clip_height_law::nextModelHeight(static_cast<float>(track.height), host.getVerticalZoom());
        host.store().resize(track.id, modelHeight);
    }

    void labelTextChanged(juce::Label* labelThatChanged) override
    {
        if (labelThatChanged == &nameLabel)
        {
            const auto newName = nameLabel.getText().trim().toStdString();
            if (!newName.empty() && newName != track.name)
            {
                host.store().rename(track.id, newName);
            }
        }
    }

    void editorShown(juce::Label*, juce::TextEditor& ed) override
    {
        ed.setSelectAllWhenFocused(true);
    }

    bool isEditing() const noexcept { return nameLabel.isBeingEdited(); }

private:
    LabComponent& host;
    Track track;
    bool selected = false;
    TrackLaneKind laneKind = TrackLaneKind::audio;
    juce::Label nameLabel;
    juce::TextButton modeButton;
};

} // namespace

class TimelineLaneRow final : public juce::Component, public juce::DragAndDropTarget
{
public:
    static constexpr int kHeaderBarHeight = HorizontalTrackHeaderBar::kHeaderHeight;

    TimelineLaneRow(LabComponent& h)
        : host(h), headerBar(h),
          audioCanvas(h), midiCanvas(h),
          lineCanvas(h, timeline_visual_style::CurveLineColourVariant::light, TimelineCurveUnderlayStyle::darkBelow),
          pointKeyframeCanvas(h, KeyframeLaneType::points),
          spanKeyframeCanvas(h, KeyframeLaneType::contiguousSpans),
          disjointKeyframeCanvas(h, KeyframeLaneType::disjointSpans),
          continuousLineCanvas(h, timeline_visual_style::CurveLineColourVariant::tempoTeal),
          imageCanvas(h)
    {
        audioCanvas.setWarpFeatureEnabled(host.isWarpFeatureEnabled());
        audioCanvas.setGainFeatureEnabled(host.isGainFeatureEnabled());
        // Iniezione Solver Matematici Engine -> Canvas (Equal-Power Audio & Curvature Mirroring)
        audioCanvas.setCrossfadeSolver ([] (const clips::AudioCrossfadeContext& ctx) -> clips::AudioCrossfadeResult
        {
            // Solver Audio: preserva guadagni nominali indipendenti e coniuga la curvatura a pari potenza
            return clips::defaultAudioCrossfadeSolver (ctx);
        });

        lineCanvas.setCrossfadeSolver ([] (const clips::LineCrossfadeContext& ctx) -> clips::LineCrossfadeResult
        {
            // Solver Line: transizione di parametro continua con curvatura speculare
            return clips::defaultLineCrossfadeSolver (ctx);
        });

        imageCanvas.setCrossfadeSolver ([] (const clips::ImageCrossfadeContext& ctx) -> clips::ImageCrossfadeResult
        {
            // Solver Image: miscelazione complementare di trasparenza (alpha blend)
            return clips::defaultImageCrossfadeSolver (ctx);
        });
        midiCanvas.setWarpFeatureEnabled(host.isWarpFeatureEnabled());
        midiCanvas.setOverlayFeatureEnabled(host.isGainFeatureEnabled());
        imageCanvas.setWarpFeatureEnabled(host.isWarpFeatureEnabled());
        imageCanvas.setGainFeatureEnabled(host.isGainFeatureEnabled());
        continuousLineCanvas.setRange (40.0f, 220.0f, 120.0f, 1);

        headerBar.onLaneKindChanged = [this](HorizontalTrackHeaderBar::TrackLaneKind kind)
        {
            userLaneKind = kind;
            updateCanvasVisibility();
        };

        addAndMakeVisible(headerBar);
        addChildComponent(audioCanvas);
        addChildComponent(midiCanvas);
        addChildComponent(lineCanvas);
        addChildComponent(pointKeyframeCanvas);
        addChildComponent(spanKeyframeCanvas);
        addChildComponent(disjointKeyframeCanvas);
        addChildComponent(continuousLineCanvas);
        addChildComponent(imageCanvas);
    }

    void setWarpFeatureEnabled(const bool enabled)
    {
        audioCanvas.setWarpFeatureEnabled(enabled);
        midiCanvas.setWarpFeatureEnabled(enabled);
        imageCanvas.setWarpFeatureEnabled(enabled);
    }

    void setGainFeatureEnabled(const bool enabled)
    {
        audioCanvas.setGainFeatureEnabled(enabled);
        midiCanvas.setOverlayFeatureEnabled(enabled);
        imageCanvas.setGainFeatureEnabled(enabled);
    }

    void setModel(const Track& t)
    {
        data = t;
        if (!userLaneKind.has_value())
        {
            const auto name = juce::String(t.name).toLowerCase();
            const auto id = juce::String(t.id).toLowerCase();
            if (id.contains("image") || id.contains("film") || name.contains("image") || name.contains("video") || name.contains("film"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::imageSequence;
            else if (id.contains("tempo") || id.contains("continuous") || name.contains("tempo") || name.contains("bpm"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::continuousLine;
            else if (id.contains("block") || name.contains("block"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::disjointSpanKeyframe;
            else if (id.contains("span") || id.contains("arranger") || name.contains("program") || name.contains("section") || name.contains("span"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::spanKeyframe;
            else if (id.contains("marker") || id.contains("cue") || id.contains("keyframe") || name.contains("marker") || name.contains("cue"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::pointKeyframe;
            else if (id.contains("line") || id.contains("filter") || id.contains("automation") || name.contains("line") || name.contains("filter") || name.contains("automation"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::lineClip;
            else if ((t.midiPins > 0 && t.audioChannels == 0)
                     || id.contains("synth") || id.contains("midi")
                     || name.contains("midi") || name.contains("synth") || name.contains("drum"))
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::midi;
            else
                userLaneKind = HorizontalTrackHeaderBar::TrackLaneKind::audio;
        }

        const bool selected = hasItem(host.store().state().selected, t.id);
        headerBar.setModel(t, selected, userLaneKind.value_or(HorizontalTrackHeaderBar::TrackLaneKind::audio));
        updateCanvasVisibility();
        resized();
    }

    void updateCanvasVisibility()
    {
        const auto kind = userLaneKind.value_or(HorizontalTrackHeaderBar::TrackLaneKind::audio);
        audioCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::audio);
        midiCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::midi);
        lineCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::lineClip);
        pointKeyframeCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::pointKeyframe);
        spanKeyframeCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::spanKeyframe);
        disjointKeyframeCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::disjointSpanKeyframe);
        continuousLineCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::continuousLine);
        imageCanvas.setVisible(kind == HorizontalTrackHeaderBar::TrackLaneKind::imageSequence);

        switch (kind)
        {
            case HorizontalTrackHeaderBar::TrackLaneKind::audio:
                audioCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::midi:
                midiCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::lineClip:
                lineCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::pointKeyframe:
                pointKeyframeCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::spanKeyframe:
                spanKeyframeCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::disjointSpanKeyframe:
                disjointKeyframeCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::continuousLine:
                continuousLineCanvas.setModel(data, data.height);
                break;
            case HorizontalTrackHeaderBar::TrackLaneKind::imageSequence:
                imageCanvas.setModel(data, data.height);
                break;
        }
    }

    bool isEditing() const noexcept { return headerBar.isEditing(); }

    void resized() override
    {
        auto r = getLocalBounds();
        headerBar.setBounds(r.removeFromTop(kHeaderBarHeight));
        audioCanvas.setBounds(r);
        midiCanvas.setBounds(r);
        lineCanvas.setBounds(r);
        pointKeyframeCanvas.setBounds(r);
        spanKeyframeCanvas.setBounds(r);
        disjointKeyframeCanvas.setBounds(r);
        continuousLineCanvas.setBounds(r);
        imageCanvas.setBounds(r);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            vp->mouseWheelMove(e, wheel);
        else
            Component::mouseWheelMove(e, wheel);
    }

    bool isInterestedInDragSource(const SourceDetails& d) override
    {
        return host.store().state().mode == Mode::free && !data.pinned && d.description.isString()
            && findTrack(host.store().state(), d.description.toString().toStdString()) != nullptr;
    }

    void itemDragMove(const SourceDetails& d) override
    {
        over = true;
        after = d.localPosition.y > getHeight() / 2;
        repaint();
        host.scrollWhileDragging(localPointToGlobal(d.localPosition));
    }

    void itemDragEnter(const SourceDetails& d) override { itemDragMove(d); }
    void itemDragExit(const SourceDetails&) override { over = false; repaint(); }

    void itemDropped(const SourceDetails& d) override
    {
        over = false;
        const auto target = data.id;
        const bool putAfter = d.localPosition.y > getHeight() / 2;
        host.move(d.description.toString().toStdString(), target, putAfter);
        repaint();
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (!over) return;
        g.setColour(host.look().colours().accent);
        g.fillRect(0, after ? getHeight() - 3 : 0, getWidth(), 3);
    }

private:
    LabComponent& host;
    HorizontalTrackHeaderBar headerBar;
    AudioClipCanvas audioCanvas;
    MidiClipCanvas midiCanvas;
    LineClipCanvas lineCanvas;
    KeyframeLaneCanvas pointKeyframeCanvas;
    KeyframeLaneCanvas spanKeyframeCanvas;
    KeyframeLaneCanvas disjointKeyframeCanvas;
    ContinuousLineCanvas continuousLineCanvas;
    ImageClipCanvas imageCanvas;
    Track data;
    std::optional<HorizontalTrackHeaderBar::TrackLaneKind> userLaneKind;
    bool over = false, after = false;
};

class TimelineVisualHarnessComponent::Content final : public juce::Component
{
public:
    static constexpr int topInset = 70;
    static constexpr int sectionHeaderHeight = 26;
    static constexpr int headerToCanvasGap = 4;
    static constexpr int sectionGap = 12;
    static constexpr int bottomPadding = 6;
    static constexpr int audioHeight = 180;
    static constexpr int midiHeight = clips::clip_midi_law::noteAndVelocityTrackHeight;
    static constexpr int lineHeight = 256;
    static constexpr int pointKeyframeHeight = 100;
    static constexpr int spanKeyframeHeight = 120;
    static constexpr int disjointKeyframeHeight = 120;
    static constexpr int continuousLineHeight = 220;
    static constexpr int imageHeight = 180;
    static constexpr int sectionCount = 10;
    static constexpr int requiredHeight = topInset + bottomPadding
                                        + audioHeight + midiHeight + lineHeight
                                        + pointKeyframeHeight + spanKeyframeHeight + disjointKeyframeHeight
                                        + continuousLineHeight * 3
                                        + imageHeight
                                        + sectionCount * (sectionHeaderHeight
                                                        + headerToCanvasGap
                                                        + sectionGap);

    Content (LabComponent& h)
        : host (h),
          audioCanvas (h, timeline_visual_style::CurveLineColourVariant::light,
                       TimelineCurveUnderlayStyle::darkBelow),
          midiCanvas (h),
          lineCanvas (h, timeline_visual_style::CurveLineColourVariant::light,
                      TimelineCurveUnderlayStyle::darkBelow),
          pointKeyframeCanvas (h, KeyframeLaneType::points),
          spanKeyframeCanvas (h, KeyframeLaneType::contiguousSpans),
          disjointKeyframeCanvas (h, KeyframeLaneType::disjointSpans),
          continuousLineCanvasLight (h, timeline_visual_style::CurveLineColourVariant::light),
          continuousLineCanvasTeal (h, timeline_visual_style::CurveLineColourVariant::tempoTeal),
          continuousLineCanvasAmber (h, timeline_visual_style::CurveLineColourVariant::accentAmber),
          imageCanvas (h)
    {
        for (auto* canvas : std::initializer_list<juce::Component*>
             { &audioCanvas, &midiCanvas, &lineCanvas,
               &pointKeyframeCanvas, &spanKeyframeCanvas, &disjointKeyframeCanvas,
               &continuousLineCanvasLight, &continuousLineCanvasTeal,
               &continuousLineCanvasAmber, &imageCanvas })
        {
            addAndMakeVisible (*canvas);
        }

        addAndMakeVisible (underlayButton);
        underlayButton.setClickingTogglesState (true);
        underlayButton.setTooltip (
            "Riempimento curve nelle clip\nSceglie se scurire la zona sotto o sopra la curva; la Continuous Line mantiene sempre il gradiente Tempo");
        underlayButton.onClick = [this]
        {
            clipUnderlayStyle = underlayButton.getToggleState()
                                ? TimelineCurveUnderlayStyle::darkAbove
                                : TimelineCurveUnderlayStyle::darkBelow;
            applyClipUnderlayStyle();
        };
        applyClipUnderlayStyle();

        for (auto* canvas : { &continuousLineCanvasLight,
                              &continuousLineCanvasTeal,
                              &continuousLineCanvasAmber })
            canvas->setRange (40.0f, 220.0f, 120.0f, 1);
    }

    void refreshFromModel()
    {
        audioCanvas.setWarpFeatureEnabled (host.isWarpFeatureEnabled());
        audioCanvas.setGainFeatureEnabled (host.isGainFeatureEnabled());
        // Iniezione Solver Matematici Engine -> Canvas (Equal-Power Audio & Curvature Mirroring)
        audioCanvas.setCrossfadeSolver ([] (const clips::AudioCrossfadeContext& ctx) -> clips::AudioCrossfadeResult
        {
            // Solver Audio: preserva guadagni nominali indipendenti e coniuga la curvatura a pari potenza
            return clips::defaultAudioCrossfadeSolver (ctx);
        });

        lineCanvas.setCrossfadeSolver ([] (const clips::LineCrossfadeContext& ctx) -> clips::LineCrossfadeResult
        {
            // Solver Line: transizione di parametro continua con curvatura speculare
            return clips::defaultLineCrossfadeSolver (ctx);
        });

        imageCanvas.setCrossfadeSolver ([] (const clips::ImageCrossfadeContext& ctx) -> clips::ImageCrossfadeResult
        {
            // Solver Image: miscelazione complementare di trasparenza (alpha blend)
            return clips::defaultImageCrossfadeSolver (ctx);
        });
        midiCanvas.setWarpFeatureEnabled (host.isWarpFeatureEnabled());
        midiCanvas.setOverlayFeatureEnabled (host.isGainFeatureEnabled());
        imageCanvas.setWarpFeatureEnabled (host.isWarpFeatureEnabled());
        imageCanvas.setGainFeatureEnabled (host.isGainFeatureEnabled());

        const Track* fixtureTrack = nullptr;
        for (const auto& track : host.store().state().tracks)
        {
            if (!track.clips.empty())
            {
                fixtureTrack = &track;
                break;
            }
        }

        if (fixtureTrack == nullptr && !host.store().state().tracks.empty())
            fixtureTrack = &host.store().state().tracks.front();

        const bool hasFixture = fixtureTrack != nullptr;
        for (auto* canvas : std::initializer_list<juce::Component*>
             { &audioCanvas, &midiCanvas, &lineCanvas,
               &pointKeyframeCanvas, &spanKeyframeCanvas, &disjointKeyframeCanvas,
               &continuousLineCanvasLight, &continuousLineCanvasTeal,
               &continuousLineCanvasAmber, &imageCanvas })
        {
            canvas->setVisible (hasFixture);
        }

        if (!hasFixture)
        {
            repaint();
            return;
        }

        audioCanvas.setModel (*fixtureTrack, audioHeight);
        midiCanvas.setModel (*fixtureTrack, midiHeight);
        lineCanvas.setModel (*fixtureTrack, lineHeight);
        pointKeyframeCanvas.setModel (*fixtureTrack, pointKeyframeHeight);
        spanKeyframeCanvas.setModel (*fixtureTrack, spanKeyframeHeight);
        disjointKeyframeCanvas.setModel (*fixtureTrack, disjointKeyframeHeight);
        continuousLineCanvasLight.setModel (*fixtureTrack, continuousLineHeight);
        continuousLineCanvasTeal.setModel (*fixtureTrack, continuousLineHeight);
        continuousLineCanvasAmber.setModel (*fixtureTrack, continuousLineHeight);
        imageCanvas.setModel (*fixtureTrack, imageHeight);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto& laf = host.look();
        g.fillAll (laf.colours().base);

        g.setColour (laf.colours().text);
        g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f).withStyle ("Bold")));
        g.drawText ("CLIP IMPLEMENTATION HARNESS", 20, 12, getWidth() - 40, 26,
                    juce::Justification::centredLeft);

        g.setColour (laf.colours().secondary);
        g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        g.drawText ("Default: scuro sotto. Il controllo permette il confronto con scuro sopra.",
                    20, 38, getWidth() - 300, 20, juce::Justification::centredLeft);

        static constexpr std::array<const char*, sectionCount> titles
        {
            "1  AUDIO CLIP - riempimento selezionabile dall'interfaccia",
            "2  MIDI NOTE CLIP - piano roll, velocity lane e scrollbar pitch",
            "3  LINE CLIP - riempimento selezionabile dall'interfaccia",
            "4  POINT KEYFRAME - layout Counter / Marker di ayra_multiverse",
            "5  CONTIGUOUS SPAN KEYFRAME - range continui ancorati (Sections)",
            "6  DISJOINT SPAN KEYFRAME - blocchi separabili con rail di rimozione e gap",
            "7  CONTINUOUS LINE - gradiente Tempo + linea TimelineLab bianca",
            "8  CONTINUOUS LINE - gradiente Tempo + linea TimelineLab teal",
            "9  CONTINUOUS LINE - gradiente Tempo + linea TimelineLab ambra",
            "10 IMAGE SEQUENCE CLIP - filmstrip full-bleed, vignettatura e curva opacita'"
        };

        for (std::size_t i = 0; i < sectionHeaders.size(); ++i)
        {
            const auto bounds = sectionHeaders[i];
            g.setColour (laf.colours().raised);
            g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
            g.setColour (laf.colours().border);
            g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (laf.colours().text);
            g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
            g.drawText (titles[i], bounds.reduced (10, 0), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        constexpr int margin = 20;
        int y = topInset;
        const int width = juce::jmax (320, getWidth() - margin * 2);
        underlayButton.setBounds (juce::jmax (margin, getWidth() - 260), 32, 240, 28);

        const auto placeSection = [&] (const std::size_t index,
                                       juce::Component& canvas,
                                       const int canvasHeight)
        {
            sectionHeaders[index] = { margin, y, width, sectionHeaderHeight };
            y += sectionHeaderHeight + headerToCanvasGap;
            canvas.setBounds (margin, y, width, canvasHeight);
            y += canvasHeight + sectionGap;
        };

        placeSection (0, audioCanvas, audioHeight);
        placeSection (1, midiCanvas, midiHeight);
        placeSection (2, lineCanvas, lineHeight);
        placeSection (3, pointKeyframeCanvas, pointKeyframeHeight);
        placeSection (4, spanKeyframeCanvas, spanKeyframeHeight);
        placeSection (5, disjointKeyframeCanvas, disjointKeyframeHeight);
        placeSection (6, continuousLineCanvasLight, continuousLineHeight);
        placeSection (7, continuousLineCanvasTeal, continuousLineHeight);
        placeSection (8, continuousLineCanvasAmber, continuousLineHeight);
        placeSection (9, imageCanvas, imageHeight);
    }

private:
    void applyClipUnderlayStyle()
    {
        audioCanvas.setCurveUnderlayStyle (clipUnderlayStyle);
        lineCanvas.setCurveUnderlayStyle (clipUnderlayStyle);
        underlayButton.setButtonText (
            clipUnderlayStyle == TimelineCurveUnderlayStyle::darkBelow
                ? "Clip: scuro SOTTO"
                : "Clip: scuro SOPRA");
        repaint();
    }

    LabComponent& host;
    AudioClipCanvas audioCanvas;
    MidiClipCanvas midiCanvas;
    LineClipCanvas lineCanvas;
    KeyframeLaneCanvas pointKeyframeCanvas;
    KeyframeLaneCanvas spanKeyframeCanvas;
    KeyframeLaneCanvas disjointKeyframeCanvas;
    ContinuousLineCanvas continuousLineCanvasLight;
    ContinuousLineCanvas continuousLineCanvasTeal;
    ContinuousLineCanvas continuousLineCanvasAmber;
    ImageClipCanvas imageCanvas;
    juce::TextButton underlayButton;
    TimelineCurveUnderlayStyle clipUnderlayStyle { TimelineCurveUnderlayStyle::darkBelow };
    std::array<juce::Rectangle<int>, sectionCount> sectionHeaders;
};

TimelineVisualHarnessComponent::TimelineVisualHarnessComponent (LabComponent& h)
    : content (std::make_unique<Content> (h))
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    refreshFromModel();
}

TimelineVisualHarnessComponent::~TimelineVisualHarnessComponent()
{
    viewport.setViewedComponent (nullptr, false);
}

void TimelineVisualHarnessComponent::refreshFromModel()
{
    content->refreshFromModel();
}

void TimelineVisualHarnessComponent::resized()
{
    viewport.setBounds (getLocalBounds());
    const int contentWidth = juce::jmax (320, viewport.getWidth() - viewport.getScrollBarThickness());
    content->setSize (contentWidth, Content::requiredHeight);
}

LabComponent::LabComponent()
{
    setWantsKeyboardFocus(true);
    setLookAndFeel(&laf);

    for (auto* button : {&grouped, &free, &theme, &undoButton, &redoButton, &saveButton, &loadButton, &resetButton, &collapse, &warpFeatureButton, &gainFeatureButton})
        addAndMakeVisible(*button);

    for (auto* comp : std::initializer_list<juce::Component*>{ &zoomVLabel, &zoomOutVButton, &zoomVSlider, &zoomInVButton, &zoomResetButton })
        addAndMakeVisible(*comp);

    grouped.setTooltip("Modalita' Raggruppata\nVisualizza e gestisce le tracce per folder e gruppi gerarchici");
    free.setTooltip("Modalita' Libera\nVisualizzazione libera delle tracce senza raggruppamento per cartelle");
    theme.setTooltip("Cambia Tema (Dark / Light)\nAlterna tra il tema scuro standard e il tema chiaro ad alto contrasto");
    undoButton.setTooltip("Annulla (Cmd+Z)\nRipristina l'ultimo stato della timeline");
    redoButton.setTooltip("Ripeti (Cmd+Shift+Z)\nRipete l'azione precedentemente annullata");
    saveButton.setTooltip("Salva Sessione JSON\nEsporta lo stato delle tracce, clip e parametri su file");
    loadButton.setTooltip("Carica Sessione JSON\nImporta uno stato salvato su file");
    resetButton.setTooltip("Reset Fixture\nRipristina la timeline e le tracce allo stato iniziale di test");
    collapse.setTooltip("Comprimi / Espandi Gruppi\nAlterna la visualizzazione compatta delle sezioni traccia");

    warpFeatureButton.setClickingTogglesState(true);
    warpFeatureButton.setToggleState(true, juce::dontSendNotification);
    warpFeatureButton.setTooltip("Toggle Feature Warp\nAbilita o disabilita il rail superiore dei Warp Markers.\nDisabilitando Warp, lo spazio superiore (10 pt) viene recuperato istantaneamente per la forma d'onda (Space Reclamation).");
    warpFeatureButton.onClick = [this]
    {
        setWarpFeatureEnabled(warpFeatureButton.getToggleState());
    };

    gainFeatureButton.setClickingTogglesState(true);
    gainFeatureButton.setToggleState(true, juce::dontSendNotification);
    gainFeatureButton.setTooltip("Toggle Feature Gain\nAbilita o disabilita l'inviluppo di Gain, i punti e le curve.\nA bassi livelli di zoom verticale (< 40px), l'inviluppo viene nascosto automaticamente (LOD).");
    gainFeatureButton.onClick = [this]
    {
        setGainFeatureEnabled(gainFeatureButton.getToggleState());
    };

    zoomVLabel.setText("Zoom V:", juce::dontSendNotification);
    zoomVLabel.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    zoomVLabel.setJustificationType(juce::Justification::centredRight);
    zoomVLabel.setTooltip("Zoom Verticale Tracce\nModifica l'altezza delle corsie per testare le proporzioni delle clip");

    zoomOutVButton.setTooltip("Riduci Zoom Verticale (Alt+Down o Alt+Rotellina)\nDiminuisce l'altezza delle tracce");
    zoomInVButton.setTooltip("Aumenta Zoom Verticale (Alt+Up o Alt+Rotellina)\nAumenta l'altezza delle tracce");
    zoomResetButton.setTooltip("Reset Zoom Verticale (100%)\nRipristina le altezze originali al 100%");
    zoomVSlider.setTooltip("Cursore Zoom Verticale (35% - 300%)\nRegola fluidamente l'altezza verticale di tutte le tracce");

    zoomVSlider.setRange(0.35, 3.0, 0.01);
    zoomVSlider.setValue(1.0, juce::dontSendNotification);
    zoomVSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomVSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomVSlider.onValueChange = [this]
    {
        setVerticalZoom(static_cast<float>(zoomVSlider.getValue()));
    };
    zoomOutVButton.onClick = [this] { adjustVerticalZoom(-0.15f); };
    zoomInVButton.onClick  = [this] { adjustVerticalZoom(0.15f); };
    zoomResetButton.onClick = [this] { setVerticalZoom(1.0f); };

    grouped.onClick = [this]{ model.setMode(Mode::grouped); };
    free.onClick    = [this]{ model.setMode(Mode::free); };
    theme.onClick   = [this]{ auto s = model.state(); s.dark = !s.dark; model.commit(std::move(s), "Tema"); };
    undoButton.onClick  = [this]{ model.undo(); };
    redoButton.onClick  = [this]{ model.redo(); };
    saveButton.onClick  = [this]{ chooseFile(true); };
    loadButton.onClick  = [this]{ chooseFile(false); };
    resetButton.onClick = [this]{ cancelInput(); model.replace(makeFixture()); };
    collapse.onClick    = [this]{ auto s = model.state(); s.groupCollapsed = !s.groupCollapsed; model.commit(std::move(s), "Gruppo di vista"); };

    timelineViewport.setViewedComponent(&timelineContent, false);
    timelineViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(timelineViewport);

    status.setFont(juce::Font(juce::FontOptions().withHeight(11)));
    addAndMakeVisible(status);

    model.changed = [this]{ triggerAsyncUpdate(); };
    setSize(1480, 960);
    refresh();
}

LabComponent::~LabComponent()
{
    model.changed = {};
    cancelPendingUpdate();
    chooser.reset();
    harnessView.reset();
    timelineViewport.setViewedComponent(nullptr, false);
    rows.clear();
    setLookAndFeel(nullptr);
}

void LabComponent::paint(juce::Graphics& g)
{
    g.fillAll(laf.colours().base);
    g.setColour(laf.colours().text);
    g.setFont(juce::Font(juce::FontOptions().withHeight(21).withStyle("Bold")));
    g.drawText("AYRA / TIMELINE LAB", 20, 12, 440, 30, juce::Justification::centredLeft);
    g.setColour(laf.colours().secondary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10)));
    g.drawText(juce::String(laf.sourceLabel()) + "  |  Header orizzontale a nastro  |  Timeline-First", 20, 43, getWidth() - 40, 20, juce::Justification::centredLeft);
    g.setColour(laf.colours().border);
    g.drawHorizontalLine(105, 0, static_cast<float>(getWidth()));
}

void LabComponent::resized()
{
    auto toolbar = juce::Rectangle<int>(16, 72, getWidth() - 32, 26);
    auto zoomArea = toolbar.removeFromRight(270);
    zoomVLabel.setBounds(zoomArea.removeFromLeft(50));
    zoomArea.removeFromLeft(4);
    zoomOutVButton.setBounds(zoomArea.removeFromLeft(22));
    zoomArea.removeFromLeft(4);
    zoomVSlider.setBounds(zoomArea.removeFromLeft(110));
    zoomArea.removeFromLeft(4);
    zoomInVButton.setBounds(zoomArea.removeFromLeft(22));
    zoomArea.removeFromLeft(6);
    zoomResetButton.setBounds(zoomArea.removeFromLeft(48));

    toolbar.removeFromRight(10);
    auto featureArea = toolbar.removeFromRight(118);
    warpFeatureButton.setBounds(featureArea.removeFromLeft(56));
    featureArea.removeFromLeft(6);
    gainFeatureButton.setBounds(featureArea.removeFromLeft(56));

    for (auto* b : {&grouped, &free, &theme, &undoButton, &redoButton})
    {
        b->setBounds(toolbar.removeFromLeft(b == &grouped ? 98 : b == &free ? 66 : 58));
        toolbar.removeFromLeft(6);
    }
    collapse.setBounds(toolbar.removeFromLeft(113));
    toolbar.removeFromLeft(8);
    for (auto* b : {&saveButton, &loadButton, &resetButton})
    {
        b->setBounds(toolbar.removeFromLeft(b == &resetButton ? 55 : 100));
        toolbar.removeFromLeft(6);
    }

    if (harnessView != nullptr)
    {
        harnessView->setBounds(0, 106, getWidth(), getHeight() - 106 - 32);
    }
    timelineViewport.setBounds(0, 106, getWidth(), getHeight() - 106 - 32);
    status.setBounds(12, getHeight() - 27, getWidth() - 24, 24);
    layoutRows();
}

void LabComponent::layoutRows()
{
    const auto ids = visibleOrder(model.state());
    const int width = std::max(getWidth(), timelineViewport.getWidth() - timelineViewport.getScrollBarThickness());
    int y = 148;
    for (const auto& id : ids)
    {
        auto row = rows.find(id);
        if (row == rows.end()) continue;
        auto t = *findTrack(model.state(), id);
        if (auto p = resizePreviews.find(id); p != resizePreviews.end()) t.height = p->second;
        t.height = std::clamp(juce::roundToInt(static_cast<float>(t.height) * verticalZoomFactor),
                              minimumHeight(t), 1200);
        row->second->setModel(t);
        const int rowHeight = TimelineLaneRow::kHeaderBarHeight + visualHeight(t);
        row->second->setBounds(0, y, width, rowHeight);
        y += rowHeight;
    }
    timelineContent.setSize(width, std::max(y, timelineViewport.getHeight()));
}

void LabComponent::mouseWheelMove(const juce::MouseEvent& e,
                                  const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isAltDown() || e.mods.isCommandDown()
        || e.mods.isCtrlDown() || e.mods.isShiftDown())
    {
        const float delta = std::abs(wheel.deltaY) > 0.0001f
                          ? wheel.deltaY : wheel.deltaX;
        if (std::abs(delta) > 0.001f)
        {
            adjustVerticalZoom(delta > 0.0f ? 0.08f : -0.08f);
            return;
        }
    }

    juce::Component::mouseWheelMove(e, wheel);
}

void LabComponent::adjustVerticalZoom(const float delta)
{
    setVerticalZoom(verticalZoomFactor + delta);
}

void LabComponent::setVerticalZoom(const float factor)
{
    const float next = juce::jlimit(0.35f, 3.0f, factor);
    if (std::abs(next - verticalZoomFactor) < 0.0001f) { return; }

    verticalZoomFactor = next;
    zoomVSlider.setValue(verticalZoomFactor, juce::dontSendNotification);
    zoomResetButton.setButtonText(juce::String(juce::roundToInt(verticalZoomFactor * 100.0f)) + "%");
    layoutRows();
    repaint();
}

void LabComponent::setWarpFeatureEnabled(const bool enabled)
{
    warpFeatureButton.setToggleState(enabled, juce::dontSendNotification);
    for (auto& [_, row] : rows)
        if (row != nullptr) row->setWarpFeatureEnabled(enabled);
    if (harnessView != nullptr) harnessView->refreshFromModel();
}

bool LabComponent::isWarpFeatureEnabled() const noexcept
{
    return warpFeatureButton.getToggleState();
}

void LabComponent::setGainFeatureEnabled(const bool enabled)
{
    gainFeatureButton.setToggleState(enabled, juce::dontSendNotification);
    for (auto& [_, row] : rows)
        if (row != nullptr) row->setGainFeatureEnabled(enabled);
    if (harnessView != nullptr) harnessView->refreshFromModel();
}

bool LabComponent::isGainFeatureEnabled() const noexcept
{
    return gainFeatureButton.getToggleState();
}

void LabComponent::refresh()
{
    laf.setDark(model.state().dark);
    sendLookAndFeelChange();

    zoomVSlider.setValue(verticalZoomFactor, juce::dontSendNotification);
    zoomResetButton.setButtonText(juce::String(juce::roundToInt(verticalZoomFactor * 100.0f)) + "%");
    warpFeatureButton.setToggleState(isWarpFeatureEnabled(), juce::dontSendNotification);
    gainFeatureButton.setToggleState(isGainFeatureEnabled(), juce::dontSendNotification);

    if (harnessView != nullptr)
        harnessView->refreshFromModel();

    grouped.setToggleState(model.state().mode == Mode::grouped, juce::dontSendNotification);
    free.setToggleState(model.state().mode == Mode::free, juce::dontSendNotification);
    undoButton.setEnabled(model.canUndo());
    redoButton.setEnabled(model.canRedo());
    collapse.setEnabled(model.state().mode == Mode::grouped);

    const auto ids = visibleOrder(model.state());
    std::set<std::string> live;
    for (const auto& t : model.state().tracks) live.insert(t.id);

    for (auto i = rows.begin(); i != rows.end();)
    {
        if (!live.contains(i->first)) i = rows.erase(i);
        else ++i;
    }
    for (const auto& t : model.state().tracks)
    {
        auto& ptr = rows[t.id];
        if (!ptr)
        {
            ptr = std::make_unique<TimelineLaneRow>(*this);
            ptr->setWarpFeatureEnabled(isWarpFeatureEnabled());
            ptr->setGainFeatureEnabled(isGainFeatureEnabled());
            timelineContent.addAndMakeVisible(*ptr);
        }
        ptr->setVisible(hasItem(ids, t.id));
    }

    // Global lanes
    static constexpr const char* globalNames = "Range                         Confini Song\nTempo                         120 BPM\nTime Sig                       4/4\nMarkers                       Intro / Verse / Chorus\nCounter                       Count-in\nActions                       Fixture";
    if (timelineContent.getNumChildComponents() > 0)
    {
        juce::Label* global = nullptr;
        for (int i = 0; i < timelineContent.getNumChildComponents(); ++i)
            if (auto* l = dynamic_cast<juce::Label*>(timelineContent.getChildComponent(i))) global = l;
        if (!global)
        {
            global = &globalLanes;
            timelineContent.addAndMakeVisible(*global);
        }
        global->setText(globalNames, juce::dontSendNotification);
        global->setFont(juce::Font(juce::FontOptions().withHeight(11)));
        global->setJustificationType(juce::Justification::centredLeft);
        global->setBounds(14, 0, 640, 145);
    }

    status.setText(str(model.lastError().empty() ? model.lastAction() : model.lastError()) + "  /  NO AUDIO - NO DSP", juce::dontSendNotification);
    layoutRows();
    repaint();
}

void LabComponent::handleAsyncUpdate() { refresh(); }
void LabComponent::cancelInput() { resizePreviews.clear(); }
void LabComponent::selectClip(const std::string& id) { model.selectClip(id); }
bool LabComponent::deleteClip(const std::string& trackId,
                              const std::string& clipId)
{
    return model.removeClip(trackId, clipId);
}
void LabComponent::requestTimelineContextMenu(
    const clips::TimelinePointerContext& context) const
{
    if (onTimelineContextMenuRequested)
        onTimelineContextMenuRequested(context);
}
void LabComponent::openClip(const std::string&, const std::string& clip)
{
    model.selectClip(clip);
    triggerAsyncUpdate();
}

void LabComponent::updateClipFade(const std::string& trackId, const std::string& clipId, double fadeIn, double fadeOut)
{
    auto s = model.state();
    auto* t = findTrack(s, trackId);
    if (!t) return;
    for (auto& c : t->clips)
    {
        if (c.id == clipId)
        {
            c.fadeIn = std::clamp(fadeIn, 0.0, c.length - c.fadeOut);
            c.fadeOut = std::clamp(fadeOut, 0.0, c.length - c.fadeIn);
            model.commit(std::move(s), "Modifica Fade Clip");
            return;
        }
    }
}

std::string LabComponent::splitClip(const std::string& trackId,
                                    const std::string& clipId,
                                    const double splitTime)
{
    auto s = model.state();
    auto* t = findTrack(s, trackId);
    if (!t) { return {}; }
    for (auto it = t->clips.begin(); it != t->clips.end(); ++it)
    {
        if (it->id == clipId)
        {
            const double rel = splitTime - it->start;
            if (rel > 0.2 && rel < it->length - 0.2)
            {
                Clip c2 = *it;
                it->length = rel;
                it->fadeOut = 0.0;

                std::string newId = it->id + "_cut";
                int counter = 1;
                auto idExists = [&](const std::string& cid) {
                    for (const auto& c : t->clips) if (c.id == cid) return true;
                    return false;
                };
                while (idExists(newId))
                    newId = it->id + "_cut_" + std::to_string(++counter);

                c2.id = newId;
                c2.start = splitTime;
                c2.length = c2.length - rel;
                c2.fadeIn = 0.0;
                c2.fadeOut = 0.0;
                t->clips.insert(it + 1, c2);
                model.commit(std::move(s), "Taglio Clip");
                return newId;
            }
            return {};
        }
    }
    return {};
}

void LabComponent::moveClip(const std::string& trackId, const std::string& clipId, double newStart)
{
    auto s = model.state();
    auto* t = findTrack(s, trackId);
    if (!t) return;
    for (auto& c : t->clips)
    {
        if (c.id == clipId)
        {
            if (std::abs(c.start - newStart) > 0.0001)
            {
                c.start = std::max(0.0, newStart);
                model.commit(std::move(s), "Sposta Clip");
            }
            return;
        }
    }
}

void LabComponent::resizeClip(const std::string& trackId, const std::string& clipId, double newStart, double newLength)
{
    auto s = model.state();
    auto* t = findTrack(s, trackId);
    if (!t) return;
    for (auto& c : t->clips)
    {
        if (c.id == clipId)
        {
            c.start = std::max(0.0, newStart);
            c.length = std::max(0.125, newLength);
            if (c.fadeIn + c.fadeOut > c.length)
            {
                const double sum = c.fadeIn + c.fadeOut;
                if (sum > 0.0001)
                {
                    const double scale = (c.length * 0.95) / sum;
                    c.fadeIn *= scale;
                    c.fadeOut *= scale;
                }
            }
            model.commit(std::move(s), "Ridimensiona Clip");
            return;
        }
    }
}

std::vector<std::string> LabComponent::duplicateClipLoop(const std::string& trackId,
                                                         const std::string& clipId,
                                                         const int count,
                                                         const bool appendAfter)
{
    if (count <= 0) return {};
    auto s = model.state();
    auto* t = findTrack(s, trackId);
    if (!t) return {};

    auto it = std::find_if(t->clips.begin(), t->clips.end(),
                           [&](const Clip& c) { return c.id == clipId; });
    if (it == t->clips.end()) return {};

    const Clip orig = *it;
    const double L = orig.length;
    if (L < 0.125) return {};

    auto idExists = [&](const std::string& cid) {
        for (const auto& track : s.tracks)
            for (const auto& c : track.clips)
                if (c.id == cid) return true;
        return false;
    };

    const int maxAllowed = std::max(0, 64 - static_cast<int>(t->clips.size()));
    const int numToCreate = std::min(count, maxAllowed);
    if (numToCreate <= 0) return {};

    std::vector<std::string> createdIds;
    createdIds.reserve(static_cast<size_t>(numToCreate));

    for (int k = 1; k <= numToCreate; ++k)
    {
        const double newStart = appendAfter ? (orig.start + static_cast<double>(k) * L)
                                            : (orig.start - static_cast<double>(k) * L);
        if (newStart < 0.0)
            break;

        Clip dup = orig;
        int counter = k;
        std::string newId = orig.id + "_loop_" + std::to_string(counter);
        while (idExists(newId))
            newId = orig.id + "_loop_" + std::to_string(++counter);

        dup.id = newId;
        dup.start = newStart;
        dup.length = L;
        createdIds.push_back(newId);
        t->clips.push_back(dup);
    }

    if (createdIds.empty()) return {};

    std::sort(t->clips.begin(), t->clips.end(),
              [](const Clip& a, const Clip& b) { return a.start < b.start; });

    model.commit(std::move(s), "Loop Clip");
    return createdIds;
}

void LabComponent::handle(HeaderIntent i)
{
    using A = HeaderAction;
    switch (i.action)
    {
        case A::select: model.select(i.id, i.selection); return;
        case A::mute: model.toggle(i.id, Flag::mute); return;
        case A::solo: model.toggle(i.id, Flag::solo); return;
        case A::enabled: model.toggle(i.id, Flag::enabled); return;
        case A::bypass: model.toggle(i.id, Flag::bypass); return;
        case A::buffer: model.toggle(i.id, Flag::buffer); return;
        case A::oversample: model.toggle(i.id, Flag::oversample); return;
        case A::expanded: model.toggle(i.id, Flag::expanded); return;
        case A::pinned: model.toggle(i.id, Flag::pinned); return;
        case A::gain: model.setGain(i.id, i.value); return;
        case A::pan: model.setPan(i.id, i.value); return;
        case A::monitor:
        {
            auto s = model.state();
            if (auto* t = findTrack(s, i.id))
            {
                t->monitor = i.text;
                model.commit(std::move(s), "Monitoring fixture");
            }
            return;
        }
        case A::resizePreview: resizePreviews[i.id] = static_cast<int>(i.value); layoutRows(); return;
        case A::resizeCommit: resizePreviews.erase(i.id); model.resize(i.id, static_cast<int>(i.value)); layoutRows(); return;
        case A::resizeCancel: resizePreviews.erase(i.id); layoutRows(); return;
        case A::stepUp: case A::stepDown:
        {
            const auto ids = visibleOrder(model.state());
            auto it = std::find(ids.begin(), ids.end(), i.id);
            if (it == ids.end()) return;
            const auto index = std::distance(ids.begin(), it);
            const auto target = index + (i.action == A::stepUp ? -1 : 1);
            if (target >= 0 && target < static_cast<std::ptrdiff_t>(ids.size()))
                model.move({i.id}, ids[static_cast<std::size_t>(target)], target > index);
            return;
        }
        default: break;
    }
    triggerAsyncUpdate();
}

void LabComponent::move(const std::string& dragged, const std::string& anchor, bool after)
{
    std::vector<std::string> moving{dragged};
    if (hasItem(model.state().selected, dragged))
    {
        moving.clear();
        for (const auto& id : visibleOrder(model.state()))
            if (hasItem(model.state().selected, id) && !findTrack(model.state(), id)->pinned)
                moving.push_back(id);
    }
    model.move(moving, anchor, after);
}

void LabComponent::scrollWhileDragging(juce::Point<int> screen)
{
    const auto local = timelineViewport.getLocalPoint(nullptr, screen);
    timelineViewport.autoScroll(local.x, local.y, 28, 12);
}

bool LabComponent::keyPressed(const juce::KeyPress& key)
{
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*>(focused) || focused->findParentComponentOfClass<juce::TextEditor>())
            return false;
    const bool command = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    if (command && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        for (const auto& [id, row] : rows)
            if (row->isEditing()) return true;
        key.getModifiers().isShiftDown() ? model.redo() : model.undo();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::upKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
    {
        adjustVerticalZoom(0.12f);
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::downKey) && (key.getModifiers().isShiftDown() || key.getModifiers().isAltDown() || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
    {
        adjustVerticalZoom(-0.12f);
        return true;
    }
    return false;
}

void LabComponent::chooseFile(bool save)
{
    chooser = std::make_unique<juce::FileChooser>(save ? "Esporta sessione Lab" : "Importa sessione Lab",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ayra-header-lab.json"), "*.json");
    const juce::Component::SafePointer<LabComponent> safe(this);
    const auto flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting)
                            : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
    chooser->launchAsync(flags, [safe, save](const juce::FileChooser& c)
    {
        if (!safe) return;
        const auto file = c.getResult();
        if (file == juce::File{}) return;
        if (save)
        {
            if (!file.replaceWithText(encodeSession(safe->model.state())))
                safe->status.setText("Esportazione fallita", juce::dontSendNotification);
            else
                safe->status.setText("Sessione Lab esportata", juce::dontSendNotification);
        }
        else safe->loadFile(file);
    });
}

void LabComponent::loadFile(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 2 * 1024 * 1024)
    {
        status.setText("File non valido o oltre 2 MiB", juce::dontSendNotification);
        return;
    }
    Session candidate;
    const auto result = decodeSession(file.loadFileAsString(), candidate);
    if (result.failed())
    {
        status.setText(result.getErrorMessage(), juce::dontSendNotification);
        return;
    }
    cancelInput();
    model.replace(std::move(candidate));
}
}
