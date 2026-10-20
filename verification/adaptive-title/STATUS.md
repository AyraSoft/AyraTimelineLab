# Adaptive event lanes and protected editable titles — 20 October 2026

Base: AyraTimelineLab `8eceba72bbf14daa2d0dd750f7e24cb833030dc0`.
This revision is confined to the Lab. Neither ayra_timeline nor ayra_multiverse
is modified. External presentation headers were recovered and verified by Git
blob hashes from ayra_timeline `ddf196e9`; no provider source is published here.

## Current executions

Linux x86_64, GNU C++ Debug/Release, Clang 17 ASan/UBSan for headless tests;
Clang 17 with real JUCE 8.0.12 and Xvfb/Openbox for the native component suites.
The visual source profile is explicitly **snapshot**, not live Multiverse.
The default application build requirement for AYRA_SOURCE_DIR is unchanged.

| Execution | Result |
| --- | --- |
| Headless Debug CTest | 5/5 PASS |
| Headless Release CTest | 5/5 PASS |
| Headless Clang ASan/UBSan, detect_leaks=1 | 5/5 PASS |
| Native identity/painter/input CTest | 3/3 PASS |
| Native geometry/render/hover checks | 252309 assertions PASS |
| Native real-panel/input checks | 602 assertions PASS |
| Source boundary/fixture checks | PASS |

Counts include pixel/mask/geometry loops, not independent end-to-end cases.
Native tests instantiate the actual TimelinePanel, title TextEditor and navigation
components with instrumented canvases. They exercise title-only hover protection,
500 ms passive suppression, left/right expansion with a stable original hit region,
Enter/Escape/focus-loss admission, blank names, same-owner propagation, concurrent
rename rejection, session cancellation, and offscreen editor retention. The host
fixture suite separately checks real Store rename Undo and unchanged lane aliases.

Content-driven height tests use the production interval packer and the actual
panel measurement port. Tests cover 24/32 px one-line layouts, 46 px two-line
transition, seven active icons under navigation, no rebind feedback loop, scroll
anchor preservation, no manual event-lane resize, half-open contiguous spans,
offscreen events and >6 overlapping rows. The randomized headless packer tests
compare non-overlap, deterministic assignment and visible-domain admission.

The pre-existing 10000-logical-row native workload remains bounded to 15 resident
instrumented rows in the exercised geometry. This is NOT a media-editor frame-time
benchmark. The existing single panel timer remains the only hover scheduler.

## Evidence scope

The PNGs delivered with the conversation are native JUCE **identity painter
fixtures** with illustrative event rows, not screenshots of the full application.
The source layout helper is used by the real Keyframe canvas; its adapters are
wired in TimelinePanelDemo, not replaced by a second implementation in tests.

The complete application/actual KeyframeLaneCanvas translation unit and live
Multiverse-source build were NOT_RUN in this session: ayra_curves retrieval through
the connector returned HTTP 404; repository and filename searches did not recover
an accessible provider checkout. No stub of that library or copied algorithm was
introduced to make a build appear successful. macOS/Windows and DSP/runtime
validation are also NOT_RUN. This limitation is not a claim about repository
permissions. Full application validation requires the existing real providers.

## Reproduce

```sh
cmake -S . -B build/core -DAYRA_LAB_BUILD_GUI=OFF \
  -DAYRA_MODULES_DIR=/path/to/provider-parent -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --parallel 2
ctest --test-dir build/core --output-on-failure
cmake -S tests/identity -B build/identity \
  -DJUCE_SOURCE_DIR=/path/to/JUCE -DAYRA_MODULES_DIR=/path/to/provider-parent \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/identity --parallel 2
# Linux native input tests require an active display/window manager.
ctest --test-dir build/identity --output-on-failure
```

New portable CI steps execute the production event packer, visual-adapter tests,
source boundary and fixture checks without downloading private providers.
Logs in this directory are executions of this revision, not inherited results.
