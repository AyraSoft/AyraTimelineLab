#pragma once
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_gui_extra/juce_gui_extra.h>

#ifndef AYRA_COMMON_WITH_MULTIVERSE
 #define AYRA_COMMON_WITH_MULTIVERSE 0
#endif

// Melatonin blur
#include "melatonin_blur/melatonin_blur.h"

// Tokens (Colours, Geometry, Typography, Tokens)
#include "tokens/ayra_Tokens_Colours.h"
#include "tokens/ayra_Tokens_Geometry.h"
#include "tokens/ayra_Tokens_Typography.h"
#include "tokens/ayra_Tokens.h"

// Glyphs
#include "glyphs/ayra_Glyph.h"
#include "glyphs/ayra_TransportGlyphs.h"
#include "glyphs/ayra_StateGlyphs.h"
#include "glyphs/ayra_FileGlyphs.h"
#include "glyphs/ayra_LayoutGlyphs.h"
#include "glyphs/ayra_LogoGlyphs.h"
#include "glyphs/ayra_ControlGlyphs.h"
#include "glyphs/ayra_SystemGlyphs.h"
#include "glyphs/ayra_Glyphs.h"
#include "glyphs/ayra_DeviceGlyphs.h"
#include "glyphs/ayra_SnapshotGlyphs.h"
#include "glyphs/ayra_CommandGlyphs.h"
#include "glyphs/ayra_ActionGlyphs.h"
#include "glyphs/ayra_GlyphHandles.h"

// Legacy Node Palette & Fonts
#include "legacy/ayra_NodePalette.h"
#include "legacy/ayra_NodeFont.h"
#include "legacy/ayra_NodeDrawings.h"
#include "legacy/ayra_NodeShapes.h"

// Identifiers
#include "identifiers/ayra_SharedIdentifiers.h"

// Chrome Primitives
#include "ayra_NodeButtons.h"
#include "ayra_ChromePrimitives.h"

// Canonical Strip Fader Geometry for Mixer LookAndFeel
struct StripFaderGeometry
{
    juce::Rectangle<float> bounds;
    static constexpr float endInset = 4.0f;
    static constexpr float thumbLength = 18.0f;
    static constexpr float thumbWidth = 32.0f;

    [[nodiscard]] float travel() const noexcept
    {
        return juce::jmax (0.0f, bounds.getHeight() - 2.0f * endInset - thumbLength);
    }
    [[nodiscard]] float centre (float position) const noexcept
    {
        return bounds.getY() + endInset + thumbLength * 0.5f + (1.0f - position) * travel();
    }
};

// Timeline Visual models & geometries
#include "ayra_TimelineVisual.h"
#include "ayra_MarqueeSelector.h"

// LookAndFeel Methods & Default LookAndFeel
#include "ayra_CommonLookAndFeelMethods.h"
#include "ayra_CommonDefaultLookAndFeel.h"

// Controls / Widgets (decoupled standalone controls)
#include "controls/shape_icon_button/ayra_ShapeIconButton.h"
#include "controls/add_button/ayra_AddButton.h"
#include "controls/tab_strip/ayra_TabStrip.h"
#include "controls/drag_value_field/ayra_DragValueField.h"
