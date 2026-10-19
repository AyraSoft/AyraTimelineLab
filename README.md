# Ayra Timeline Lab

## Headless-track timeline prototype

This branch adds a portable C++20 presentation module and a **native JUCE integration
candidate** around the existing clip canvases. No meters or faders; owner accent and
editable stream/type colour are separate. Structured and free-lane views share owner
controls and keep presentation order separate from musical ownership.

**Verified:** the new headless presentation and fixture-transaction tests in Debug,
Release and Clang ASan/UBSan. **Not verified:** native GUI compilation, launch, gesture
parity or performance. This is not the completed production module.

Start with [implementation and limits](docs/TIMELINE_PANEL_IMPLEMENTATION.md),
[module contracts](modules/ayra_timeline_panel/README.md) and
[executed verification](verification/panel/STATUS.md).
The previous Lab README is retained verbatim as
[legacy reference](docs/LEGACY_LAB_README.md); its paths are relative to repository root.

## Build the new headless tests, without JUCE or network

```sh
cmake -S modules/ayra_timeline_panel -B build/panel-core -DCMAKE_BUILD_TYPE=Debug
cmake --build build/panel-core
ctest --test-dir build/panel-core --output-on-failure
```

The root core-only build also registers these tests alongside the existing core tests.

## Native candidate

```sh
cmake -S . -B build/panel-native \
  -DJUCE_SOURCE_DIR="/path/to/JUCE" \
  -DAYRA_MODULES_DIR="/path/to/ayra-modules" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/panel-native --target AyraTimelineLab --config Debug
```

The default application opens the new 17-lane fictitious project. `--legacy` preserves
the previous clip-design application; `--session "file.json"` uses that legacy app and
its JSON v1 codec. New showcase metadata is in memory and is not falsely exported as
v1. The approved files under `app/clips/` are unchanged.

Temporal grouping is reversible relation metadata, **not yet a composite visual
container or rendered PCM Glue**. Point/Span Keyframe and Continuous Line retain their
old local editing state; document navigation is disabled until their real event
projection is connected. The linked fixture edits support atomic geometry/delete,
not a new canonical media engine. Read the limits before native evaluation or porting.
