#pragma once
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include "ayra_VisualSource.h"
#include <juce_gui_extra/juce_gui_extra.h>

#ifndef AYRA_COMMON_WITH_MULTIVERSE
 #define AYRA_COMMON_WITH_MULTIVERSE 0
#endif

// Melatonin blur
#include "melatonin_blur/melatonin_blur.h"

// Tokens (Colours, Geometry, Typography, Tokens)
#include AYRA_LAB_LAF_FILE(tokens/ayra_Tokens_Colours.h)
#include AYRA_LAB_LAF_FILE(tokens/ayra_Tokens_Geometry.h)
#include AYRA_LAB_LAF_FILE(tokens/ayra_Tokens_Typography.h)
#include AYRA_LAB_LAF_FILE(tokens/ayra_Tokens.h)

// Glyphs
#include AYRA_LAB_LAF_FILE(glyphs/ayra_Glyph.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_TransportGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_StateGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_FileGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_LayoutGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_LogoGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_ControlGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_SystemGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_Glyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_DeviceGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_SnapshotGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_CommandGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_ActionGlyphs.h)
#include AYRA_LAB_LAF_FILE(glyphs/ayra_GlyphHandles.h)

// Legacy Node Palette & Fonts
#include "legacy/ayra_NodePalette.h"
#include "legacy/ayra_NodeFont.h"
#include "legacy/ayra_NodeDrawings.h"
#include "legacy/ayra_NodeShapes.h"

// Identifiers
#include "identifiers/ayra_SharedIdentifiers.h"

// Chrome Primitives
#include AYRA_LAB_LAF_FILE(ayra_NodeButtons.h)
#include AYRA_LAB_LAF_FILE(ayra_ChromePrimitives.h)

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
#include AYRA_LAB_CONTROL_FILE(shape_icon_button/ayra_ShapeIconButton.h)
#include AYRA_LAB_CONTROL_FILE(add_button/ayra_AddButton.h)
#include AYRA_LAB_CONTROL_FILE(tab_strip/ayra_TabStrip.h)
#include AYRA_LAB_CONTROL_FILE(drag_value_field/ayra_DragValueField.h)
