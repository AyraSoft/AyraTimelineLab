# Compact media-lane previews and mirrored frosted identity

Implementation follows the 21 October 2026 request. All measurements are logical JUCE pixels.

## Viewport height presets

- Audio, Image/Video and Line/clip automation: **24 <-> 400**.
- MIDI: **24 -> 256 -> 400 -> 24**.
- Continuous lines: manually resizable down to 24; double-click **accent** cycles 24/400, preserving double-click on curves for their musical editing action.
- Point/event/span lanes keep their automatic content-derived height and cannot be manually resized.

The portable contract lives in `modules/ayra_timeline_panel/TimelineHeightPresets.h` and is shared by the lane presentation controller and existing clip canvases. The new display height is a `ViewState` change: no media, document or timeline time-coordinate mutation. Double click on the accent and on an unclaimed area of a clip cycles the view; specialist double-click actions win (note insertion, warp/slip/stretch reset, fade-point reset, etc.). Accents display the dragging-hand cursor. The six legacy side controls remain hidden by default but their APIs are retained.

The 24px level deliberately prioritises a compact display preview. Existing audio/MIDI/image clip visual editors still own all full-height editing gestures and hit zones; the ultra-compact level does not rescale musical time or force additional channel-strip/header pixels. There is no promise that fully detailed editors can be operated in 24px: expand before detailed editing.

## Frosted identity

The gradient core is darker (Dark: alpha 0.96; Light: alpha 0.91, supported by a stronger mid-gradient stop). Card maximum expands from 229 to 255 pixels logical, allowing more of the Performer to survive truncation without reserving any canvas space. The fade is mirrored when the name side is switched, in both tinted material and the masked real-blur underlay. Right-docked: transparent LEFT, opaque RIGHT. Left-docked: opaque LEFT, transparent RIGHT.

For non-editable service-lane labels, hover over a real edit target immediately suppresses the card rather than protecting a non-existent name editor. Track names retain their existing guarded inline-rename protection and 500ms hover policy. Navigation chevrons remain in the card.

## Distinguish fixed fade boundaries

The actual boundary-fade segments in Audio and Image draw a short neutral dashed overstroke. The editable intermediate curve retains the canonical painter and its original hit-test. Fade endpoint double clicks remain reserved; this is visual feedback only.

## Verification scope

A pure C++20 preset test (`tests/HeightPresetTests.cpp`) and the existing source boundary audit pass in the isolated environment. Native painter compilation is attempted separately; the full AyraTimelineLab application cannot be claimed compiled in the absence of the real external media providers and a complete JUCE/Multiverse checkout. This delivery does not change those dependencies or copy private provider sources.
