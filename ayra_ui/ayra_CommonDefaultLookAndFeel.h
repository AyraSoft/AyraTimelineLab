/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

  ==============================================================================
*/

#pragma once

/**
 * @file ayra_CommonDefaultLookAndFeel.h
 * @brief Implementazione di DEFAULT dell'API di disegno bespoke di ayra_common.
 *
 * Fornisce un disegno sensato per ogni ricetta di `CommonLookAndFeelMethods`,
 * fedele al prototipo Multiverse (`_AppWidgets.cpp`). NON eredita da
 * `juce::LookAndFeel_V4` — solo da `CommonLookAndFeelMethods` (pattern
 * multi-modulo, vedi quel file).
 *
 * # USO
 *
 * **1. Come default standalone** — i widget fanno fallback su un'istanza
 * statica quando il LookAndFeel corrente non implementa l'interfaccia:
 * @code
 *   auto& fallback = ayra::CommonDefaultLookAndFeel::getDefaultInstance();
 * @endcode
 *
 * **2. Come base per la LAF d'app** — l'app eredita `juce::LookAndFeel_V4` +
 * questa classe e override-a solo le ricette che vuole personalizzare:
 * @code
 *   class MyAppLAF : public juce::LookAndFeel_V4,
 *                    public ayra::CommonDefaultLookAndFeel
 *   {
 *       void drawNodeCard (...) override { ... }   // resto = default Tensor
 *   };
 * @endcode
 *
 * L'implementazione e' divisa in partial `.cpp` per coesione e leggibilita':
 *  - `..._Chrome.cpp`    -> drawApp* + colori-ponte.
 *  - `..._NodeGraph.cpp` -> drawNodeCard / drawNodePin / drawConnectionCable.
 *  - `..._Mixer.cpp`     -> channel strip, fader, VU, macro knob, colour cap.
 *  - `..._Inspector.cpp` -> CPU arc, snapshot launcher, node list item, curve,
 *                           clip cell, transport button.
 *  - `..._Form.cpp`      -> switch, badge, search field, param slider, plugin card.
 *
 * @note `<JuceHeader.h>`, `ayra_Tokens.h`, `ayra_ChromePrimitives.h`,
 *       `ayra_CommonLookAndFeelMethods.h` sono inclusi dal file root del modulo.
 */

namespace ayra
{

/** @brief Implementazione di default di tutte le ricette di disegno di Tensor.

    Meyer's singleton via `getDefaultInstance()` per il fallback dei widget.

    @see CommonLookAndFeelMethods
*/
class CommonDefaultLookAndFeel : public CommonLookAndFeelMethods
{
public:
    CommonDefaultLookAndFeel() = default;
    ~CommonDefaultLookAndFeel() override = default;

    /** Istanza condivisa di default (Meyer's singleton). Message-thread only. */
    static CommonDefaultLookAndFeel& getDefaultInstance();

    // NB: l'accent PER-ISTANZA (track colour) — setInstanceAccent/clearInstanceAccent/
    // accentOr — vive sulla base `CommonLookAndFeelMethods`, cosi' e' raggiungibile
    // anche dai painter nativi via `getCommonLAF(comp).accentOr(...)`. Qui e' ereditato.

    //==========================================================================
    // Chrome (impl: ayra_CommonDefaultLookAndFeel_Chrome.cpp)
    //==========================================================================
    void drawAppBackground    (juce::Graphics&, juce::Rectangle<int> bounds) override;
    void drawAppPanel         (juce::Graphics&, juce::Rectangle<float> bounds, bool raised, float corner) override;
    void drawAppGlassStrip    (juce::Graphics&, juce::Rectangle<float>, bool hairlineBottom) override;
    void drawAppSectionHeader (juce::Graphics&, juce::Rectangle<int>, const juce::String& text) override;
    void drawAppIconChip      (juce::Graphics&, juce::Rectangle<float>, bool active) override;
    void drawAppLogoBadge     (juce::Graphics&, juce::Rectangle<float>) override;
    void drawAppWindowHeader  (juce::Graphics&, juce::Rectangle<float>, const unsigned char*, size_t, const juce::String&) override;
    void drawSidebarNavItem   (juce::Graphics&, juce::Rectangle<float>, const unsigned char*, size_t, const juce::String&, bool, bool) override;
    void drawSplitPanelBackground (juce::Graphics&, juce::Rectangle<int>, int, int) override;

    //==========================================================================
    // Node graph (impl: ayra_CommonDefaultLookAndFeel_NodeGraph.cpp)
    //==========================================================================
    void drawNodeCard        (juce::Graphics&, const NodeCardVisual&) override;
    void drawNodePin         (juce::Graphics&, const NodePinVisual&) override;
    void drawConnectionCable (juce::Graphics&, juce::Point<float> from, juce::Point<float> to,
                              juce::Colour tint, bool dashed, bool verticalFlow) override;

    //==========================================================================
    // Mixer (impl: ayra_CommonDefaultLookAndFeel_Mixer.cpp)
    //==========================================================================
    void drawChannelStripBackground (juce::Graphics&, juce::Rectangle<float>) override;
    void drawColourCap              (juce::Graphics&, juce::Rectangle<float>, juce::Colour tint, const juce::String& name) override;
    void drawFader                  (juce::Graphics&, juce::Rectangle<float> area, float pos01, juce::Colour tint) override;
    void drawVMeter                 (juce::Graphics&, juce::Rectangle<float>, float level) override;
    void drawMacroKnob              (juce::Graphics&, juce::Rectangle<float>, float value, bool bipolar) override;

    //==========================================================================
    // Inspector / snapshot / CPU / session / transport
    //  (impl: ayra_CommonDefaultLookAndFeel_Inspector.cpp)
    //==========================================================================
    void drawClipCell            (juce::Graphics&, juce::Rectangle<float>, juce::Colour tint, ClipState, const juce::String& name) override;
    void drawTransportButton     (juce::Graphics&, juce::Rectangle<float>, TransportKind, bool active) override;
    void drawCpuArc              (juce::Graphics&, juce::Rectangle<float>, float level01, const juce::String& caption) override;
    void drawSnapshotLaunchButton (juce::Graphics&, juce::Rectangle<float>, juce::Colour tint, ClipState, float ringProgress) override;
    void drawNodeListItem        (juce::Graphics&, juce::Rectangle<float>, const juce::String& name, const juce::String& uid, juce::Colour tint, bool selected, bool hover) override;
    void drawCurveEditor         (juce::Graphics&, juce::Rectangle<float>, int mode, juce::Colour tint) override;

    //==========================================================================
    // Timeline lane (impl: ayra_CommonDefaultLookAndFeel_Timeline.cpp)
    //==========================================================================
    void drawTimelineLane (juce::Graphics&, const TimelineLaneVisual&) override;
    void drawTimelineAutomationCurveUnderlay (juce::Graphics&, const juce::Path&,
                                              juce::Rectangle<float>,
                                              TimelineCurveUnderlayStyle) override;
    void drawTimelineAutomationCurveStroke (juce::Graphics&, const juce::Path&,
                                            juce::Colour) override;
    void drawTimelineKeyframeLaneBackground (juce::Graphics&,
                                             juce::Rectangle<float>) override;
    void drawTimelineKeyframeEvent (juce::Graphics&, juce::Rectangle<float>,
                                    juce::Rectangle<float>, const juce::String&,
                                    juce::Colour, bool, bool, bool) override;

    //==========================================================================
    // Form / browser (impl: ayra_CommonDefaultLookAndFeel_Form.cpp)
    //==========================================================================
    void drawSwitch      (juce::Graphics&, juce::Rectangle<float>, bool on,
                          juce::Colour accentColour, juce::Colour offColour) override;
    void drawBadge       (juce::Graphics&, juce::Rectangle<float>, const juce::String& text, juce::Colour tint) override;
    void drawSearchField (juce::Graphics&, juce::Rectangle<float>, const juce::String& placeholder) override;
    void drawParamSlider (juce::Graphics&, juce::Rectangle<float>, float value01, const juce::String& name, const juce::String& valueText, juce::Colour tint) override;
    void drawPluginCard  (juce::Graphics&, juce::Rectangle<float>, const juce::String& name, const juce::String& vendor, const juce::String& fmt, StreamType kind, bool selected) override;

private:
    /** Entry della cache LRU del fondo. Una LookAndFeel applicativa e' condivisa
        da piu' top-level: una sola Image causerebbe alloc/free a ogni alternanza
        fra finestre di dimensioni diverse. */
    struct AppBackgroundCacheEntry
    {
        juce::Image image;
        std::uint64_t lastUse = 0;
        int width = 0;
        int height = 0;
        bool dark = false;
    };

    static constexpr std::size_t appBackgroundCacheCapacity = 4;
    std::array<AppBackgroundCacheEntry, appBackgroundCacheCapacity>
        appBackgroundCaches;
    std::uint64_t appBackgroundUseCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CommonDefaultLookAndFeel)
};

//==============================================================================
/** @brief Helper condiviso da TUTTI i widget di ayra_common: restituisce l'API di
    disegno bespoke per il componente `c`.

    Se la LAF corrente implementa `CommonLookAndFeelMethods`, la restituisce;
    altrimenti cade sul singleton `CommonDefaultLookAndFeel::getDefaultInstance()`.
    Questo garantisce che ogni widget funzioni out-of-the-box anche senza alcun
    setup di LookAndFeel (harness di test, canvas headless-visual, ecc.).

    @code
      void MyWidget::paint (juce::Graphics& g)
      {
          getCommonLAF (*this).drawAppPanel (g, getLocalBounds().toFloat(), true, tokens::radius::card);
      }
    @endcode */
[[nodiscard]] inline CommonLookAndFeelMethods& getCommonLAF (juce::Component& c)
{
    if (auto* l = dynamic_cast<CommonLookAndFeelMethods*> (&c.getLookAndFeel()))
        return *l;
    return CommonDefaultLookAndFeel::getDefaultInstance();
}

} // namespace ayra
