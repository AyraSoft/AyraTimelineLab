# Ayra Timeline Panel — presentation prototype

**Status: tested headless core + uncompiled native integration candidate.**
This module is a proposed presentation boundary, not a replacement for the existing
`ayra_timeline` document, edit planners, sources, transport, or runtime.

## Files and ownership

`TimelineModel.h` is portable C++20. `Snapshot` is derived, read-only display data;
`Controller` owns only `ViewState`, geometry, lane selection, navigation indices
and bounded view Undo. `CommandSink` sends semantic requests with session and
revision to an external owner. There is no media, DSP, file I/O or musical write.

`TimelinePanel.h` hosts a `CanvasFactory`, a read-only `PanelStyle`, and a separate
`TimelinePanelLookAndFeel` interface. Existing clip geometry and pointer ownership
remain in the supplied canvases. The module does not include Lab/Multiverse types.
The interface CMake target is `ayra_timeline_panel`; including the native header
additionally requires the host to link official JUCE 8 `juce_gui_basics` or above.

`../../demo/PanelFixture.h` is the only synthetic document adapter. It is NOT a
provider to promote into production. The demo's one existing Lab Store owns an
immutable metadata sidecar plus the old compatibility rows consumed by canvases.
The metadata is authoritative for owner controls; compatibility row copies are
read projections, not independent track states.

## Frozen design decisions for this experiment

No input/output meters and no faders. Owner accent and settings-derived stream
colour remain independent. Audio/MIDI/DMX/etc. are lane capabilities, never a
track role inferred from a name. In production inject the same type-colour resolver
used by Tensor and its settings. The demo's editable palette is explicitly a fixture.

One owner has one Mute, Solo and Monitoring policy regardless of displayed lane
count. Open addresses that owner. Monitoring is not a lane-local SourceSlot control;
production wiring must map the owner intent onto the canonical monitoring service.
Song Main capability suppresses Solo. Owner controls and musical effectiveness are
different: the module must not reconstruct the runtime audibility resolver.

A fixed 16 px side rail carries owner accent, bounded hierarchy guides and stream
colour. Hierarchy depth NEVER changes temporal X. Compact controls and bounded
identity labels live in the 24 px separator below each lane's full canvas bounds.
The bottom 4 px is resize hit space, not clip gesture space. No floating overlay
captures clicks over clip handles. Drag the accent to reorder. Body height changes
independently per lane; preview is transient, Escape cancels, release makes one view
Undo. Double-click the separator cycles 96/256/400 subject to lane limits.

The separator is a first native layout candidate, not a user-approved pixel layout.
It intentionally trades 24 px of separation for non-overlapping controls.

## View modes and relations

Structured mode follows declared owner parentage and preserves an empty/collapsed
owner summary, including convergence/MIX. Reordering cannot reparent musical data.
Free mode orders typed lane IDs independently. Switching modes changes only view
state. Both modes preserve the inactive layout groups; a partial free group NEVER
silently widens into whole tracks. Structured layout groups use whole owner IDs,
free layout groups use lane IDs. Pinned rows are fixed in order, not a separate
frozen-scroll viewport in this prototype.

Three concepts are explicit:

- **LayoutGroup**: vertical arrangement only, scoped to one view mode.
- **temporalGroup**: reversible same-lane temporal grouping, without media render.
- **clipLink**: explicit clip references across lanes; physical geometry propagates,
  while gain, fade, colour and owner control remain independent.

The fixture's temporal group is NOT `prepareGlueAudioSelection`: canonical Audio
Glue requires a worker-produced immutable PCM asset and exact Dissolve provenance.
No PCM Glue/Bounce is performed here. The product must retain those existing owners.
Cross-owner links require product-level multi-owner admission, not merely a GUI Undo.

In the fixture a geometry/delete request closes over the explicit group/link graph,
checks locks/conflicting deltas/all bounds, and submits one candidate to the existing
Store. No partial publication is allowed. Split/duplicate during a linked geometry
change is rejected rather than inventing relation-aware splitting. Fade/warp/note
editor internals already local to old canvases have not gained canonical persistence
or whole-link time-stretch semantics by this change.

## Performance and lifetime

Viewport range lookup is binary; navigation uses sorted event starts and has no
wrap. Summary navigation includes descendant document events. No paint-time decoding
or projection is introduced. Rows are mounted lazily; pure scroll does not rebind an
unchanged canvas frame. Visited rows are retained because the approved legacy
canvases contain local editor state. This is **not** bounded eviction virtualization,
not zero-allocation certification, and not a measured native performance improvement.
Session/revision guards reject stale asynchronous commands. Destroy the native panel
before its Controller, factory-captured services and borrowed LookAndFeel.

## Integration into Multiverse

Read canonical Song/Performer/Track/convergence identities from `TimelineProjection`;
map `TensorTimelineBandView.busId/laneId` explicitly. Do not use a display row index as
an authoring destination. Supply existing clip providers to `CanvasFactory`, canonical
owner capability/effective-state readers to the projection, settings colour resolution
to `PanelStyle`, and document edit admission to `CommandSink`. Musical coordinates
here are already resolved display seconds: beat/sample/timecode conversion must remain
with the canonical TempoMap/anchor services. No second time conversion formula belongs
in this module. The demo retains the legacy canvas 32-second extent; the generic panel
exposes `setTimeExtent`, which a production factory must consume consistently.

Do not copy `demo/`, legacy Store limits, synthetic waveforms, local curve states,
monitor policy cycling or fixture safety limits into the product.
