> Current identity override: compact HTML frosted lens, no editable text,
> no input forwarding proxy. Hide only on a real clip hit and retain that state
> until pointer exit. See `docs/FROSTED_LENS.md` for the current contract.

# Ayra Timeline Panel — no-header presentation

`TimelineModel.h` is C++20 without JUCE. The native `TimelinePanel.h` consumes it
and official JUCE. Musical documents, transport, media, DSP and Undo admission
remain outside the module. The default demo uses **free lanes**; the existing
structured projection is retained for comparison, not redesigned in this iteration.

## One provider for geometry and residency

The module includes `<ayra_timeline/ayra_timeline_presentation.h>` from an external
checkout at `caa00902` or compatible. No fallback implementation is copied here.

- `TimelineRowIndex` supplies incremental height/hide queries. The native path
  visits its window directly; legacy `rows()` top offsets are refreshed lazily.
- `TimelineRowResidency` limits mounted/idle UI resources and preserves stable IDs.
  Active gestures and text editors obtain retention through `hasInteraction`.
- `TimelineTimeWindow` supplies viewport-relative display seconds. Horizontal
  scrolling never creates a component as wide as the whole Song.
- `layoutTimelineLaneChrome` supplies the same bounds for side controls and hit tests.
- `TimelineIntervalIndex` is reused by the demo's derived `PanelClipWindow` to
  project visible clips, retaining dependency partners and a full gesture scope.

One shared time origin is determined from the configured side-control requirements,
not the currently visible rows. Vertical scroll therefore cannot shift musical X.
Per-lane content rectangles clip the same coordinate system; hiding chrome returns
its space without independently fitting each lane. Names/gradients overlay content
and do not subtract another region. `seamHeight` is zero.

## View state and commands

The Controller owns layout mode/order/groups, selected and hidden lane IDs, height
previews, chrome preferences and bounded view Undo. Reordering never reparents
musical data. Structured layout groups contain whole owners; free groups contain
lane IDs; switching modes never widens a partial group silently.

Snapshot owner/lane/event IDs remain external identities. Commands carry session,
revision, owner and the relevant lane/event scope. Rename changes a lane label;
M/S/Monitoring/Open address its owner. Song Main has no Solo. Previous/next address
the lane, not every lane of that owner. The host admits commands atomically.

`HeightPolicy::compact` rejects manual resize; `automatic` accepts a content height;
`manual` supports preview, Escape cancel and one view Undo on release. A supplied
`LaneCanvas::preferredHeight(width)` reports a pure measurement. The panel coalesces
that result outside recursive layout. Measurement must not edit the document.

## LaneCanvas boundary

The host supplies a component plus `bind`, `saveState`, `hasInteraction`, optional
`eventTimes` and optional `preferredHeight`. Source/editor state belongs to a host
cache or canonical owner, not an evictable row. The demo captures the existing typed
canvas state, keyed by both lane ID and visual kind; changing kind remounts only that
row. Clip IDs outside the viewport remain known, while actual deletion prunes state.

`NameInput` forwards the first pointer gesture to the real underlying target with
converted coordinates. The second click on the text is swallowed and opens the
inline editor. The passive gradient never intercepts input. Interactive side buttons
and accents are separate children. The resize glyph is paint only; its bottom hit
region is a distinct child and never adds height.

## Artwork and optional meters

`TimelinePanelLookAndFeel` is a borrowed interface. The demo delegates recessed wells
to `ayra::drawFlagToolbarWell`, accents to `ayra::drawAccentSpine`, icons to existing
`ShapeIconButton`/Ayra glyphs, meters to the existing `drawVMeter`. It does not copy
Multiverse's private widgets or invent a second meter painter. The new name gradient
is composition-specific and uses the injected palette, not a duplicate global widget.

Type colours and owner colours remain separate. Neutral global lanes use theme
roles; the product adapter must supply its actual settings resolver. Meter count and
readback are host capabilities, not exclusions based on visual kind. One panel timer
updates visible meter rectangles only. Missing data is not treated as a live signal.
The demo explicitly supplies synthetic readback, disabled by default.

## Scope of verification

The actual native panel is tested with instrumented canvases for hit regions,
name passthrough/rename, compact/manual/automatic height, identity changes, common X,
viewport width, hide/reveal and 10,000 logical rows. The maximum observed residency
is 15 in that fixed workload. This is **not** a frame-time benchmark of 10,000 real
Audio/MIDI canvases. The real demo was separately launched and captured on Linux.
See `../../verification/no-header/STATUS.md` for exact runs and limitations.

## Porting

Promote the presentation contracts, not `demo/` fixtures. Map real
`TensorTimelineBandView.busId/laneId`, Song/Performer/Track identities, capabilities,
monitoring service, colour resolver, source/event projections and prepared edit
admission from Multiverse. Its coordinator/TempoMap remains authoritative for
musical/nonlinear axes. The local 32-second demo source extent is not a product limit.

The previous canonical TimelineComponent is not silently replaced by this adapter.
Its other hosts require explicit canvas-lifetime integration. The legacy structured
summary rows and local event editing still have the documented limits of the Lab.
