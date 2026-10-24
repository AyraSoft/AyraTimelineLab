/** @file ayra_clips.h
    Punto di ingresso unico dei widget clip del Lab.
    Questo layer rappresenta esclusivamente la proiezione visuale delle clip.
    Il documento musicale, l'Undo e le sorgenti Audio/MIDI restano di proprieta'
    di `ayra_timeline`; un consumer fornisce al widget una view gia' risolta. */

#pragma once

#include "ayra_ui.h"
#include "ayra_CurveProvider.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "component/ayra_CutRailGroupGesture.h"
#include "component/ayra_ClipSplitReconciliation.h"
#include "visual/ayra_ClipVisual.h"
#include "visual/ayra_ImageClipVisual.h"
#include "component/ayra_ClipComponent.h"
#include "audio/ayra_AudioClipComponent.h"
#include "midi/ayra_MidiClipComponent.h"
#include "line/ayra_LineClipComponent.h"
#include "keyframe/ayra_KeyframeLaneComponent.h"
#include "continous/ayra_ContinuousLineComponent.h"
#include "image/ayra_ImageClipComponent.h"
#include "look_and_feel/ayra_ClipLookAndFeelMethods.h"
#include "look_and_feel/ayra_ClipDefaultLookAndFeel.h"
