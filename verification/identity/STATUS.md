# Identity and visual-source verification — 20 October 2026

Source workspace: unpublished no-header checkpoint `278060c` plus the identity and
visual-source changes in this revision. The recorded remote base was `b214bcc7`.
This report separates executions from older evidence and from product acceptance.

## Executed in this revision

Linux x86_64; Clang 17 native tests, GNU 14.2 headless; CMake 3.31.6;
JUCE 8.0.12; Xvfb 1600x1200 and Openbox. The test uses the real JUCE raster painter,
fonts and components. No substitute renderer or stub framework was introduced.

| Run | Result |
| --- | --- |
| Native source profile: configure, build and link | PASS, including the generated original Buttons TU |
| Native source profile CTest | 3/3 PASS: adapter, identity painter, native panel/input |
| Native explicit snapshot profile CTest | 3/3 PASS, separately rebuilt |
| GNU Debug core CTest | 4/4 PASS |
| Native identity layout/render assertions | 745 PASS per profile |
| Native panel/input assertions | 354 PASS per profile |
| Python adapter tests | 3 unittest cases PASS, including contract drift and atomic rejection |
| Actual original Buttons body comparison | PASS; reversing include/qualifier changes restores the source exactly |
| Full revised application rebuild/launch | NOT_RUN in this styling session |
| Full ApplicationLookAndFeel/runtime, macOS/Windows | NOT_RUN |

The native panel test uses 10,000 logical rows with instrumented canvases; maximum
residency was 15 in its geometry. This is not a frame-time benchmark with 10,000
real Audio/MIDI editors. It exercises title/secondary rename targets, stale context,
input forwarding, resize, selection, finite coordinates and identity backdrop
reuse on stationary and horizontal-scroll frames. Owner rename has fixture Undo
and sibling-label tests; lane alias remains independent.

The graphical fixtures cover Dark/Light, widths 0/12/44/120/540/1200 and row heights
16/24/48/96/400 for layout; visual outputs at 100/125/150/200%, both name sides,
long labels and real canonical button states/meter painters. They are offscreen
JUCE painter fixtures, NOT screenshots of the whole application or decoded media.
They are delivered separately, not inserted as opaque binaries into the source
repository. The new treatment still requires user aesthetic approval.

## Sources actually compiled

Thirty visual source files were staged read-only outside the public repository and
verified against GitHub blob hashes for Multiverse `459d8c5c`. This is an exact
source slice, not a claim of a full Multiverse checkout/build. See
[reference hashes](reference-hashes.json) and the generated
[build manifest](visual-source-manifest.txt). The original private files and generated
private-body TU are NOT published. `ApplicationLookAndFeel_Buttons.cpp` was
compiled with only the class qualifier/include adapted; the new name painter is
a host-side proposal that consumes canonical tokens, not an existing product widget.

Logs: [source build](source-build.log), [source CTest](source-ctest.log),
[source assertions](source-tests.log), [snapshot CTest](snapshot-ctest.log),
[snapshot assertions](snapshot-tests.log), [core CTest](core-ctest.log),
[core assertions](core-tests.log), [body audit](adapter-body-audit.txt).

Earlier complete 19-lane application verification belongs to the
[historical no-header report](../no-header/STATUS.md). It does not validate every
new application-level interaction or platform by inheritance.

## Reproduce scoped visual verification

```sh
cmake -S tests/identity -B build/identity \
  -DJUCE_SOURCE_DIR=/path/to/JUCE \
  -DAYRA_MODULES_DIR=/path/to/provider-parent \
  -DAYRA_SOURCE_DIR=/path/to/ayra_multiverse \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/identity --parallel 2
ctest --test-dir build/identity --output-on-failure
```

A display service/window manager is required by the native Linux test. To compare
snapshot deliberately, use a separate build directory, empty AYRA_SOURCE_DIR and
`-DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF`. The manifest and UI distinguish them.
