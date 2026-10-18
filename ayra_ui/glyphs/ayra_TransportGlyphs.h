/*
  ==============================================================================

   This file is part of the Ayra ecosystem.
   Copyright (c) Ayra Productions.

   ayra_common / look_and_feel / glyphs / Transport
   ------------------------------------------------------------------
   Glyph categoria "transport/timing". Il clock conserva il path-data legacy;
   i controlli transport condivisi usano path SVG normalizzati. Categorizzati per FORMA
   visiva (non per il concetto Ayra d'origine). Si disegnano via
   ayra::glyphs::drawGlyph(...) (facade ayra_Glyphs.h).

   @note Nessun #include: gli array sono puri byte; JUCE arriva transitivamente
         dal root del modulo prima del facade.

  ==============================================================================
*/

#pragma once

namespace ayra::glyphs
{

// Controlli transport canonici: UNA geometria condivisa da Tensor e Multiverse.
inline constexpr const char* playSvg =
    "M12 0 L100 50 L12 100 Z";

inline constexpr const char* stopSvg =
    "M12 0 H88 Q100 0 100 12 V88 Q100 100 88 100 H12 Q0 100 0 88 V12 Q0 0 12 0 Z";

inline constexpr const char* rewindSvg =
    "M0 0 H16 V100 H0 Z M100 0 V100 L24 50 Z";

inline constexpr const char* metronomeSvg =
    "M40 22 H60 L80 88 H20 Z M48.5 8 H51.5 L66 62 H63 Z";

/// Counter vocale: balloon con tre impulsi, distinto dal metronomo meccanico.
inline constexpr const char* counterSvg =
    "M12 12 H88 Q96 12 96 20 V66 Q96 74 88 74 H50 L32 90 V74 H12 Q4 74 4 66 V20 Q4 12 12 12 Z "
    "M24 31 H32 V55 H24 Z M46 25 H54 V55 H46 Z M68 35 H76 V55 H68 Z";

// clockPath -- orologio (timing/sync)
inline constexpr unsigned char clockPath[] = {
    110,109,0,0,64,65,0,0,48,64,98,26,134,220,64,0,0,48,64,0,0,48,64,26,
    134,220,64,0,0,48,64,0,0,64,65,98,0,0,48,64,106,222,136,65,26,134,220,64,
    0,0,170,65,0,0,64,65,0,0,170,65,98,106,222,136,65,0,0,170,65,0,0,170,
    65,106,222,136,65,0,0,170,65,0,0,64,65,98,0,0,170,65,26,134,220,64,106,222,
    136,65,0,0,48,64,0,0,64,65,0,0,48,64,99,109,0,0,160,63,0,0,64,65,
    98,0,0,160,63,155,3,194,64,155,3,194,64,0,0,160,63,0,0,64,65,0,0,160,
    63,98,46,127,143,65,0,0,160,63,0,0,182,65,155,3,194,64,0,0,182,65,0,0,
    64,65,98,0,0,182,65,46,127,143,65,46,127,143,65,0,0,182,65,0,0,64,65,0,
    0,182,65,98,155,3,194,64,0,0,182,65,0,0,160,63,46,127,143,65,0,0,160,63,
    0,0,64,65,99,109,0,0,64,65,0,0,232,64,98,144,160,70,65,0,0,232,64,0,
    0,76,65,203,190,242,64,0,0,76,65,0,0,0,65,108,0,0,76,65,95,7,59,65,
    108,28,124,112,65,228,131,95,65,98,212,43,117,65,156,51,100,65,212,43,117,65,100,204,
    107,65,28,124,112,65,28,124,112,65,98,100,204,107,65,212,43,117,65,156,51,100,65,212,
    43,117,65,228,131,95,65,28,124,112,65,108,228,131,55,65,28,124,72,65,98,150,67,53,
    65,54,60,70,65,0,0,52,65,178,46,67,65,0,0,52,65,0,0,64,65,108,0,0,
    52,65,0,0,0,65,98,0,0,52,65,203,190,242,64,112,95,57,65,0,0,232,64,0,
    0,64,65,0,0,232,64,99,101,0,0
};

} // namespace ayra::glyphs
