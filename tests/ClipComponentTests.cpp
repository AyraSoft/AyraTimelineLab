/** @file ClipComponentTests.cpp
    Regressioni strutturali del layer clip: geometria float, input ownership,
    provider e dispatch LookAndFeel. Non apre finestre e non avvia il Lab. */
#include "clips/ayra_clips.h"

#include <iostream>
#include <stdexcept>

namespace
{
using namespace ayra::lab::clips;

class TestProvider final : public AudioClipPresentationProvider
{
public:
    AudioClipVisual makeAudioClipVisual (
        const std::string& id,
        const juce::Rectangle<float> body,
        const juce::Point<float> origin) override
    {
        ++calls;
        clipId = id;
        localBody = body;
        parentOrigin = origin;

        AudioClipVisual visual;
        visual.bounds = body;
        visual.title = "Test";
        return visual;
    }

    int calls = 0;
    std::string clipId;
    juce::Rectangle<float> localBody;
    juce::Point<float> parentOrigin;
};

class TestMidiProvider final : public MidiClipPresentationProvider
{
public:
    MidiClipVisual makeMidiClipVisual (
        const std::string& id,
        const juce::Rectangle<float> body,
        const juce::Point<float> origin) override
    {
        ++calls;
        clipId = id;
        localBody = body;
        parentOrigin = origin;

        MidiClipVisual visual;
        visual.bounds = body;
        visual.title = "MidiTest";
        return visual;
    }

    int calls = 0;
    std::string clipId;
    juce::Rectangle<float> localBody;
    juce::Point<float> parentOrigin;
};

class TestLineProvider final : public LineClipPresentationProvider
{
public:
    LineClipVisual makeLineClipVisual (const std::string& id,
                                       const juce::Rectangle<float> body,
                                       const juce::Point<float>) override
    {
        ++calls;
        clipId = id;
        LineClipVisual visual;
        visual.bounds = body;
        return visual;
    }

    int calls = 0;
    std::string clipId;
};

class TestKeyframeProvider final : public KeyframeLanePresentationProvider
{
public:
    KeyframeLaneVisual makeKeyframeLaneVisual (const std::string& id,
                                               const juce::Rectangle<float> body,
                                               const juce::Point<float>) override
    {
        ++calls;
        laneId = id;
        KeyframeLaneVisual visual;
        visual.bounds = body;
        return visual;
    }

    int calls = 0;
    std::string laneId;
};

class TestContinuousProvider final : public ContinuousLinePresentationProvider
{
public:
    ContinuousLineVisual makeContinuousLineVisual (const std::string& id,
                                                   const juce::Rectangle<float> body,
                                                   const juce::Point<float>) override
    {
        ++calls;
        laneId = id;
        ContinuousLineVisual visual;
        visual.bounds = body;
        return visual;
    }

    int calls = 0;
    std::string laneId;
};

class TestImageProvider final : public ImageClipPresentationProvider
{
public:
    ImageClipVisual makeImageClipVisual (
        const std::string& id,
        const juce::Rectangle<float> body,
        const juce::Point<float> origin) override
    {
        ++calls;
        clipId = id;
        localBody = body;
        parentOrigin = origin;

        ImageClipVisual visual;
        visual.bounds = body;
        visual.title = "ImageTest";
        return visual;
    }

    int calls = 0;
    std::string clipId;
    juce::Rectangle<float> localBody;
    juce::Point<float> parentOrigin;
};

class TestLookAndFeel final : public juce::LookAndFeel_V4,
                              public ClipLookAndFeelMethods
{
public:
    void drawAudioClip (juce::Graphics&,
                        const AudioClipVisual& visual,
                        AudioClipComponent& component) override
    {
        ++calls;
        lastBounds = visual.bounds;
        lastComponent = &component;
    }

    void drawMidiClip (juce::Graphics&,
                       const MidiClipVisual& visual,
                       MidiClipComponent& component) override
    {
        ++midiCalls;
        lastMidiBounds = visual.bounds;
        lastMidiComponent = &component;
    }

    void drawLineClip (juce::Graphics&, const LineClipVisual&, LineClipComponent&) override
    {
        ++lineCalls;
    }

    void drawKeyframeLane (juce::Graphics&, const KeyframeLaneVisual&, KeyframeLaneComponent&) override
    {
        ++keyframeCalls;
    }

    void drawContinuousLine (juce::Graphics&, const ContinuousLineVisual&, ContinuousLineComponent&) override
    {
        ++continuousCalls;
    }

    void drawImageClip (juce::Graphics&,
                        const ImageClipVisual& visual,
                        ImageClipComponent& component) override
    {
        ++imageCalls;
        lastImageBounds = visual.bounds;
        lastImageComponent = &component;
    }

    int calls = 0;
    juce::Rectangle<float> lastBounds;
    AudioClipComponent* lastComponent = nullptr;

    int midiCalls = 0;
    juce::Rectangle<float> lastMidiBounds;
    MidiClipComponent* lastMidiComponent = nullptr;
    int lineCalls = 0;
    int keyframeCalls = 0;
    int continuousCalls = 0;

    int imageCalls = 0;
    juce::Rectangle<float> lastImageBounds;
    ImageClipComponent* lastImageComponent = nullptr;
};
}

int main()
{
    int checks = 0;
    const auto require = [&checks](const bool condition, const char* message)
    {
        ++checks;
        if (!condition) { throw std::runtime_error(message); }
    };

    try
    {
        juce::ScopedJuceInitialiser_GUI juce;
        TestProvider provider;
        TestLookAndFeel lookAndFeel;
        AudioClipComponent component ("clip-audio-1", provider);
        component.setLookAndFeel(&lookAndFeel);

        const juce::Rectangle<float> requestedBody (10.25f, 14.0f, 123.5f, 80.0f);
        component.setBodyBoundsInParent(requestedBody);

        require(component.getClipId() == "clip-audio-1", "clip identity changed");
        require(component.getClipKind() == ClipKind::audio, "audio kind not reported");
        require(!component.hitTest(1, 1), "clip child intercepted canvas mouse input");

        const auto reconstructedBody = component.getBodyBounds().translated(
            component.getParentOrigin().x, component.getParentOrigin().y);
        require(reconstructedBody == requestedBody, "float body geometry was rounded");

        juce::Image image (juce::Image::ARGB,
                           juce::jmax(1, component.getWidth()),
                           juce::jmax(1, component.getHeight()), true);
        juce::Graphics graphics (image);
        component.paint(graphics);

        require(provider.calls == 1, "audio presentation provider not called once");
        require(provider.clipId == "clip-audio-1", "provider received wrong identity");
        require(provider.localBody == component.getBodyBounds(), "provider received wrong local body");
        require(lookAndFeel.calls == 1, "clip LookAndFeel dispatch not called once");
        require(lookAndFeel.lastBounds == component.getBodyBounds(), "LookAndFeel received wrong bounds");
        require(lookAndFeel.lastComponent == &component, "LookAndFeel received wrong component");

        // 1b. Test MidiClipComponent identity, provider and LookAndFeel dispatch
        TestMidiProvider midiProvider;
        MidiClipComponent midiComponent ("clip-midi-1", midiProvider);
        midiComponent.setLookAndFeel(&lookAndFeel);
        midiComponent.setBodyBoundsInParent(requestedBody);

        require(midiComponent.getClipId() == "clip-midi-1", "midi clip identity changed");
        require(midiComponent.getClipKind() == ClipKind::midiNote, "midi kind not reported");
        require(!midiComponent.hitTest(1, 1), "midi clip child intercepted canvas mouse input");

        midiComponent.paint(graphics);
        require(midiProvider.calls == 1, "midi presentation provider not called once");
        require(midiProvider.clipId == "clip-midi-1", "midi provider received wrong identity");
        require(midiProvider.localBody == midiComponent.getBodyBounds(), "midi provider received wrong local body");
        require(lookAndFeel.midiCalls == 1, "midi LookAndFeel dispatch not called once");
        require(lookAndFeel.lastMidiBounds == midiComponent.getBodyBounds(), "LookAndFeel received wrong midi bounds");
        require(lookAndFeel.lastMidiComponent == &midiComponent, "LookAndFeel received wrong midi component");

        midiComponent.setLookAndFeel(nullptr);

        // 1c. Le tre primitive aggiunte devono appartenere davvero all'aggregato
        // e attraversare lo stesso dispatch LookAndFeel delle clip esistenti.
        TestLineProvider lineProvider;
        LineClipComponent lineComponent ("clip-line-1", lineProvider);
        lineComponent.setLookAndFeel (&lookAndFeel);
        lineComponent.setBodyBoundsInParent (requestedBody);
        lineComponent.paint (graphics);
        require(lineComponent.getClipKind() == ClipKind::lineClip, "line clip kind not reported");
        require(!lineComponent.hitTest (1, 1), "line clip child intercepted canvas input");
        require(lineProvider.calls == 1 && lineProvider.clipId == "clip-line-1",
                "line clip provider dispatch failed");
        require(lookAndFeel.lineCalls == 1, "line clip LookAndFeel dispatch failed");

        TestKeyframeProvider keyframeProvider;
        KeyframeLaneComponent keyframeComponent ("lane-keyframe-1", keyframeProvider, false);
        keyframeComponent.setLookAndFeel (&lookAndFeel);
        keyframeComponent.setBodyBoundsInParent (requestedBody);
        keyframeComponent.paint (graphics);
        require(keyframeComponent.getClipKind() == ClipKind::pointKeyframe,
                "point keyframe kind not reported");
        require(keyframeComponent.getLaneType() == KeyframeLaneType::points,
                "points laneType not reported");
        keyframeComponent.setSpanMode (true);
        require(keyframeComponent.getClipKind() == ClipKind::spanKeyframe,
                "span keyframe kind not reported");
        require(keyframeComponent.getLaneType() == KeyframeLaneType::contiguousSpans,
                "contiguousSpans laneType not reported");
        keyframeComponent.setLaneType (KeyframeLaneType::disjointSpans);
        require(keyframeComponent.getLaneType() == KeyframeLaneType::disjointSpans,
                "disjointSpans laneType not reported");
        require(keyframeComponent.getClipKind() == ClipKind::spanKeyframe,
                "span keyframe kind not retained for disjointSpans");
        require(keyframeProvider.calls == 1 && lookAndFeel.keyframeCalls == 1,
                "keyframe provider or LookAndFeel dispatch failed");

        TestContinuousProvider continuousProvider;
        ContinuousLineComponent continuousComponent ("lane-continuous-1", continuousProvider);
        continuousComponent.setLookAndFeel (&lookAndFeel);
        continuousComponent.setBodyBoundsInParent (requestedBody);
        continuousComponent.paint (graphics);
        require(continuousComponent.getClipKind() == ClipKind::continuousLine,
                "continuous line kind not reported");
        require(continuousProvider.calls == 1 && lookAndFeel.continuousCalls == 1,
                "continuous provider or LookAndFeel dispatch failed");

        lineComponent.setLookAndFeel (nullptr);
        keyframeComponent.setLookAndFeel (nullptr);
        continuousComponent.setLookAndFeel (nullptr);

        // 1d. Test ImageClipComponent identity, provider, LookAndFeel dispatch e clip_opacity_law
        TestImageProvider imageProvider;
        ImageClipComponent imageComponent ("clip-image-1", imageProvider);
        imageComponent.setLookAndFeel (&lookAndFeel);
        imageComponent.setBodyBoundsInParent (requestedBody);
        imageComponent.paint (graphics);
        require(imageComponent.getClipId() == "clip-image-1", "image clip identity changed");
        require(imageComponent.getClipKind() == ClipKind::imageSequence, "image clip kind not reported");
        require(!imageComponent.hitTest (1, 1), "image clip child intercepted canvas input");
        require(imageProvider.calls == 1 && imageProvider.clipId == "clip-image-1",
                "image clip provider dispatch failed");
        require(imageProvider.localBody == imageComponent.getBodyBounds(), "image provider received wrong local body");
        require(lookAndFeel.imageCalls == 1, "image clip LookAndFeel dispatch failed");
        require(lookAndFeel.lastImageBounds == imageComponent.getBodyBounds(), "LookAndFeel received wrong image bounds");
        require(lookAndFeel.lastImageComponent == &imageComponent, "LookAndFeel received wrong image component");
        imageComponent.setLookAndFeel (nullptr);

        // Test clip_opacity_law
        const std::array<OpacityPoint, 3> opPoints {{
            { 0.0f, 1.0f, 0.5f, false },
            { 0.5f, 0.5f, 0.5f, false },
            { 1.0f, 0.0f, 0.5f, false }
        }};
        require(clip_opacity_law::formatOpacityPercentage (0.5f) == "50%", "opacity format failed");
        require(std::abs (clip_opacity_law::evaluateOpacityAt (opPoints.data(), 3, 1.0f, 0.0f) - 1.0f) < 0.001f,
                "opacity eval at 0 failed");
        require(std::abs (clip_opacity_law::evaluateOpacityAt (opPoints.data(), 3, 1.0f, 0.5f) - 0.5f) < 0.001f,
                "opacity eval at 0.5 failed");
        require(std::abs (clip_opacity_law::evaluateOpacityAt (opPoints.data(), 3, 1.0f, 1.0f) - 0.0f) < 0.001f,
                "opacity eval at 1 failed");

        const std::array<OpacityPoint, 2> fadedOpacityPoints {{
            { 0.25f, 1.0f, 0.5f, false },
            { 0.75f, 1.0f, 0.5f, false }
        }};
        const auto alphaFadeValue = clip_opacity_law::evaluateOpacityAt (
            fadedOpacityPoints.data(), 2, 1.0f, 0.125f,
            { 0.0, 1.0 }, { 0.5, 0.5 });
        require(alphaFadeValue >= 0.0f && alphaFadeValue <= 1.0f
                    && alphaFadeValue > 0.5f,
                "image alpha fade control was ignored or escaped its endpoint rectangle");

        const OpacityPoint curvedOpacityStart { 0.0f, 0.2f, 0.75f, false };
        const OpacityPoint curvedOpacityEnd { 1.0f, 0.8f, 0.5f, false };
        const float expectedOpacityMid = static_cast<float> (
            ayra::curves::evaluateBoundedQuadraticValue (
                0.5, curvedOpacityStart.opacity,
                { 0.5, curvedOpacityStart.tension },
                curvedOpacityEnd.opacity));
        require(std::abs (clip_opacity_law::evaluateOpacitySegmentUnclamped (
                             curvedOpacityStart, curvedOpacityEnd, 0.5f)
                         - expectedOpacityMid) < 0.000001f,
                "opacity geometry diverged from canonical curve engine");

        OpacityPoint explicitOpacityStart { 0.1f, 0.2f, 0.5f, false };
        const OpacityPoint explicitOpacityEnd { 0.9f, 0.8f, 0.5f, false };
        clip_opacity_law::setOpacityControlPoint (
            explicitOpacityStart, explicitOpacityEnd, 0.26f, 0.68f);
        const auto opacityControl = clip_opacity_law::opacityControlPoint (
            explicitOpacityStart, explicitOpacityEnd);
        require(std::abs (opacityControl.x - 0.26f) < 0.0001f
                    && std::abs (opacityControl.y - 0.68f) < 0.0001f,
                "opacity handle did not preserve its two-dimensional position");

        OpacityPoint horizontalOpacityStart { 0.0f, 0.5f, 0.5f, false };
        const OpacityPoint horizontalOpacityEnd { 1.0f, 0.5f, 0.5f, false };
        clip_opacity_law::setOpacityControlPoint (
            horizontalOpacityStart, horizontalOpacityEnd, 0.2f, 0.9f);
        require(std::abs (clip_opacity_law::evaluateOpacitySegmentUnclamped (
                             horizontalOpacityStart, horizontalOpacityEnd, 0.7f)
                          - 0.5f) < 0.0001f,
                "bounded opacity control escaped a zero-height endpoint rectangle");

        const auto opacitySplit = clip_opacity_law::splitOpacitySegment (
            explicitOpacityStart, explicitOpacityEnd, 0.43f);
        auto opacityLeft = explicitOpacityStart;
        OpacityPoint opacityCut {
            0.43f, opacitySplit.cutValue, 0.5f, false
        };
        opacityLeft.controlX = static_cast<float> (opacitySplit.leftControl.x);
        opacityLeft.controlY = static_cast<float> (opacitySplit.leftControl.y);
        opacityLeft.controlWeight = static_cast<float> (
            opacitySplit.leftControl.weight);
        opacityCut.controlX = static_cast<float> (opacitySplit.rightControl.x);
        opacityCut.controlY = static_cast<float> (opacitySplit.rightControl.y);
        opacityCut.controlWeight = static_cast<float> (
            opacitySplit.rightControl.weight);
        require(std::abs (clip_opacity_law::evaluateOpacitySegmentUnclamped (
                             opacityLeft, opacityCut, 0.25f)
                         - clip_opacity_law::evaluateOpacitySegmentUnclamped (
                             explicitOpacityStart, explicitOpacityEnd, 0.25f)) < 0.0001f
                    && std::abs (clip_opacity_law::evaluateOpacitySegmentUnclamped (
                                    opacityCut, explicitOpacityEnd, 0.75f)
                                - clip_opacity_law::evaluateOpacitySegmentUnclamped (
                                    explicitOpacityStart, explicitOpacityEnd, 0.75f)) < 0.0001f,
                "opacity split did not preserve the exact curve");

        // 2. Test Space Reclamation e Overflow dinamici delle decorazioni
        component.setDecorationOverflows(0.0f, 0.0f, 7.0f);
        require(component.getTopOverflow() == 0.0f, "top overflow not reclaimed");
        require(component.getBottomOverflow() == 0.0f, "bottom overflow not reclaimed");

        component.setBodyBoundsInParent(requestedBody);
        const auto reclaimedBody = component.getBodyBounds().translated(
            component.getParentOrigin().x, component.getParentOrigin().y);
        require(reclaimedBody == requestedBody, "reclaimed body bounds diverged");

        // 3. Test logica Level of Detail (LOD) e Space Reclamation metrics
        ClipFeatures base;
        base.warpEnabled = true;
        base.gainEnabled = true;
        base.cutEnabled = true;

        // Full fidelity (altezza 80px)
        const auto lodFull = computeClipFeaturesLOD(base, 80.0f);
        require(lodFull.showWarpRail, "warp rail should be visible at 80px");
        require(lodFull.showGainEnvelope, "gain envelope should be visible at 80px");
        require(lodFull.showCutRail, "cut rail should be visible at 80px");
        require(lodFull.showDisplacementBadges, "badges should be visible at 80px");

        const auto ovFull = computeClipOverflows(lodFull);
        require(ovFull.top == 8.0f, "top overflow should be 8px when warp is visible");
        require(ovFull.bottom == 6.0f, "bottom overflow should be 6px when cut is visible");

        // Compact height (altezza 42px) -> Warp e Cut restano visibili, Gain envelope e badges nascosti
        const auto lodCompact = computeClipFeaturesLOD(base, 42.0f);
        require(lodCompact.showWarpRail, "warp rail must stay visible across zoom levels");
        require(lodCompact.showCutRail, "cut rail must stay visible across zoom levels");
        require(!lodCompact.showGainEnvelope, "gain envelope should be hidden at 42px");
        require(!lodCompact.showDisplacementBadges, "badges should be hidden at 42px");

        // Micro height (altezza 30px) -> Warp e Cut restano SEMPRE visibili con lo zoom
        const auto lodMicro = computeClipFeaturesLOD(base, 30.0f);
        require(lodMicro.showWarpRail, "warp rail must never hide with vertical zoom");
        require(lodMicro.showCutRail, "cut rail must never hide with vertical zoom");
        require(!lodMicro.showGainEnvelope, "gain envelope hidden at micro height");

        // Disabilitazione feature (Space Reclamation: quando warp e' spento, top overflow e' 0px)
        base.warpEnabled = false;
        const auto lodNoWarp = computeClipFeaturesLOD(base, 80.0f);
        require(!lodNoWarp.showWarpRail, "warp rail must be hidden when user disables warp");
        const auto ovNoWarp = computeClipOverflows(lodNoWarp);
        require(ovNoWarp.top == 0.0f, "space reclamation: top overflow must be 0px when warp disabled");

        // Gesture override: se in drag del gain, resta visibile anche ad altezze minime
        base.gainEnabled = true;
        const auto lodDragGain = computeClipFeaturesLOD(base, 30.0f, false, true);
        require(lodDragGain.showGainEnvelope, "active gain drag must override LOD threshold");

        // 4. La geometria MIDI usata da paint e hit-test resta identica.
        MidiNoteVisual melodicNote;
        melodicNote.sourceIndex = 7;
        melodicNote.startNormX = 0.25f;
        melodicNote.endNormX = 0.50f;
        melodicNote.pitch = 60;
        melodicNote.velocity = 100;
        const juce::Rectangle<float> midiBounds (10.0f, 20.0f, 200.0f, 220.0f);
        const auto melodicGeometry = computeMidiNoteGeometry (
            melodicNote, midiBounds, 48, 72, false);
        require(melodicGeometry.bounds.getHeight() <= 18.0f,
                "midi note hit bounds exceeded painted note height");

        const auto drumGeometry = computeMidiNoteGeometry (
            melodicNote, midiBounds, 48, 72, true);
        const float eventX = midiBounds.getX() + melodicNote.startNormX * midiBounds.getWidth();
        require(drumGeometry.bounds.getX() < eventX && drumGeometry.bounds.getRight() > eventX,
                "drum note hit bounds did not cover both sides of diamond");

        // 5. Il badge Program Change condivide la stessa geometria tra paint e hit-test.
        const auto pcFlag = computeMidiProgramChangeFlagBounds (midiBounds, 0.98f);
        require(!pcFlag.isEmpty() && pcFlag.getRight() <= midiBounds.getRight(),
                "program change flag escaped clip bounds");
        require(computeMidiProgramChangeFlagBounds ({ 0.0f, 0.0f, 40.0f, 80.0f }, 0.5f).isEmpty(),
                "program change flag should be omitted on narrow clips");

        require(clip_midi_law::programChangeLabel (0) == "PC 1",
                "program change display must be 1-based");
        require(clip_midi_law::programChangeLabel (127) == "PC 128",
                "program change 127 displayed with wrong user index");

        // 5b. Primitiva canonica di automazione, De Casteljau e BoundaryPolicy
        AutomationPoint autoPt;
        autoPt.normY = 0.75f;
        require(autoPt.normValue == 0.75f, "AutomationPoint union normY and normValue must alias");
        autoPt.normValue = 0.42f;
        require(autoPt.normY == 0.42f, "AutomationPoint union normValue and normY must alias");

        LineClipVisual lineVis;
        require(lineVis.boundaryPolicy == AutomationBoundaryPolicy::pinnedEndpoints,
                "LineClipVisual must default to pinnedEndpoints boundary policy");

        AutomationPoint curveStart { 0.0f, 0.2f, 0.8f, false };
        AutomationPoint curveEnd { 1.0f, 0.6f, 0.5f, false };
        const float expectedMidpoint = static_cast<float> (
            ayra::curves::evaluateBoundedQuadraticValue (
                0.5, curveStart.normValue,
                { 0.5, curveStart.tension }, curveEnd.normValue));
        require(std::abs (evaluateAutomationSegment (curveStart, curveEnd, 0.5f)
                          - expectedMidpoint) < 0.0001f,
                "automation evaluation diverged from canonical bounded curve");
        require(std::abs (evaluateMidiEnvelopeSegment (curveStart, curveEnd, 0.5f)
                          - evaluateAutomationSegment (curveStart, curveEnd, 0.5f)) < 0.0001f,
                "legacy midi envelope wrapper diverged from canonical evaluateAutomationSegment");

        const juce::Rectangle<float> projectedBounds { 10.0f, 20.0f, 100.0f, 80.0f };
        const auto projectedStart = automationPointPixelPosition (
            projectedBounds, curveStart, 0, 2);
        const auto projectedControl = automationControlPixelPosition (
            projectedBounds, curveStart, curveEnd, 0, 2);
        require(std::abs (projectedStart.x - 14.0f) < 0.0001f
                    && projectedControl.x >= projectedStart.x
                    && projectedControl.x <= projectedBounds.getRight() - 4.0f,
                "shared paint/hit projection let the curve precede its endpoint");
        const juce::Rectangle<float> narrowBounds { 10.0f, 20.0f, 6.0f, 40.0f };
        require(std::abs (automationPointRenderX (narrowBounds, 0, 2, 0.0f) - 13.0f) < 0.0001f
                    && std::abs (automationPointRenderX (narrowBounds, 1, 2, 1.0f) - 13.0f) < 0.0001f,
                "endpoint insets inverted inside a narrow clip");

        const auto pointIsInsideOrOn = [&projectedBounds] (const juce::Point<float> point)
        {
            return point.x >= projectedBounds.getX()
                && point.x <= projectedBounds.getRight()
                && point.y >= projectedBounds.getY()
                && point.y <= projectedBounds.getBottom();
        };
        const std::array<GainPoint, 2> gainPoints {{
            { 0.0f, -24.0f, 0.8f, false },
            { 1.0f, 12.0f, 0.5f, false }
        }};
        const auto gainStart = gainPointAsAutomationPoint (gainPoints[0]);
        const auto gainEnd = gainPointAsAutomationPoint (gainPoints[1]);
        const float canonicalGainDb = clip_gain_law::normToDb (
            evaluateAutomationSegment (gainStart, gainEnd, 0.5f));
        require(std::abs (clip_gain_law::evaluateGainDbAt (
                             gainPoints.data(), 2, 0.0f, 0.5f)
                         - canonicalGainDb) < 0.0001f,
                "waveform gain diverged from rendered automation geometry");

        AutomationPoint explicitStart { 0.1f, 0.2f, 0.5f, false };
        AutomationPoint explicitEnd { 0.9f, 0.8f, 0.5f, false };
        explicitStart.controlX = 0.2f;
        explicitStart.controlY = 0.8f;
        const auto explicitControl = automationControlPoint (explicitStart, explicitEnd);
        require(std::abs (explicitControl.x - 0.26f) < 0.0001f
                    && std::abs (explicitControl.y - 0.68f) < 0.0001f,
                "explicit automation control must stay relative to the endpoint rectangle");
        const float explicitMid = evaluateAutomationSegment (
            explicitStart, explicitEnd, 0.5f);
        require(explicitMid > 0.5f && explicitMid < explicitEnd.normY,
                "explicit two-dimensional control did not curve the segment");

        AutomationPoint horizontalStart { 0.0f, 0.5f, 0.5f, false };
        AutomationPoint horizontalEnd { 1.0f, 0.5f, 0.5f, false };
        setAutomationControlPoint (horizontalStart, horizontalEnd, 0.2f, 0.9f);
        require(std::abs (evaluateAutomationSegment (
                             horizontalStart, horizontalEnd, 0.7f) - 0.5f) < 0.0001f,
                "bounded automation control escaped a zero-height endpoint rectangle");

        const auto exactSplit = splitAutomationSegment (
            explicitStart, explicitEnd, 0.43f);
        AutomationPoint exactLeft = explicitStart;
        AutomationPoint exactCut { 0.43f, exactSplit.cutValue, 0.5f, false };
        exactLeft.controlX = static_cast<float> (exactSplit.leftControl.x);
        exactLeft.controlY = static_cast<float> (exactSplit.leftControl.y);
        exactLeft.controlWeight = static_cast<float> (
            exactSplit.leftControl.weight);
        exactCut.controlX = static_cast<float> (exactSplit.rightControl.x);
        exactCut.controlY = static_cast<float> (exactSplit.rightControl.y);
        exactCut.controlWeight = static_cast<float> (
            exactSplit.rightControl.weight);
        for (const float sampleX : { 0.11f, 0.25f, 0.42f })
            require(std::abs (evaluateAutomationSegment (exactLeft, exactCut, sampleX)
                              - evaluateAutomationSegment (explicitStart, explicitEnd, sampleX)) < 0.0001f,
                    "explicit left split must preserve the exact curve");
        for (const float sampleX : { 0.44f, 0.65f, 0.89f })
            require(std::abs (evaluateAutomationSegment (exactCut, explicitEnd, sampleX)
                              - evaluateAutomationSegment (explicitStart, explicitEnd, sampleX)) < 0.0001f,
                    "explicit right split must preserve the exact curve");

        AutomationPoint archedStart { 0.0f, 0.5f, 0.8f, false };
        AutomationPoint archedEnd { 1.0f, 0.5f, 0.5f, false };
        const auto archedSplit = splitAutomationSegment (
            archedStart, archedEnd, 0.5f);
        AutomationPoint archedCut { 0.5f, archedSplit.cutValue, 0.5f, false };
        archedStart.controlX = static_cast<float> (archedSplit.leftControl.x);
        archedStart.controlY = static_cast<float> (archedSplit.leftControl.y);
        archedStart.controlWeight = static_cast<float> (
            archedSplit.leftControl.weight);
        archedCut.controlX = static_cast<float> (archedSplit.rightControl.x);
        archedCut.controlY = static_cast<float> (archedSplit.rightControl.y);
        archedCut.controlWeight = static_cast<float> (
            archedSplit.rightControl.weight);
        require (std::abs (evaluateAutomationSegment (archedStart, archedCut, 0.25f)
                           - evaluateAutomationSegment (
                               AutomationPoint { 0.0f, 0.5f, 0.8f, false },
                               archedEnd, 0.25f)) < 0.0001f
                 && std::abs (evaluateAutomationSegment (archedCut, archedEnd, 0.75f)
                              - evaluateAutomationSegment (
                                  AutomationPoint { 0.0f, 0.5f, 0.8f, false },
                                  archedEnd, 0.75f)) < 0.0001f,
                 "horizontal legacy arch split must remain exact");

        AutomationPoint splitStart { 0.0f, 0.2f, 0.5f, false };
        AutomationPoint splitPivot { 0.5f, 0.6f, 0.5f, false };
        AutomationPoint splitEnd { 1.0f, 0.3f, 0.5f, false };
        splitPivot.controlX = 0.2f;
        splitPivot.controlY = 0.9f;
        const auto sequenceSplit = splitAutomationPointSequence (
            { splitStart, splitPivot, splitEnd }, 0.5f, 0.5f);
        require (sequenceSplit.left.size() == 2
                    && sequenceSplit.right.size() == 2
                    && std::abs (sequenceSplit.right.front().controlX - 0.2f) < 0.0001f
                    && std::abs (sequenceSplit.right.front().controlY - 0.9f) < 0.0001f,
                "split on an existing point must preserve its outgoing control");
        require (std::abs (evaluateAutomationSegment (
                            sequenceSplit.right[0], sequenceSplit.right[1], 0.5f)
                          - evaluateAutomationSegment (
                            splitPivot, splitEnd, 0.75f)) < 0.0001f,
                "existing-point split changed the right segment geometry");

        const ayra::curves::CurveBoundarySpec pinnedBoundary {
            ayra::curves::CurveBoundaryMode::pinnedEndpoints };
        auto pinnedStart = explicitStart;
        auto pinnedEnd = explicitEnd;
        pinnedStart.normX = 0.0f;
        pinnedEnd.normX = 1.0f;
        const auto pinnedSplit = splitAutomationPointSequence (
            { pinnedStart, pinnedEnd }, 0.5f, 0.43f, pinnedBoundary);
        const float pinnedCutValue = evaluateAutomationSegment (
            pinnedStart, pinnedEnd, 0.43f);
        require (pinnedSplit.left.size() >= 2 && pinnedSplit.right.size() >= 2
                    && std::abs (pinnedSplit.left.front().normX) < 0.0001f
                    && std::abs (pinnedSplit.left.back().normX - 1.0f) < 0.0001f
                    && std::abs (pinnedSplit.right.front().normX) < 0.0001f
                    && std::abs (pinnedSplit.right.back().normX - 1.0f) < 0.0001f
                    && std::abs (pinnedSplit.left.back().normY
                                 - pinnedCutValue) < 0.0001f
                    && std::abs (pinnedSplit.right.front().normY
                                 - pinnedCutValue) < 0.0001f
                    && pinnedSplit.leftBoundary.mode
                        == ayra::curves::CurveBoundaryMode::pinnedEndpoints
                    && pinnedSplit.rightBoundary.mode
                        == ayra::curves::CurveBoundaryMode::pinnedEndpoints,
                "split pinned endpoints did not remain pinned at the sampled curve value");

        resetAutomationControlPoint (explicitStart, explicitEnd);
        require(std::abs (evaluateAutomationSegment (
                            explicitStart, explicitEnd, 0.5f) - 0.5f) < 0.0001f,
                "reset automation control must restore a linear segment");

        // Split esatto De Casteljau
        float cutY = 0.0f, leftTension = 0.5f, rightTension = 0.5f;
        splitQuadraticSegmentDeCasteljau (curveStart, curveEnd, 0.5f, cutY, leftTension, rightTension);
        const float directY = evaluateAutomationSegment (curveStart, curveEnd, 0.5f);
        require(std::abs (cutY - directY) < 0.0001f,
                "De Casteljau split value must match curve evaluation at split point");

        require (leftTension >= 0.0f && leftTension <= 1.0f
                    && rightTension >= 0.0f && rightTension <= 1.0f,
                "legacy tension-only split must remain bounded");

        std::vector<AutomationPoint> pinnedDragPoints {
            explicitStart, explicitEnd
        };
        const auto inheritedControl = pinnedDragPoints.front();
        const int liftedSegment = pinAutomationSegmentForVerticalDrag (
            pinnedDragPoints, 0);
        require (liftedSegment == 1 && pinnedDragPoints.size() == 4
                    && std::abs (pinnedDragPoints[0].normX
                               - pinnedDragPoints[1].normX) < 0.0001f
                    && std::abs (pinnedDragPoints[2].normX
                               - pinnedDragPoints[3].normX) < 0.0001f,
                "shift segment drag did not duplicate pins at the original X positions");
        require (std::abs (pinnedDragPoints[1].controlX
                           - inheritedControl.controlX) < 0.0001f
                    && std::abs (pinnedDragPoints[1].controlY
                                 - inheritedControl.controlY) < 0.0001f
                    && std::abs (pinnedDragPoints[0].controlX - 0.5f) < 0.0001f
                    && std::abs (pinnedDragPoints[2].controlX - 0.5f) < 0.0001f,
                "shift segment drag did not preserve the inner curve/default bridges");

        // 6. I tre LOD MIDI hanno altezze realmente editabili e un viewport
        // di pitch bounded anche vicino agli estremi MIDI.
        using ZoomLod = clip_midi_law::ZoomLod;
        require(clip_midi_law::zoomLodForTrackHeight (95.0f) == ZoomLod::preview,
                "95px should remain MIDI preview");
        require(clip_midi_law::zoomLodForTrackHeight (256.0f) == ZoomLod::noteEdit,
                "256px should select MIDI note edit");
        require(clip_midi_law::zoomLodForTrackHeight (400.0f) == ZoomLod::noteAndVelocityEdit,
                "400px should select MIDI velocity edit");
        require(clip_midi_law::nextZoomLod (ZoomLod::noteAndVelocityEdit) == ZoomLod::preview,
                "MIDI LOD cycle did not wrap");

        const auto lowPitchRange = clip_midi_law::pitchRangeCentredAt (0);
        const auto c3PitchRange = clip_midi_law::pitchRangeCentredAt (
            clip_midi_law::centreC3Pitch);
        const auto highPitchRange = clip_midi_law::pitchRangeCentredAt (127);
        require(lowPitchRange.minPitch == 0 && lowPitchRange.maxPitch == 24,
                "low MIDI pitch viewport escaped valid range");
        require(c3PitchRange.minPitch == 36 && c3PitchRange.maxPitch == 60,
                "C3 MIDI pitch viewport is not centred");
        require(highPitchRange.minPitch == 103 && highPitchRange.maxPitch == 127,
                "high MIDI pitch viewport escaped valid range");

        const juce::Rectangle<float> editableBounds (0.0f, 0.0f, 400.0f, 400.0f);
        const auto velocityLayout = computeMidiClipLayout (
            editableBounds, ZoomLod::noteAndVelocityEdit);
        require(!velocityLayout.velocityLaneBounds.isEmpty()
                && velocityLayout.pianoRollBounds.getHeight() > 200.0f,
                "velocity LOD compressed the piano roll below an editable height");

        MidiClipVisual defaultMidiVisual;
        require(!defaultMidiVisual.showPitchScrollbar,
                "MIDI pitch scrollbar must default to hidden until mouse enters clip");
        require(!defaultMidiVisual.pitchScrollbarOnLeft,
                "MIDI pitch scrollbar side must default to right/false");

        const auto leftScrollbar = computeMidiPitchScrollbarGeometry (
            editableBounds, ZoomLod::noteEdit, 36, 60, true);
        const auto rightScrollbar = computeMidiPitchScrollbarGeometry (
            editableBounds, ZoomLod::noteEdit, 36, 60, false);
        require(leftScrollbar.trackBounds.getX() < editableBounds.getCentreX()
                && rightScrollbar.trackBounds.getX() > editableBounds.getCentreX(),
                "MIDI pitch scrollbar did not follow the requested side");
        require(leftScrollbar.trackBounds.getRight() <= editableBounds.getX(),
                "left MIDI pitch scrollbar must reside on the left margin of the clip");
        require(rightScrollbar.trackBounds.getX() >= editableBounds.getRight(),
                "right MIDI pitch scrollbar must reside on the right margin of the clip");
        require(editableBounds.getX() - leftScrollbar.trackBounds.getRight() >= 6.5f,
                "left MIDI pitch scrollbar must provide sufficient clearance from clip border for edge handles");
        require(rightScrollbar.trackBounds.getX() - editableBounds.getRight() >= 6.5f,
                "right MIDI pitch scrollbar must provide sufficient clearance from clip border for edge handles");
        require(!leftScrollbar.trackBounds.expanded (2.0f, 1.0f).intersects (editableBounds),
                "left MIDI pitch scrollbar hit area must not encroach on clip body or edge brackets");
        require(!rightScrollbar.trackBounds.expanded (2.0f, 1.0f).intersects (editableBounds),
                "right MIDI pitch scrollbar hit area must not encroach on clip body or edge brackets");
        require(computeMidiPitchScrollbarGeometry (
                    editableBounds, ZoomLod::preview, 36, 60, true).trackBounds.isEmpty(),
                "MIDI pitch scrollbar must be hidden in preview LOD");

        const auto lowScrollbar = computeMidiPitchScrollbarGeometry (
            editableBounds, ZoomLod::noteEdit, lowPitchRange.minPitch, lowPitchRange.maxPitch, false);
        const auto highScrollbar = computeMidiPitchScrollbarGeometry (
            editableBounds, ZoomLod::noteEdit, highPitchRange.minPitch, highPitchRange.maxPitch, false);
        require(highScrollbar.thumbBounds.getY() < lowScrollbar.thumbBounds.getY(),
                "high pitch scrollbar thumb must be above low pitch thumb");
        require(std::abs (highScrollbar.thumbBounds.getY() - highScrollbar.trackBounds.getY()) < 0.01f,
                "highest pitch scrollbar thumb must be at the top of the track");
        require(std::abs (lowScrollbar.thumbBounds.getBottom() - lowScrollbar.trackBounds.getBottom()) < 0.01f,
                "lowest pitch scrollbar thumb must reach the bottom of the track");

        MidiNoteVisual outOfViewport = melodicNote;
        outOfViewport.pitch = 127;
        require(computeMidiNoteGeometry (
                    outOfViewport, editableBounds, 36, 60, false,
                    ZoomLod::noteEdit).bounds.isEmpty(),
                "off-viewport MIDI note remained hit-testable");

        const auto continuousLayout = computeContinuousLineLayout (
            { 0.0f, 0.0f, 500.0f, 120.0f }, true);
        require(continuousLayout.scaleBounds.getWidth() == 42.0f,
                "continuous-line scale width changed unexpectedly");
        require(continuousLayout.graphBounds.getX() == 44.0f
                && continuousLayout.graphBounds.getRight() == 498.0f,
                "continuous-line paint and hit geometry diverged");

        require(AutomationCurveParams {}.underlayStyle
                    == ayra::TimelineCurveUnderlayStyle::darkBelow,
                "clip curve default must shade below the line");
        require(AudioClipVisual {}.curveUnderlayStyle
                    == ayra::TimelineCurveUnderlayStyle::darkBelow
                && LineClipVisual {}.curveUnderlayStyle
                    == ayra::TimelineCurveUnderlayStyle::darkBelow,
                "audio/line visual defaults no longer shade below the curve");

        // 7. Point e span materializzano la nuova geometria visuale Multiverse.
        KeyframeEventVisual pointEvent;
        pointEvent.startNormX = 0.25f;
        pointEvent.endNormX = 0.75f;
        pointEvent.pillWidth = 48.0f;
        pointEvent.visualRow = 2;
        const juce::Rectangle<float> laneBounds (10.0f, 20.0f, 200.0f, 100.0f);
        const auto pointBounds = keyframe_lane_layout::eventBounds (
            laneBounds, pointEvent, false);
        const auto spanBounds = keyframe_lane_layout::eventBounds (
            laneBounds, pointEvent, true);
        require(pointBounds.getX() == 60.0f && pointBounds.getWidth() == 48.0f,
                "point keyframe pill lost exact event anchor or text width");
        require(pointBounds.getY() == 67.0f
                && pointBounds.getHeight() == keyframe_lane_layout::pillHeight,
                "point keyframe anti-collision row geometry changed");
        require(spanBounds.getX() == 60.0f && spanBounds.getRight() == 160.0f,
                "span keyframe no longer reaches the next event anchor");

        // 8. La ricetta definitiva conserva linea/punti TimelineLab e rende
        // davvero il gradiente teal della corsia Tempo sotto la curva.
        const std::array<AutomationPoint, 3> visualPoints
        {{
            { 0.10f, 0.30f, 0.75f, false },
            { 0.50f, 0.80f, 0.25f, true },
            { 0.90f, 0.45f, 0.50f, false }
        }};
        juce::Image renderedCurve (juce::Image::ARGB, 140, 90, true);
        juce::Graphics curveGraphics (renderedCurve);
        AutomationCurveParams params;
        params.bounds = { 4.0f, 4.0f, 132.0f, 82.0f };
        params.boundaryPolicy = AutomationBoundaryPolicy::fadeEndpoints;
        params.curveColour = juce::Colours::white;
        params.accentColour = juce::Colour (0xffff7a1a);
        params.underlayStyle = ayra::TimelineCurveUnderlayStyle::tempoGradientBelow;
        params.points = visualPoints.data();
        params.numPoints = static_cast<int> (visualPoints.size());
        ClipDefaultLookAndFeel::getDefaultInstance().drawAutomationCurve (
            curveGraphics, params);

        bool foundTempoUnderlayPixel = false;
        for (int y = 20; y < renderedCurve.getHeight() && !foundTempoUnderlayPixel; ++y)
            for (int x = 0; x < renderedCurve.getWidth(); ++x)
            {
                const auto pixel = renderedCurve.getPixelAt (x, y);
                if (pixel.getAlpha() > 0
                    && pixel.getGreen() > pixel.getRed() + 8
                    && pixel.getBlue() > pixel.getRed() + 8)
                {
                    foundTempoUnderlayPixel = true;
                    break;
                }
            }
        require(foundTempoUnderlayPixel,
                "canonical Timeline curve is missing the teal Tempo underlay");

        const auto lightLine = ayra::timeline_visual_style::curveLineColour (
            ayra::timeline_visual_style::CurveLineColourVariant::light);
        const auto tealLine = ayra::timeline_visual_style::curveLineColour (
            ayra::timeline_visual_style::CurveLineColourVariant::tempoTeal);
        const auto amberLine = ayra::timeline_visual_style::curveLineColour (
            ayra::timeline_visual_style::CurveLineColourVariant::accentAmber);
        require(lightLine != tealLine && lightLine != amberLine && tealLine != amberLine,
                "curve comparison palette collapsed to duplicate colours");
        require(tealLine.getGreen() > tealLine.getRed()
                && amberLine.getRed() > amberLine.getGreen(),
                "curve comparison palette lost teal/amber identities");

        juce::Path horizontalCurve;
        horizontalCurve.startNewSubPath (0.0f, 45.0f);
        horizontalCurve.lineTo (139.0f, 45.0f);
        juce::Image darkBelow (juce::Image::ARGB, 140, 90, true);
        juce::Image darkAbove (juce::Image::ARGB, 140, 90, true);
        darkBelow.clear (darkBelow.getBounds(), juce::Colours::white);
        darkAbove.clear (darkAbove.getBounds(), juce::Colours::white);
        juce::Graphics darkBelowGraphics (darkBelow);
        juce::Graphics darkAboveGraphics (darkAbove);
        auto& commonLaf = ayra::CommonDefaultLookAndFeel::getDefaultInstance();
        const juce::Rectangle<float> underlayBounds (0.0f, 0.0f, 140.0f, 90.0f);
        commonLaf.drawTimelineAutomationCurveUnderlay (
            darkBelowGraphics, horizontalCurve, underlayBounds,
            ayra::TimelineCurveUnderlayStyle::darkBelow);
        commonLaf.drawTimelineAutomationCurveUnderlay (
            darkAboveGraphics, horizontalCurve, underlayBounds,
            ayra::TimelineCurveUnderlayStyle::darkAbove);
        require(darkBelow.getPixelAt (70, 70).getBrightness()
                    < darkBelow.getPixelAt (70, 20).getBrightness(),
                "dark-below clip variant shades the wrong side");
        require(darkAbove.getPixelAt (70, 20).getBrightness()
                    < darkAbove.getPixelAt (70, 70).getBrightness(),
                "dark-above clip variant shades the wrong side");

        // Il renderer keyframe canonico vive nello stesso modulo ayra_ui.
        juce::Image renderedKeyframe (juce::Image::ARGB, 140, 40, true);
        juce::Graphics keyframeGraphics (renderedKeyframe);
        const juce::Rectangle<float> keyLane (0.0f, 0.0f, 140.0f, 40.0f);
        const juce::Rectangle<float> keyPill (30.0f, 3.0f, 54.0f, 18.0f);
        commonLaf.drawTimelineKeyframeLaneBackground (keyframeGraphics, keyLane);
        commonLaf.drawTimelineKeyframeEvent (keyframeGraphics, keyLane, keyPill,
                                             "Marker", juce::Colour (0xffe0b020),
                                             true, false, false);
        // Verifica della Legge Canonica delle Altezze (clip_height_law) e Single Source of Truth
        require (clip_midi_law::previewTrackHeight == clip_height_law::compactTrackHeight,
                 "clip_midi_law::previewTrackHeight must match clip_height_law::compactTrackHeight");
        require (clip_midi_law::noteEditTrackHeight == clip_height_law::mediumTrackHeight,
                 "clip_midi_law::noteEditTrackHeight must match clip_height_law::mediumTrackHeight");
        require (clip_midi_law::noteAndVelocityTrackHeight == clip_height_law::fullTrackHeight,
                 "clip_midi_law::noteAndVelocityTrackHeight must match clip_height_law::fullTrackHeight");

        require (clip_height_law::compactTrackHeight == keyframe_lane_layout::contentHeight(1,true),
                 "compact height is the actual event-span height including cut rail");
        require (clip_height_law::nextCanonicalHeight (32.0f) == 400
                 && clip_height_law::nextCanonicalHeight (400.0f) == 32,
                 "non-MIDI has precisely two presentation levels");
        require (clip_height_law::nextCanonicalHeight (32.0f,true) == 256
                 && clip_height_law::nextCanonicalHeight (256.0f,true) == 400
                 && clip_height_law::nextCanonicalHeight (400.0f,true) == 32,
                 "MIDI cycles compact, note editor, note+velocity editor");
        require (clip_midi_law::zoomLodForTrackHeight(256)==clip_midi_law::ZoomLod::noteEdit
                 && clip_midi_law::zoomLodForTrackHeight(400)==clip_midi_law::ZoomLod::noteAndVelocityEdit,
                 "MIDI levels must reach real semantic editing states");
        require (clip_height_law::nextModelHeight(32,2)==200,"legacy zoom applied exactly once");
        require (clip_height_law::isHeaderAreaOrClosedClip(TimelineClipZone::clipTitleMove,300)
                 && clip_height_law::isHeaderAreaOrClosedClip(TimelineClipZone::none,18)
                 && !clip_height_law::isHeaderAreaOrClosedClip(TimelineClipZone::none,300),
                 "old hit policy, independent from new height action");
        for (auto zone : {TimelineClipZone::gainEnvelopeLine, TimelineClipZone::gainEnvelopePoint,
                          TimelineClipZone::warpMarker, TimelineClipZone::fadeInCurveHandle,
                          TimelineClipZone::fadeOutCurveHandle, TimelineClipZone::edgeLeftTrim,
                          TimelineClipZone::midiPitchScrollbar,TimelineClipZone::midiNoteBody,
                          TimelineClipZone::midiVelocityStem,TimelineClipZone::bottomCutRail})
            require (!clip_height_law::isHeaderAreaOrClosedClip(zone,18),
                     "specialist double clicks never change height even at the compact level");
        for(int height=32;height<=128;++height) {
            using namespace ayra::timeline_panel::visual_metrics;
            const auto layout=ClipRowGeometry::make(height,true,true);
            const auto f=computeClipFeaturesLOD({},layout.body,false,false,static_cast<float>(height));
            require(f.showWarpRail,"internal warp lines never vanish with vertical zoom");
            require(f.showWarpControls==(height>=64),"warp control threshold is LANE height");
            require(f.showGainEnvelope!=f.showEnvelopePreview,"preview and editor are exclusive");
            require(layout.top+layout.body+cutRailExtent+eventPadding==height,"compact geometry fits the lane");
            for(float rel:{0.1f,0.35f,0.6f,0.9f})for(bool left:{false,true})
                if(height<96)require(computeClipEdgeZone(left,rel,height)==(left?TimelineClipZone::edgeLeftSlip:TimelineClipZone::edgeRightSlip),
                    "compact edge consistently uses third bracket from bottom");
        }
        require(resolveTimelineCursorRole({TimelineClipZone::bottomCutRail,false,true})==TimelineCursorRole::glue,
                "Shift immediately switches cut rail to Glue");
        require(resolveTimelineCursorRole({TimelineClipZone::bottomCutRail,false,false})==TimelineCursorRole::cut,
                "release Shift restores scissors");

        // Verifica del Principio Canonico del Tratteggio: canAutomationPointMoveHorizontally
        require (! canAutomationPointMoveHorizontally (AutomationBoundaryPolicy::pinnedEndpoints, 0, 3),
                 "pinnedEndpoints start point (0) must NOT move horizontally");
        require (! canAutomationPointMoveHorizontally (AutomationBoundaryPolicy::pinnedEndpoints, 2, 3),
                 "pinnedEndpoints end point (last) must NOT move horizontally");
        require (canAutomationPointMoveHorizontally (AutomationBoundaryPolicy::pinnedEndpoints, 1, 3),
                 "pinnedEndpoints intermediate point (1) must move horizontally");
        require (canAutomationPointMoveHorizontally (AutomationBoundaryPolicy::fadeEndpoints, 0, 3),
                 "fadeEndpoints start point (0) can move horizontally for fade in");
        require (canAutomationPointMoveHorizontally (AutomationBoundaryPolicy::fadeEndpoints, 2, 3),
                 "fadeEndpoints end point (last) can move horizontally for fade out");
        require (! canAutomationPointBeRemoved (0, 3)
                    && canAutomationPointBeRemoved (1, 3)
                    && ! canAutomationPointBeRemoved (2, 3),
                 "first/last automation points must be permanent for every boundary policy");
        require (isClipDeletionZone (TimelineClipZone::clipTitleMove)
                    && ! isClipDeletionZone (TimelineClipZone::none)
                    && ! isClipDeletionZone (TimelineClipZone::clipBody)
                    && ! isClipDeletionZone (
                        TimelineClipZone::gainEnvelopeLine)
                    && semanticClipZone (TimelineClipZone::none)
                        == TimelineClipZone::clipBody,
                 "clip deletion must remain exclusive to the header");
        require (resolveTimelineCursorRole ({ TimelineClipZone::clipTitleMove,
                    true, false, true, true }) == TimelineCursorRole::erase,
                 "Alt over a deletable clip header must expose the eraser cursor");
        require (resolveTimelineCursorRole ({ TimelineClipZone::clipTitleMove,
                    false, false, true, true }) == TimelineCursorRole::move,
                 "a clip header without Alt must retain its move cursor");
        require (resolveTimelineCursorRole ({ TimelineClipZone::bottomCutRail })
                    == TimelineCursorRole::cut,
                 "the clip split rail must expose the cut cursor");
        require (resolveTimelineCursorRole ({ TimelineClipZone::gainEnvelopePoint,
                    true, false, true, false })
                    == TimelineCursorRole::twoDimensionalResize,
                 "Alt must not advertise deletion for permanent endpoints");
        require (resolveTimelineCursorRole ({ TimelineClipZone::gainEnvelopePoint,
                    true, false, true, true }) == TimelineCursorRole::erase,
                 "Alt over removable automation points must expose the eraser cursor");
        require (resolveTimelineCursorRole ({ TimelineClipZone::gainEnvelopePoint,
                    false, true, true, true }) == TimelineCursorRole::verticalResize,
                 "Shift over a movable automation point must expose vertical-only movement");
        require (resolveTimelineCursorRole ({ TimelineClipZone::midiNoteBody,
                    true, false, true, true }) == TimelineCursorRole::erase,
                 "Alt over a MIDI note must expose the eraser cursor");
        const auto pointerContext = makeTimelinePointerContext (
            "track", "clip", TimelinePointerTarget::clip,
            ClipKind::lineClip, TimelineClipZone::gainEnvelopeLine,
            { 20.0f, 30.0f }, 4.0, {});
        require (pointerContext.trackId == "track"
                    && pointerContext.itemId == "clip"
                    && pointerContext.target == TimelinePointerTarget::clip
                    && pointerContext.zone == TimelineClipZone::gainEnvelopeLine
                    && std::abs (timelineSecondsFromCanvasX (20.0f, 160) - 4.0)
                        < 0.0001,
                 "structured pointer context lost its semantic target or time");
        auto keyframeContext = makeTimelinePointerContext (
            "lane", "42", TimelinePointerTarget::keyframeEvent,
            ClipKind::spanKeyframe, TimelineClipZone::keyframeBody,
            { 10.0f, 5.0f }, 2.0, {});
        keyframeContext.keyframeLaneType = KeyframeLaneType::disjointSpans;
        keyframeContext.eventId = 42;
        require (keyframeContext.keyframeLaneType
                        == KeyframeLaneType::disjointSpans
                    && keyframeContext.eventId == 42,
                 "structured keyframe context lost its lane/event identity");

        const auto sanitizedFadeEndpoints =
            ayra::curves::fadeEndpointPositionsFromDurations (10.0, 8.0, 7.0);
        require (sanitizedFadeEndpoints.first
                    <= sanitizedFadeEndpoints.last
                    && std::abs (sanitizedFadeEndpoints.first - 8.0 / 15.0)
                        < 0.0001f
                    && std::abs (sanitizedFadeEndpoints.last - 8.0 / 15.0)
                        < 0.0001f,
                 "overlapping legacy fade durations inverted the endpoints");
        const auto zeroLengthFadeEndpoints =
            ayra::curves::fadeEndpointPositionsFromDurations (0.0, 1.0, 1.0);
        require (zeroLengthFadeEndpoints.first == 0.0
                    && zeroLengthFadeEndpoints.last == 1.0,
                 "zero-length clips must produce finite default fade endpoints");

        const std::array<AutomationPoint, 2> durationProjectionPoints {{
            { 0.25f, 0.2f, 0.5f, false },
            { 0.75f, 0.8f, 0.5f, false }
        }};
        const auto projectedDurations = projectAutomationFadeDurations (
            durationProjectionPoints.data(), 2, 8.0,
            [] (const AutomationPoint& point) noexcept { return point; });
        require (std::abs (projectedDurations.fadeIn - 2.0) < 0.0001
                    && std::abs (projectedDurations.fadeOut - 2.0) < 0.0001,
                 "fade duration compatibility projection diverged from endpoint X");

        std::array<AutomationPoint, 3> pinnedPositionPoints {{
            { 0.20f, 0.25f, 0.5f, false },
            { 0.50f, 0.50f, 0.5f, false },
            { 0.80f, 0.75f, 0.5f, false }
        }};
        enforceAutomationBoundaryPositions (
            pinnedPositionPoints.data(),
            static_cast<int> (pinnedPositionPoints.size()),
            AutomationBoundaryPolicy::pinnedEndpoints);
        require (pinnedPositionPoints.front().normX == 0.0f
                    && pinnedPositionPoints[1].normX == 0.50f
                    && pinnedPositionPoints.back().normX == 1.0f,
                 "pinned endpoint positions were not restored canonically");

        // Gli anchor impliciti possono avere valori arbitrari e restano una
        // sola geometria per paint, hit-test, valutazione e split.
        const std::array<AutomationPoint, 2> boundaryPoints {{
            { 0.20f, 0.25f, 0.5f, false },
            { 0.80f, 0.75f, 0.5f, false }
        }};
        ayra::curves::CurveBoundarySpec boundarySpec;
        boundarySpec.mode = AutomationBoundaryPolicy::fadeEndpoints;
        boundarySpec.startValue = 0.5;
        boundarySpec.endValue = 0.6;
        boundarySpec.fadeInControl = { 0.0, 1.0 };
        boundarySpec.fadeOutControl = { 1.0, 0.0 };

        require (std::abs (evaluateAutomationCurveAt (
                              boundaryPoints.data(), 2, 0.0f,
                              boundarySpec, 0.0f) - 0.5f) < 0.0001f
                    && std::abs (evaluateAutomationCurveAt (
                                    boundaryPoints.data(), 2, 0.0f,
                                    boundarySpec, 0.20f) - 0.25f) < 0.0001f
                    && std::abs (evaluateAutomationCurveAt (
                                    boundaryPoints.data(), 2, 0.0f,
                                    boundarySpec, 1.0f) - 0.6f) < 0.0001f,
                 "configurable boundary values are not respected by evaluation");

        const std::array<GainPoint, 2> unifiedGainFade {{
            { 0.20f, 0.0f, 0.5f, false },
            { 0.80f, 0.0f, 0.5f, false }
        }};
        ayra::curves::CurveBoundarySpec gainBoundary;
        gainBoundary.mode = AutomationBoundaryPolicy::fadeEndpoints;
        gainBoundary.fadeInControl = { 0.0, 1.0 };
        gainBoundary.fadeOutControl = { 1.0, 0.0 };
        require (clip_gain_law::evaluateGainDbAt (
                     unifiedGainFade.data(), 2, 0.0f, 0.0f,
                     gainBoundary) <= -59.0f
                    && std::abs (clip_gain_law::evaluateGainDbAt (
                                     unifiedGainFade.data(), 2, 0.0f, 0.20f,
                                     gainBoundary)) < 0.0001f
                    && clip_gain_law::evaluateGainDbAt (
                           unifiedGainFade.data(), 2, 0.0f, 0.10f,
                           gainBoundary) > -30.0f,
                 "audio gain endpoints are not the canonical fade endpoints");

        const auto projectedGainGeometry = computeProjectedAutomationBoundaryGeometry (
            projectedBounds, unifiedGainFade.data(), 2, gainBoundary,
            [] (const GainPoint& point) noexcept
            {
                return gainPointAsAutomationPoint (point);
            });
        require (projectedGainGeometry.hasFadeIn
                    && projectedGainGeometry.hasFadeOut
                    && pointIsInsideOrOn (projectedGainGeometry.fadeIn.control)
                    && pointIsInsideOrOn (projectedGainGeometry.fadeOut.control),
                 "projected audio fade geometry diverged from the canonical boundary path");

        const auto boundaryGeometry = computeAutomationBoundaryGeometry (
            projectedBounds, boundaryPoints.data(), 2, boundarySpec);
        require (boundaryGeometry.hasFadeIn && boundaryGeometry.hasFadeOut
                    && boundaryGeometry.showGuide
                    && std::abs (boundaryGeometry.startAnchor.y - 60.0f) < 0.0001f
                    && std::abs (boundaryGeometry.endAnchor.y - 52.0f) < 0.0001f
                    && pointIsInsideOrOn (boundaryGeometry.fadeIn.control)
                    && pointIsInsideOrOn (boundaryGeometry.fadeOut.control),
                 "boundary geometry escaped the endpoint rectangles");

        const std::vector<AutomationPoint> boundaryVector (
            boundaryPoints.begin(), boundaryPoints.end());
        const auto boundarySplit = splitAutomationPointSequence (
            boundaryVector, 0.0f, 0.55f, boundarySpec);
        require (! boundarySplit.left.empty() && ! boundarySplit.right.empty(),
                 "boundary-aware split returned an empty child");
        require (boundarySplit.left.front().normX > 0.0f
                    && boundarySplit.right.back().normX < 1.0f,
                 "split materialized implicit fade anchors as editable points");
        for (int sample = 0; sample <= 100; ++sample)
        {
            const float x = static_cast<float> (sample) / 100.0f;
            const float original = evaluateAutomationCurveAt (
                boundaryPoints.data(), 2, 0.0f, boundarySpec, x);
            const bool useLeft = x <= 0.55f;
            const auto& child = useLeft ? boundarySplit.left : boundarySplit.right;
            const auto& childBoundary = useLeft
                ? boundarySplit.leftBoundary : boundarySplit.rightBoundary;
            const float childX = useLeft ? x / 0.55f : (x - 0.55f) / 0.45f;
            const float reconstructed = evaluateAutomationCurveAt (
                child.data(), static_cast<int> (child.size()), 0.0f,
                childBoundary, childX);
            require (std::abs (original - reconstructed) < 0.0002f,
                     "boundary-aware split changed the curve");
        }

        for (const float edgeSplitX : { 0.10f, 0.90f })
        {
            const auto edgeSplit = splitAutomationPointSequence (
                boundaryVector, 0.0f, edgeSplitX, boundarySpec);
            require (edgeSplit.left.size() >= 2 && edgeSplit.right.size() >= 2,
                     "split inside a fade did not preserve editable endpoints");

            for (int sample = 0; sample <= 100; ++sample)
            {
                const float x = static_cast<float> (sample) / 100.0f;
                const float original = evaluateAutomationCurveAt (
                    boundaryPoints.data(), 2, 0.0f, boundarySpec, x);
                const bool useLeft = x <= edgeSplitX;
                const auto& child = useLeft ? edgeSplit.left : edgeSplit.right;
                const auto& childBoundary = useLeft
                    ? edgeSplit.leftBoundary : edgeSplit.rightBoundary;
                const float localX = useLeft ? x / edgeSplitX
                    : (x - edgeSplitX) / (1.0f - edgeSplitX);
                const float reconstructed = evaluateAutomationCurveAt (
                    child.data(), static_cast<int> (child.size()), 0.0f,
                    childBoundary, localX);
                require (std::abs (original - reconstructed) < 0.0002f,
                         "split inside a fade changed the curve");
            }
        }

        // =====================================================================
        // Test suite per i 4 quadranti dei bordi clip (Loop / Slip / Trim / Stretch)
        // e la matematica deterministica del Loop Drag (Cubase-style)
        // =====================================================================

        // 1. Risoluzione dei 4 quadranti esatti al 25% dell'altezza clip
        require (computeClipEdgeZone (true, 0.00f) == TimelineClipZone::edgeLeftLoop,
                 "relY 0.00 left must resolve to edgeLeftLoop");
        require (computeClipEdgeZone (true, 0.15f) == TimelineClipZone::edgeLeftLoop,
                 "relY 0.15 left must resolve to edgeLeftLoop");
        require (computeClipEdgeZone (true, 0.25f) == TimelineClipZone::edgeLeftLoop,
                 "relY 0.25 left must resolve to edgeLeftLoop");
        require (computeClipEdgeZone (true, 0.26f) == TimelineClipZone::edgeLeftSlip,
                 "relY 0.26 left must resolve to edgeLeftSlip");
        require (computeClipEdgeZone (true, 0.50f) == TimelineClipZone::edgeLeftSlip,
                 "relY 0.50 left must resolve to edgeLeftSlip");
        require (computeClipEdgeZone (true, 0.51f) == TimelineClipZone::edgeLeftTrim,
                 "relY 0.51 left must resolve to edgeLeftTrim");
        require (computeClipEdgeZone (true, 0.75f) == TimelineClipZone::edgeLeftTrim,
                 "relY 0.75 left must resolve to edgeLeftTrim");
        require (computeClipEdgeZone (true, 0.76f) == TimelineClipZone::edgeLeftStretch,
                 "relY 0.76 left must resolve to edgeLeftStretch");
        require (computeClipEdgeZone (true, 1.00f) == TimelineClipZone::edgeLeftStretch,
                 "relY 1.00 left must resolve to edgeLeftStretch");

        require (computeClipEdgeZone (false, 0.10f) == TimelineClipZone::edgeRightLoop,
                 "relY 0.10 right must resolve to edgeRightLoop");
        require (computeClipEdgeZone (false, 0.35f) == TimelineClipZone::edgeRightSlip,
                 "relY 0.35 right must resolve to edgeRightSlip");
        require (computeClipEdgeZone (false, 0.60f) == TimelineClipZone::edgeRightTrim,
                 "relY 0.60 right must resolve to edgeRightTrim");
        require (computeClipEdgeZone (false, 0.85f) == TimelineClipZone::edgeRightStretch,
                 "relY 0.85 right must resolve to edgeRightStretch");

        // 2. Invarianti di appartenenza isClipEdgeZone / isLeftClipEdgeZone / isRightClipEdgeZone
        require (isClipEdgeZone (TimelineClipZone::edgeLeftLoop)
                 && isClipEdgeZone (TimelineClipZone::edgeRightLoop)
                 && isClipEdgeZone (TimelineClipZone::edgeLeftSlip)
                 && isClipEdgeZone (TimelineClipZone::edgeRightSlip)
                 && isClipEdgeZone (TimelineClipZone::edgeLeftTrim)
                 && isClipEdgeZone (TimelineClipZone::edgeRightTrim)
                 && isClipEdgeZone (TimelineClipZone::edgeLeftStretch)
                 && isClipEdgeZone (TimelineClipZone::edgeRightStretch),
                 "All 8 edge zones must be recognized by isClipEdgeZone");
        require (! isClipEdgeZone (TimelineClipZone::none)
                 && ! isClipEdgeZone (TimelineClipZone::bottomCutRail)
                 && ! isClipEdgeZone (TimelineClipZone::clipTitleMove),
                 "Non-edge zones must not be recognized by isClipEdgeZone");
        require (isLeftClipEdgeZone (TimelineClipZone::edgeLeftLoop)
                 && ! isRightClipEdgeZone (TimelineClipZone::edgeLeftLoop),
                 "edgeLeftLoop side classification invariant");
        require (isRightClipEdgeZone (TimelineClipZone::edgeRightLoop)
                 && ! isLeftClipEdgeZone (TimelineClipZone::edgeRightLoop),
                 "edgeRightLoop side classification invariant");

        // 3. Calcolo numero duplicati computeLoopDuplicateCount (Destra: trascinamento in avanti)
        constexpr double kTestClipLen = 1.0;
        constexpr double kTestClipStart = 4.0;
        // Trascina verso l'interno (accorcia) -> 0 duplicati (comportamento di trim)
        require (computeLoopDuplicateCount (true, -0.5, kTestClipStart, kTestClipLen) == 0,
                 "Dragging right edge inwards must produce 0 loop duplicates");
        require (computeLoopDuplicateCount (true, 0.0, kTestClipStart, kTestClipLen) == 0,
                 "Zero delta must produce 0 loop duplicates");
        // Rilasciato tra fine clip e 1 secondo -> 1 duplicato
        require (computeLoopDuplicateCount (true, 0.1, kTestClipStart, kTestClipLen) == 1,
                 "Dragging 0.1s beyond end must produce 1 duplicate");
        require (computeLoopDuplicateCount (true, 1.0, kTestClipStart, kTestClipLen) == 1,
                 "Dragging 1.0s beyond end must produce 1 duplicate");
        // Rilasciato dopo piu' di 1 secondo -> tanti duplicati quanti necessari a coprire il range (ceil)
        require (computeLoopDuplicateCount (true, 1.01, kTestClipStart, kTestClipLen) == 2,
                 "Dragging 1.01s beyond end must produce 2 duplicates");
        require (computeLoopDuplicateCount (true, 2.0, kTestClipStart, kTestClipLen) == 2,
                 "Dragging 2.0s beyond end must produce 2 duplicates");
        require (computeLoopDuplicateCount (true, 2.4, kTestClipStart, kTestClipLen) == 3,
                 "Dragging 2.4s beyond end must produce 3 duplicates");

        // 4. Calcolo numero duplicati computeLoopDuplicateCount (Sinistra: trascinamento all'indietro)
        require (computeLoopDuplicateCount (false, 0.5, kTestClipStart, kTestClipLen) == 0,
                 "Dragging left edge inwards must produce 0 loop duplicates");
        require (computeLoopDuplicateCount (false, -0.5, kTestClipStart, kTestClipLen) == 1,
                 "Dragging left edge -0.5s outwards must produce 1 duplicate");
        require (computeLoopDuplicateCount (false, -1.0, kTestClipStart, kTestClipLen) == 1,
                 "Dragging left edge -1.0s outwards must produce 1 duplicate");
        require (computeLoopDuplicateCount (false, -1.5, kTestClipStart, kTestClipLen) == 2,
                 "Dragging left edge -1.5s outwards must produce 2 duplicates");
        // Bounded by available space before start (cannot place clips before t=0)
        require (computeLoopDuplicateCount (false, -10.0, 2.0, kTestClipLen) == 2,
                 "Dragging left edge beyond timeline start must clamp duplicates to maxAvailable");

        // 5. Calcolo posizioni e bounds duplicati (computeLoopDuplicateStart / computeLoopDuplicateBounds)
        require (std::abs (computeLoopDuplicateStart (true, 0, 4.0, 1.5) - 5.5) < 0.0001,
                 "Right duplicate 0 start must be 4.0 + 1.5 = 5.5");
        require (std::abs (computeLoopDuplicateStart (true, 1, 4.0, 1.5) - 7.0) < 0.0001,
                 "Right duplicate 1 start must be 4.0 + 3.0 = 7.0");
        require (std::abs (computeLoopDuplicateStart (false, 0, 4.0, 1.5) - 2.5) < 0.0001,
                 "Left duplicate 0 start must be 4.0 - 1.5 = 2.5");
        require (std::abs (computeLoopDuplicateStart (false, 1, 4.0, 1.5) - 1.0) < 0.0001,
                 "Left duplicate 1 start must be 4.0 - 3.0 = 1.0");

        const juce::Rectangle<float> testOrigBounds (100.0f, 10.0f, 50.0f, 80.0f);
        const auto rightDupBounds0 = computeLoopDuplicateBounds (testOrigBounds, true, 0);
        require (std::abs (rightDupBounds0.getX() - 150.0f) < 0.001f
                 && std::abs (rightDupBounds0.getWidth() - 50.0f) < 0.001f,
                 "Right duplicate 0 bounds x must be 150.0f");
        const auto rightDupBounds1 = computeLoopDuplicateBounds (testOrigBounds, true, 1);
        require (std::abs (rightDupBounds1.getX() - 200.0f) < 0.001f,
                 "Right duplicate 1 bounds x must be 200.0f");
        const auto leftDupBounds0 = computeLoopDuplicateBounds (testOrigBounds, false, 0);
        require (std::abs (leftDupBounds0.getX() - 50.0f) < 0.001f,
                 "Left duplicate 0 bounds x must be 50.0f");

        // 7. Test suite per Keyframe Spans e Blocks
        {
            const juce::Rectangle<float> testLaneBounds (4.0f, 4.0f, 992.0f, 112.0f);
            KeyframeEventVisual evPoint;
            evPoint.startNormX = 0.25f;
            evPoint.pillWidth = 40.0f;
            const auto boundsPoint = keyframe_lane_layout::eventBounds (testLaneBounds, evPoint, false);
            require (std::abs (boundsPoint.getX() - (4.0f + 0.25f * 992.0f)) < 0.01f,
                     "point keyframe bounds must match startNormX");
            require (boundsPoint.getWidth() == 40.0f,
                     "point keyframe pill width must match pillWidth");

            KeyframeEventVisual evSpan;
            evSpan.startNormX = 0.10f;
            evSpan.endNormX = 0.40f;
            const auto boundsSpan = keyframe_lane_layout::eventBounds (testLaneBounds, evSpan, true);
            require (std::abs (boundsSpan.getX() - (4.0f + 0.10f * 992.0f)) < 0.01f,
                     "span start x must match startNormX");
            require (std::abs (boundsSpan.getWidth() - 0.30f * 992.0f) < 0.01f,
                     "span width must match (endNormX - startNormX) * laneWidth");
        }

        // 8. Test suite per trimAutomationPointsWindow (Trim 1 e Trim 2)
        {
            // pinnedEndpoints
            const ayra::curves::CurveBoundarySpec pinnedSpec {
                ayra::curves::CurveBoundaryMode::pinnedEndpoints };
            std::vector<AutomationPoint> origPinned {
                { 0.0f, 0.20f, 0.5f, false },
                { 0.5f, 0.80f, 0.5f, false },
                { 1.0f, 0.40f, 0.5f, false }
            };

            // 8.1 Trim 1 Right / riduzione range: endpoint assume valore curva nel punto di taglio
            const auto trimmedRight = trimAutomationPointsWindow (
                origPinned, pinnedSpec, 0.0f, 0.0, 2.0, 0.0, 1.5);
            require (trimmedRight.size() >= 2
                     && std::abs (trimmedRight.front().normX) < 0.0001f
                     && std::abs (trimmedRight.back().normX - 1.0f) < 0.0001f,
                     "trimmed right pinned sequence must start at 0 and end at 1");
            const float expectedCutY = evaluateAutomationCurveAt (
                origPinned.data(), static_cast<int> (origPinned.size()), 0.0f, pinnedSpec, 1.5f / 2.0f);
            require (std::abs (trimmedRight.back().normY - expectedCutY) < 0.0001f,
                     "endpoint did not take the curve value at trim cut point");

            // 8.2 Trim 1 Right / espansione oltre bounds: fissa endpoint originale e crea segmento costante
            const auto extendedRight = trimAutomationPointsWindow (
                origPinned, pinnedSpec, 0.0f, 0.0, 2.0, 0.0, 3.0);
            require (extendedRight.size() >= 3,
                     "extended right sequence must preserve original end and add new endpoint");
            const float origEndX = 2.0f / 3.0f;
            require (std::abs (extendedRight.back().normX - 1.0f) < 0.0001f
                     && std::abs (extendedRight.back().normY - 0.40f) < 0.0001f,
                     "new right endpoint must have same Y as original endpoint");
            const auto& origEndPt = extendedRight[extendedRight.size() - 2];
            require (std::abs (origEndPt.normX - origEndX) < 0.001f
                     && std::abs (origEndPt.normY - 0.40f) < 0.0001f,
                     "original endpoint must be fixed at its timeline coordinate with same Y");

            // 8.3 Trim 1 Left / riduzione range
            const auto trimmedLeft = trimAutomationPointsWindow (
                origPinned, pinnedSpec, 0.0f, 0.0, 2.0, 0.5, 2.0);
            const float expectedLeftCutY = evaluateAutomationCurveAt (
                origPinned.data(), static_cast<int> (origPinned.size()), 0.0f, pinnedSpec, 0.5f / 2.0f);
            require (std::abs (trimmedLeft.front().normX) < 0.0001f
                     && std::abs (trimmedLeft.front().normY - expectedLeftCutY) < 0.0001f,
                     "left endpoint did not take the curve value at left cut point");

            // 8.4 Trim 1 Left / espansione oltre bounds: fissa start originale e crea segmento costante
            const auto extendedLeft = trimAutomationPointsWindow (
                origPinned, pinnedSpec, 0.0f, 1.0, 2.0, 0.0, 3.0);
            require (extendedLeft.size() >= 3,
                     "extended left sequence must have new boundary point and fixed original start");
            require (std::abs (extendedLeft.front().normX) < 0.0001f
                     && std::abs (extendedLeft.front().normY - 0.20f) < 0.0001f,
                     "new left start point must have same Y as original start");
            const float expectedOrigStartX = (1.0f - 0.0f) / 3.0f;
            require (std::abs (extendedLeft[1].normX - expectedOrigStartX) < 0.001f
                     && std::abs (extendedLeft[1].normY - 0.20f) < 0.0001f,
                     "original start point must be fixed at its timeline coordinate with same Y");

            // 8.5 fadedEndpoints / riduzione range: mantiene spazio x e assume valore curva
            ayra::curves::CurveBoundarySpec fadeSpec;
            fadeSpec.mode = ayra::curves::CurveBoundaryMode::fadeEndpoints;
            fadeSpec.startValue = 0.0f;
            fadeSpec.endValue = 0.0f;
            std::vector<AutomationPoint> origFade {
                { 0.10f, 0.50f, 0.5f, false },
                { 0.50f, 0.85f, 0.5f, false },
                { 0.90f, 0.60f, 0.5f, false }
            };
            const auto trimmedFade = trimAutomationPointsWindow (
                origFade, fadeSpec, 0.0f, 0.0, 2.0, 0.0, 1.5);
            const float expectedFadeY = evaluateAutomationCurveAt (
                origFade.data(), static_cast<int> (origFade.size()), 0.0f, fadeSpec, 1.3f / 2.0f);
            require (std::abs (trimmedFade.back().normY - expectedFadeY) < 0.0001f,
                     "fade point must take curve value at new fade position");
            const float expectedFadeNormX = 1.0f - (0.20f / 1.5f);
            require (std::abs (trimmedFade.back().normX - expectedFadeNormX) < 0.001f,
                     "fade point must maintain constant temporal width from clip edge");

            // 8.6 fadedEndpoints / espansione oltre bounds: fissa fade point originale e crea segmento costante
            const auto extendedFade = trimAutomationPointsWindow (
                origFade, fadeSpec, 0.0f, 0.0, 2.0, 0.0, 3.0);
            require (extendedFade.size() >= 4,
                     "extended fade sequence must fix original fade point and add new fade point");
            const auto& newFadePt = extendedFade.back();
            const auto& origFadePt = extendedFade[extendedFade.size() - 2];
            require (std::abs (newFadePt.normY - 0.60f) < 0.0001f
                     && std::abs (origFadePt.normY - 0.60f) < 0.0001f,
                     "extended fade points must maintain constant Y equal to original fade value");
            const float expectedOrigFadeX = 1.80f / 3.0f;
            const float expectedNewFadeX = 1.0f - (0.20f / 3.0f);
            require (std::abs (origFadePt.normX - expectedOrigFadeX) < 0.001f
                     && std::abs (newFadePt.normX - expectedNewFadeX) < 0.001f,
                     "fade coordinates must correctly position original and new fade points");
        }

        // 9. Crossfade Engine Solvers & Curvature Conjugation Regressions
        {
            // 9.1 Solver Audio: Preservazione dei livelli nominali indipendenti delle clip
            AudioCrossfadeContext audioCtx;
            audioCtx.interaction = CrossfadeInteractionKind::levelChange;
            audioCtx.leftClipId = "clipA";
            audioCtx.rightClipId = "clipB";
            audioCtx.currentLeftGainDb = -6.0f;
            audioCtx.currentRightGainDb = 3.0f;
            audioCtx.isLeftClipModified = false;
            audioCtx.requestedGainDb = 6.0f; // Utente alza il volume solo su clipB

            audioCtx.rightFadeInControl = { 0.5, 0.5, -1.0 }; // Curva lineare richiesta
            const auto audioRes1 = defaultAudioCrossfadeSolver (audioCtx);
            require (std::abs (audioRes1.leftGainDb - (-6.0f)) < 0.001f,
                     "clipA gain must remain unchanged at -6 dB when clipB is edited");
            require (std::abs (audioRes1.rightGainDb - 6.0f) < 0.001f,
                     "clipB gain must be updated to requested +6 dB");
            require (std::abs (audioRes1.leftFadeOutControl.x - 0.5) < 0.001f
                     && std::abs (audioRes1.leftFadeOutControl.y - 0.5) < 0.001f,
                     "clipA fadeOut handle must be conjugate mirror (0.5, 0.5) of clipB linear fadeIn on levelChange");

            // Modifica su clipA
            audioCtx.isLeftClipModified = true;
            audioCtx.requestedGainDb = -12.0f;
            const auto audioRes2 = defaultAudioCrossfadeSolver (audioCtx);
            require (std::abs (audioRes2.leftGainDb - (-12.0f)) < 0.001f,
                     "clipA gain must be updated to requested -12 dB");
            require (std::abs (audioRes2.rightGainDb - 3.0f) < 0.001f,
                     "clipB gain must remain unchanged at +3 dB when clipA is edited");

            // 9.2 Solver Audio: Coniugazione speculare delle maniglie di curvatura (Drag & Reset da doppio click)
            audioCtx.interaction = CrossfadeInteractionKind::curvatureChange;
            audioCtx.isLeftClipModified = false;
            audioCtx.requestedControl = { 0.40, 0.75, -1.0 }; // Curvatura convessa su fadeIn di clipB
            const auto curveRes = defaultAudioCrossfadeSolver (audioCtx);
            require (std::abs (curveRes.rightFadeInControl.x - 0.40) < 0.001f
                     && std::abs (curveRes.rightFadeInControl.y - 0.75) < 0.001f,
                     "clipB fadeIn control must take requested coordinates");
            require (std::abs (curveRes.leftFadeOutControl.x - (1.0 - 0.40)) < 0.001f
                     && std::abs (curveRes.leftFadeOutControl.y - 0.75) < 0.001f,
                     "clipA fadeOut control must be conjugate mirror (1-x, y) for equal-power symmetry");

            // Test doppio click / reset a default (0.5, 0.5, -1.0)
            audioCtx.requestedControl = { 0.5, 0.5, -1.0 };
            const auto resetRes = defaultAudioCrossfadeSolver (audioCtx);
            require (std::abs (resetRes.rightFadeInControl.x - 0.5) < 0.001f
                     && std::abs (resetRes.rightFadeInControl.y - 0.5) < 0.001f,
                     "clipB fadeIn control must reset to linear center (0.5, 0.5)");
            require (std::abs (resetRes.leftFadeOutControl.x - 0.5) < 0.001f
                     && std::abs (resetRes.leftFadeOutControl.y - 0.5) < 0.001f,
                     "clipA fadeOut control must simultaneously reset to conjugate linear center (0.5, 0.5)");

            // 9.3 Iniezione solver custom: engine custom delegate
            bool customSolverInvoked = false;
            AudioCrossfadeSolver customSolver = [&] (const AudioCrossfadeContext& c) -> AudioCrossfadeResult
            {
                customSolverInvoked = true;
                AudioCrossfadeResult r;
                r.leftGainDb = c.currentLeftGainDb;
                r.rightGainDb = c.currentRightGainDb;
                return r;
            };
            const auto customRes = customSolver (audioCtx);
            require (customSolverInvoked, "custom engine crossfade solver must be successfully invokable");

            // 9.4 Solver Line e Image: verifica coniugazione complementare di default
            LineCrossfadeContext lineCtx;
            lineCtx.interaction = CrossfadeInteractionKind::levelChange;
            lineCtx.currentLeftValue = 0.2f;
            lineCtx.currentRightValue = 0.8f;
            lineCtx.isLeftClipModified = false;
            lineCtx.requestedValue = 0.95f;
            const auto lineRes = defaultLineCrossfadeSolver (lineCtx);
            require (std::abs (lineRes.leftValue - 0.2f) < 0.001f && std::abs (lineRes.rightValue - 0.95f) < 0.001f,
                     "line crossfade solver must preserve independent endpoint values");

            ImageCrossfadeContext imgCtx;
            imgCtx.interaction = CrossfadeInteractionKind::curvatureChange;
            imgCtx.isLeftClipModified = true;
            imgCtx.requestedControl = { 0.35, 0.65, -1.0 };
            const auto imgRes = defaultImageCrossfadeSolver (imgCtx);
            require (std::abs (imgRes.rightFadeInControl.x - (1.0 - 0.35)) < 0.001f
                     && std::abs (imgRes.rightFadeInControl.y - 0.65) < 0.001f,
                     "image crossfade solver must conjugate alpha curve control");
        }

        // 14. Verifica del Badge Pill Fluttuante Unificato (Single Source of Truth per Audio, MIDI, Image, Line)
        {
            const juce::Rectangle<float> clipBounds (10.0f, 20.0f, 200.0f, 100.0f);
            const juce::Point<float> targetCentre (50.0f, 60.0f);
            const juce::String badgeText = "Gain: +3.0 dB";

            const auto bounds = computeClipBadgeBounds (targetCentre, badgeText, clipBounds, 18.0f);
            require (bounds.getWidth() >= 48.0f, "badge width must satisfy minimum pill width");
            require (bounds.getHeight() == 15.0f, "badge height must be canonical 15px");
            require (bounds.getY() >= clipBounds.getY() + 18.0f, "badge must respect title bar minimum offset");
            require (bounds.getRight() <= clipBounds.getRight() - 4.0f, "badge must stay within right bounds");
            require (bounds.getX() >= clipBounds.getX() + 4.0f, "badge must stay within left bounds");

            // Verifica clamp sui bordi estremi
            const auto leftClamped = computeClipBadgeBounds ({ 0.0f, 22.0f }, badgeText, clipBounds, 18.0f);
            require (leftClamped.getX() == clipBounds.getX() + 4.0f, "badge must clamp to left boundary");

            const auto rightClamped = computeClipBadgeBounds ({ 300.0f, 22.0f }, badgeText, clipBounds, 18.0f);
            require (std::abs (rightClamped.getRight() - (clipBounds.getRight() - 4.0f)) < 0.001f,
                     "badge must clamp to right boundary");

            // Verifica rendering del badge pill: sfondo scuro ad alto contrasto
            juce::Image badgeImg (juce::Image::ARGB, 100, 30, true);
            juce::Graphics bg (badgeImg);
            drawClipBadge (bg, juce::Rectangle<float> (10.0f, 5.0f, 80.0f, 15.0f), badgeText, juce::Colours::cyan);

            // Verifica campionando un pixel all'interno del pill lontano dal testo centrato
            const auto bgPixel = badgeImg.getPixelAt (16, 12);
            require (bgPixel.getAlpha() > 0, "rendered badge pill interior must not be transparent");
            require (bgPixel.getRed() < 40 && bgPixel.getGreen() < 50 && bgPixel.getBlue() < 60,
                     "badge background must be the approved high-contrast dark slate");
        }

        // 15. Verifica Sub-Interval Detection e Smart Shrink Push (Compressione Proporzionale dei Fade)
        {
            // A) Sub-interval check (Layering / Mute Overlay senza 2-way crossfade)
            require (isClipSubInterval (2.0, 10.0, 4.0, 3.0),
                     "clip B (4..7) totally inside A (2..12) must be recognized as sub-interval");
            require (isClipSubInterval (4.0, 3.0, 2.0, 10.0),
                     "symmetric sub-interval check must detect inclusion in both orders");
            require (isClipSubInterval (2.0, 10.0, 2.0, 4.0),
                     "clip B aligned to start of A must be recognized as sub-interval");
            require (isClipSubInterval (2.0, 10.0, 8.0, 4.0),
                     "clip B aligned to end of A must be recognized as sub-interval");
            require (! isClipSubInterval (2.0, 8.0, 6.0, 6.0),
                     "partial overlap A (2..10) and B (6..12) is not a sub-interval");
            require (! isClipSubInterval (2.0, 4.0, 8.0, 4.0),
                     "disjoint clips must not be sub-intervals");

            // B) Smart Shrink Push Left (Overlap invades left clip opposite fade)
            std::vector<GainPoint> leftPoints = {
                { 0.20f, 0.0f, 0.5f, false }, // Fade In endpoint at normX = 0.20
                { 0.80f, 0.0f, 0.5f, false }  // Fade Out endpoint at normX = 0.80
            };
            const float nominalLeftFadeIn = 0.20f;

            // Scenario: Overlap normale (Fade Out a 0.50, non invade il Fade In)
            applySmartShrinkPushLeft (leftPoints, 0.50f, nominalLeftFadeIn);
            require (std::abs (leftPoints[1].normX - 0.50f) < 0.001f, "Fade Out must track target");
            require (std::abs (leftPoints[0].normX - 0.20f) < 0.001f, "Fade In must remain nominal when uninvaded");

            // Scenario: Overlap estremo invade il Fade In (target Fade Out spinge a 0.10)
            applySmartShrinkPushLeft (leftPoints, 0.10f, nominalLeftFadeIn);
            require (std::abs (leftPoints[1].normX - 0.10f) < 0.001f, "Fade Out must track target into fade-in zone");
            require (leftPoints[0].normX < leftPoints[1].normX, "Fade In must be pushed strictly before Fade Out");
            require (leftPoints[0].normX >= 0.0f, "Fade In must not be negative");
            require (std::abs (leftPoints[0].normX - 0.09f) < 0.001f, "Fade In must compress proportionally with minSeparation");

            // Scenario: Overlap recede (target Fade Out torna a 0.70) -> Self-Healing!
            applySmartShrinkPushLeft (leftPoints, 0.70f, nominalLeftFadeIn);
            require (std::abs (leftPoints[1].normX - 0.70f) < 0.001f, "Fade Out tracks receded target");
            require (std::abs (leftPoints[0].normX - 0.20f) < 0.001f,
                     "Fade In must self-heal back to nominal value when overlap recedes");

            // C) Smart Shrink Push Right (Overlap invades right clip opposite fade)
            std::vector<GainPoint> rightPoints = {
                { 0.20f, 0.0f, 0.5f, false }, // Fade In endpoint at normX = 0.20
                { 0.80f, 0.0f, 0.5f, false }  // Fade Out endpoint at normX = 0.80
            };
            const float nominalRightFadeOut = 0.80f;

            // Scenario: Overlap normale (Fade In a 0.40, non invade il Fade Out)
            applySmartShrinkPushRight (rightPoints, 0.40f, nominalRightFadeOut);
            require (std::abs (rightPoints[0].normX - 0.40f) < 0.001f, "Fade In must track target");
            require (std::abs (rightPoints[1].normX - 0.80f) < 0.001f, "Fade Out must remain nominal when uninvaded");

            // Scenario: Overlap estremo invade il Fade Out (target Fade In spinge a 0.90)
            applySmartShrinkPushRight (rightPoints, 0.90f, nominalRightFadeOut);
            require (std::abs (rightPoints[0].normX - 0.90f) < 0.001f, "Fade In must track target into fade-out zone");
            require (rightPoints[1].normX > rightPoints[0].normX, "Fade Out must be pushed strictly after Fade In");
            require (rightPoints[1].normX <= 1.0f, "Fade Out must not exceed 1.0");
            require (std::abs (rightPoints[1].normX - 0.91f) < 0.001f, "Fade Out must compress proportionally with minSeparation");

            // Scenario: Overlap recede (target Fade In torna a 0.30) -> Self-Healing!
            applySmartShrinkPushRight (rightPoints, 0.30f, nominalRightFadeOut);
            require (std::abs (rightPoints[0].normX - 0.30f) < 0.001f, "Fade In tracks receded target");
            require (std::abs (rightPoints[1].normX - 0.80f) < 0.001f,
                     "Fade Out must self-heal back to nominal value when overlap recedes");

            // D) Multi-point strict monotonicity check under extreme push
            std::vector<AutomationPoint> multiPoints = {
                { 0.10f, 0.2f }, { 0.30f, 0.4f }, { 0.50f, 0.6f }, { 0.70f, 0.8f }, { 0.90f, 1.0f }
            };
            applySmartShrinkPushLeft (multiPoints, 0.03f, 0.10f, 0.01f);
            for (size_t p = 1; p < multiPoints.size(); ++p)
            {
                require (multiPoints[p].normX > multiPoints[p - 1].normX,
                         "multi-point sequence must maintain strict monotonicity under extreme push left");
            }

            applySmartShrinkPushRight (multiPoints, 0.97f, 0.90f, 0.01f);
            for (size_t p = 1; p < multiPoints.size(); ++p)
            {
                require (multiPoints[p].normX > multiPoints[p - 1].normX,
                         "multi-point sequence must maintain strict monotonicity under extreme push right");
            }
        }

        component.setLookAndFeel(nullptr);
        std::cout << "PASS: " << checks << " clip component assertions\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
