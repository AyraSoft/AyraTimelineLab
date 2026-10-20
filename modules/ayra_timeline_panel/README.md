> Current contract: only the track title and navigation intercept input. Title
> hover protects/expands the lens; double click renames its owner through the host.
> One panel scheduler hides passive areas after 500 ms over editable content.
> Automatic event lanes measure the widgets, never the card. See
> `../../docs/ADAPTIVE_CARD_AND_TITLE_EDIT.md`.

# Ayra Timeline Panel — no-header presentation

`TimelineModel.h` is C++20 without JUCE. The native `TimelinePanel.h` consumes it
and official JUCE. Musical documents, transport, media, DSP and Undo admission
remain outside the module. The default demo uses **free lanes**; the existing
structured projection is retained for comparison, not redesigned in this iteration.

## One provider for geometry and residency

The module includes `<ayra_timeline/ayra_timeline_presentation.h>` from an external
checkout at `ddf196e9` or compatible. No fallback implementation is copied here.

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
revision, owner and the relevant lane/event scope. Inline title edits use renameOwner;
the separate renameLane API continues to change only a lane alias;
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

`containsEditableAt` uses the exact canvas input geometry (clips, points, span
edges, curve segments and handles). `containsClipAt` remains a compatibility
fallback. Missing callbacks never invent editable regions. Performer/type/status remain passive. The actual title hit region protects the
card and owns a lazy inline editor. Navigation uses 16x18 targets, or 16x12 in a
compact 22px card with active status glyphs beneath; hidden cards have no invisible buttons. `eventTimes` supplies
local editor targets; otherwise the controller uses canonical snapshot events.
`setContentChangeListener` reports programmatic content changes independently of a
frame revision. Unregister the listener at teardown. `preferredHeight` is a pure
content measurement, and repeated identical values do not cause recursive binds.

`Owner::status` is a read-only host projection, shared by every lane of that owner.
The seven effective booleans do not change the monitoring policy, document flags or
runtime. Monitoring Auto is not proof of activity; `canvasActive` is not row visibility.
`muted/soloed` remain the existing owner fields, independent of side control visibility.

`ViewState::identityNavigation` defaults true. The old side `chrome` APIs, buttons,
wells, callbacks and their height law are preserved but default to hidden, like meters.
The resize marker defaults hidden; the bottom hit region still resizes manual lanes.
Continuous lanes remain manually resizable, but do not cycle height on double-click.

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
title editing/passive metadata/navigation/status, compact/manual/automatic height, identity changes, common X,
viewport width, hide/reveal and 10,000 logical rows. The maximum observed residency
is 15 in that fixed workload. This is **not** a frame-time benchmark of 10,000 real
Audio/MIDI canvases. The real demo was separately launched and captured on Linux.
See `../../verification/adaptive-title/STATUS.md` for current runs and limitations.
Older full-application evidence remains scoped to its original revision.

## Porting

Promote the presentation contracts, not `demo/` fixtures. Map real
`TensorTimelineBandView.busId/laneId`, Song/Performer/Track identities, capabilities,
monitoring service, colour resolver, source/event projections and prepared edit
admission from Multiverse. Its coordinator/TempoMap remains authoritative for
musical/nonlinear axes. The local 32-second demo source extent is not a product limit.

The previous canonical TimelineComponent is not silently replaced by this adapter.
Its other hosts require explicit canvas-lifetime integration. The legacy structured
summary rows and local event editing still have the documented limits of the Lab.
