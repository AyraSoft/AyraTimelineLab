> Aggiornamento 20 ottobre: [centratura e controlli raggruppati](LANE_VISUAL_POLISH.md)
> prevale sul posizionamento top-aligned del riferimento HTML originale, che
> rimane conservato senza alterazioni come riferimento storico.

# Frosted lens — approved HTML and passive clip-aware identity

This revision supersedes the **visual and rename** policy of
`TIMELINE_IDENTITY_AND_VISUAL_SOURCES.md`. The direct Multiverse visual-source
wiring remains unchanged. The reference is the user's unmodified
[`fixtures/frosted-lens.html`](../fixtures/frosted-lens.html).

## Composition

Follow the supplied HTML's **two lines**, not the previous three-line proposal:
uppercase Performer and type/dot in the metadata row; Track/Tensor below.
The performer comes from the actual owner ancestry. Song-owned tracks have no
invented Performer. Global lanes have a compact one-line label.

CSS content width is 120–205 px plus 10/14 px padding: the complete lens is
144–229 logical pixels, bounded further by the available canvas. A low lane is
never enlarged for its name. Long names use ellipsis, never font compression.
The same lens can be anchored left or right; right anchoring mirrors the fade,
not the reading order. The metadata may collapse on a very short lane.

The native painter uses the HTML system/Inter family preference, 12.5 px track,
9.5 px metadata, tracked uppercase Performer, a 5 px semantic type dot, rounded
7 px glass and the 105-degree gradient with the supplied .72/.32/0 stops.
JUCE **point height** specifies the font em-square in logical pixels; plain
`withHeight()` would shrink the letters relative to CSS `font-size`.
Static font families use their available Medium/SemiBold weights, not a claim of
pixel-identical variable weights 560/650. No font files are distributed.
The Light rendition is an explicit theme adaptation; the supplied reference is
Dark. Audio/MIDI/etc. keep their canonical editable type colour, not a new fixed
cyan for every lane. Other widgets retain the selected Multiverse source profile.

## Interaction: no editor and no forwarding proxy

All three labels are **display-only**. The previous `NameInput` components and
`TextEditor` have been removed, so the actual JUCE mouse target, cursor, capture,
click/double-click, context menu, drag and wheel remain with the underlying
canvas. No rename command is emitted by the identity. Domain rename APIs are not
deleted; a future editor can use them without making this overlay interactive.

A row observes pointer events; it does not consume or redispatch them:

1. Pointer outside lens: identity visible.
2. Pointer inside lens over empty canvas: identity visible.
3. Pointer inside lens **over a real clip hit**: hide the whole lens.
4. Once hidden, remain hidden until the pointer leaves the same footprint,
   including when moving to a gap within it or moving/deleting the clip.
5. Leaving that footprint restores the lens. The footprint does not shrink or
   disappear when its artwork is hidden, preventing enter/exit oscillation.

`LaneCanvas::containsClipAt` receives canvas-local coordinates. The Audio, MIDI,
Image and Line clip adapters call their existing `hitTestZones`: paint and clip
input geometry stay authoritative, including editing affordances. The mere
presence of a lane, a time event or a rectangular canvas is NOT a clip hit.
An adapter without actual clips (global events or continuous/local point editors)
or without this callback leaves the passive name visible. Widgets such as the
resize zone and side controls do not count as the canvas.

Scroll/zoom/rebinding also re-evaluate a stationary cursor. Hiding a row resets
its transient hover state. The document, selection and musical coordinates do
not change when a name hides. No hover timer or continuous animation is added.

## Frosting and invalidation

The painter uses the existing Melatonin blur provider on the **actual canvas
pixels** behind the lens. It is not a faux texture, waveform copy or a second
renderer. Only a lens-sized region plus 15 px sampling padding is captured,
never the row itself, so there is no recursive overlay capture. The rendered
material is supersampled; the text/rim/dot are vector-rendered at the current DPI.

Each resident row owns its small underlay/material cache. Stationary and meter-only
frames reuse it. Layout, content revision, zoom/scroll, theme, pointer edits and
source invalidations refresh it. A hidden lens does not recapture its underlay.
Asynchronous media providers must call `notifyCanvasContentChanged(laneId)` when
pixels change independently of the host's `CanvasFrame` revision. This is an
explicit presentation invalidation port, not a polling dependency.

## Files and verification

- `TimelineIdentity.h`: content, compact layout and hover latch.
- `TimelinePanel.h`: ancestor binding, passive pointer observation, cache lifecycle.
- `demo/TimelineIdentityArtwork.h`: HTML-derived host recipe, canonical blur provider.
- `demo/TimelinePanelDemo.cpp`: artwork and canvas adapter bindings.
- Four clip canvases: side-effect-free query exposing existing clip hit geometry.
- `tests/identity/IdentityTests.cpp`: sizing, real rendering, blur and hover state.
- `tests/NativePanelTests.cpp`: actual JUCE peer dispatch, no rename interception,
  cache reuse, cursor/clip changes, existing resizing and 10,000-lane residency.

See `verification/frosted-lens/STATUS.md` for executions on this exact revision;
prior screenshots/test totals are not reused as evidence for this change.
