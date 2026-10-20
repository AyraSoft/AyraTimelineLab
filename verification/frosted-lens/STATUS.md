# Frosted lens verification — 20 October 2026

Base: `0a1f9304722db721b2575b8ca1808b1f3d000b47`. This report covers the new
HTML-derived identity and passive clip-aware hover policy, not the superseded
rename implementation.

## Executed

Linux x86_64, Clang 17 Debug, JUCE 8.0.12, Xvfb 1600x1200 and Openbox.
Full application and graphical tests use the **original Multiverse source
profile**, including the checked original Buttons TU (not the snapshot fallback).
The 30 staged visual files were verified against the recorded original hashes.
`ayra_timeline` is the actual pinned presentation provider. The unavailable curves
checkout was recovered from previously fetched source and verified against its
original Git blob; no stub/replacement maths were introduced.

| Execution | Result |
| --- | --- |
| Full native application configure / compile / link | PASS |
| Full native CTest | 7/7 PASS |
| Dedicated identity/source-profile CTest | 3/3 PASS |
| Native identity geometry/render/hover assertions | 2,806 PASS |
| Native panel/input assertions | 366 PASS |
| Python visual adapter tests | 3 cases PASS |
| Source boundary audit | PASS |
| Whole application launch, Dark/Light and native XTest pointer checks | PASS |
| macOS / Windows / full Multiverse runtime | NOT_RUN |

The two suites overlap in panel-input coverage; their totals are not ten distinct
end-to-end scenarios. The native row-residency test uses 10,000 logical lanes and
instrumented canvases (maximum resident 15), not 10,000 decoded media editors.

## Interaction coverage

No name/editor input components remain. The real canvas is the mouse target both
when the lens is visible and hidden. Single click, double click, drag and wheel
reach the canvas without rename commands or accidental lane selection.

Tests drive actual JUCE peer mouse dispatch. They cover empty canvas, introducing
a clip under a stationary pointer, staying hidden over gaps, deleting that clip,
leaving/re-entering the lens, both name sides, unchanged hit footprint, and
cached underlay reuse. Existing resize/Undo, hide/show and residency checks remain.
The host's asynchronous pixel-invalidation callback is exercised separately.

Whole-app XTest checks use the real Audio/MIDI/Image canvases, not the instrumented
factory. Hover removes the relevant lens; pointer exit restores the Audio lens
pixel-for-pixel. Zoom-out places the name over empty timeline: hovering does NOT
remove it. The line-clip adapter uses the same existing hit-zone contract; its
interactive path was compiled but not separately automated at OS level.

## Rendering coverage

The supplied HTML was also rendered in Chromium for visual comparison. JUCE
fixtures cover Dark/Light, both anchors, scales 100/125/150/200%, long labels,
widths 0/12/44/120/240/540/1200 and heights 16/24/32/40/48/64/96/400.
The blur test verifies suppression of actual high-frequency background pixels.
The native screenshots use the real painter. Fixture waveforms are synthetic;
whole-app screenshots are the sandbox's synthetic project, not decoded audio.

The HTML specifies Dark only. Light colours and the compact form for tiny global
lanes are explicit adaptations. Available system font weights/antialiasing differ
between JUCE and a browser; pixel-identical output across platforms is not claimed.
No external fonts are included in source or evidence archives.

## Reproduce

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Debug \
  -DJUCE_SOURCE_DIR=/path/to/JUCE -DAYRA_MODULES_DIR=/path/to/providers \
  -DAYRA_SOURCE_DIR=/path/to/ayra_multiverse -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=ON
cmake --build build/native --parallel 3
ctest --test-dir build/native --output-on-failure
cmake -S tests/identity -B build/identity -DCMAKE_BUILD_TYPE=Debug \
  -DJUCE_SOURCE_DIR=/path/to/JUCE -DAYRA_MODULES_DIR=/path/to/providers \
  -DAYRA_SOURCE_DIR=/path/to/ayra_multiverse -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=ON
cmake --build build/identity --parallel 3
ctest --test-dir build/identity --output-on-failure
python3 tools/check_panel_boundary.py
```

Native Linux tests need an active display service/window manager. Logs and source
manifest beside this file describe what ran. Screenshots are delivered separately,
not used as a substitute for code or tests. No claims about persistence, media
Glue, production monitoring, or full platform acceptance are added by this change.
