# Card navigation and passive track state — 20 October 2026

Base: `370459b0629d66857f282efafc2a97a4f15f1ce0` (AyraTimelineLab main).
No change to the ayra_timeline or ayra_multiverse repositories is part of this delivery.
Provider presentation headers match the Git blobs from ayra_timeline `ddf196e9`.

## Executed

Linux x86_64, GNU C++ headless; Clang 17 / JUCE 8.0.12 native;
Xvfb with Openbox. Explicit **snapshot visual profile**, not a newly built full
Multiverse-source application. The app's default requirement for AYRA_SOURCE_DIR
is unchanged. No stubs, alternate renderer or font files are introduced.

| Suite | Result |
| --- | --- |
| Debug core CTest | 4/4 PASS |
| Release core CTest | 4/4 PASS |
| Native identity/render/input CTest | 3/3 PASS |
| Native render/geometry/hover assertions | 251733 PASS |
| Native panel/input assertions | 492 PASS |
| Source boundary audit | PASS |

Counts include loops over pixels and 128 status masks x 4 mute/solo states x both
name sides; they are not independent end-to-end tests. The component test mounts
10,000 logical lanes with instrumented canvases and observed a maximum of 15
resident rows. This does not measure 10,000 real Audio/MIDI editors or audio DSP.

The actual JUCE panel exercises: default hidden side controls/meters, preserved
opt-in callbacks, independent card-navigation toggle and view Undo, event seek,
owner readbacks shared by sibling lanes, Auto monitoring not inferred active,
no text editor, passive input, single deadline with navigation protection,
hidden arrows losing their hit targets, empty-gap latching, visibility reset,
continuous manual resize without double-click height cycling, and row residency.
Native window remapping is allowed to settle before pointer injection. Exact
500 ms boundaries are tested with injected monotonic time; native delivery permits
bounded message-thread jitter rather than treating a UI Timer as sample-accurate.

The render fixtures use the existing HTML-derived material/blur, canonical Ayra
glyph selectors, M/S semantic colours, all-off/individual/all-on combinations,
Dark/Light, left/right, long names and 100/200% status outputs. Existing frosted
lens outputs also exercise 125/150%. They are native **component/painter fixtures**,
not whole-application screenshots or decoded-media/performance evidence.

## Reference audit

Read through GitHub: Multiverse `459d8c5c`, MixerChannelStrip.cpp and NodeButtons.h.
The local NodeButtons snapshot blob `e7ce36fc2bd18f0bec7cf8a214dbedaba84d5df8`
matched the inspected original exactly. New status artwork invokes those selectors
instead of copying private renderer bodies. Monitoring uses the existing Lab Audio
glyph with the shared active-green token; canvas uses the canonical window selector.
These two host mappings are explicit, not claimed as complete mixer runtime bindings.

## Not executed / not implemented here

Full revised demo application build/launch, live Multiverse-source-profile build,
macOS/Windows validation and DSP/runtime integration are NOT_RUN. Retrieval of the
external ayra_curves dependency was attempted using contents, fetch_file (including
its recorded commit) and fetch_blob; those calls returned HTTP 404 in this session.
This is not treated as a repository-permissions diagnosis or as a reason to omit
publication of the scoped, natively verified panel changes.

The previous broader height-preset/MIDI-focus/layout-obstacle plan is not silently
included. Product right-click menus remain app-owned. Synthetic demo flags are
explicit examples; the portable status structure is supplied by the real host.

## Reproduce

```sh
cmake -S tests/identity -B build/card-status \
  -DCMAKE_BUILD_TYPE=Debug -DJUCE_SOURCE_DIR=/path/to/JUCE \
  -DAYRA_MODULES_DIR=/path/to/provider-parent \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF
cmake --build build/card-status --parallel 3
ctest --test-dir build/card-status --output-on-failure
python3 tools/check_panel_boundary.py
```

Use an active display for native Linux tests. To validate original visual leaves,
configure a separate build with AYRA_SOURCE_DIR and require visuals ON.
Logs in this directory record the scoped runs, not older application results.
