# Ayra media edit — experimental foundation

This is an **opt-in, standalone C++20 contract prototype**, not a finished editor
and not yet wired into Ayra Timeline Lab. The existing GUI and root build are
unchanged. No third-party library, private provider, font or media is bundled.

Read [the architecture and delivery plan (Italian)](ARCHITECTURE.it.md) and
[executed verification / explicit limits](VERIFICATION.md).

## Implemented

`CaptionDocument.h` owns a small source-bound audio/caption document. It validates
IDs, source/stream matches and half-open time ranges. Caption projection follows
move, trim, split and audio deletion. Editing is atomic, session/revision guarded,
with bounded snapshot Undo/Redo and monotonically increasing revisions.

`AudioRouting.h` validates audio sends and sidechain control dependencies as a
combined acyclic graph. It rejects unknown buses, duplicate routes, self/indirect
feedback and invalid parameters. **It does not process sound.**

The clock is integer ticks at 282240000/s. The current slice supports unity-rate
clips only; input times must be converted by a future importer. The actual asset
length is not known here, so media bounds must additionally be checked by that
importer. UI seconds and JavaScript numbers must not become canonical time.

A split inside a word emits two bounded fragments with `partialWord=true` rather
than guessing missing audio. Muting is not deletion. Deletion removes dependent
bindings, not a shared transcript or an original media file.

## Build without JUCE, private modules or network

From the repository root:

```sh
cmake -S modules/ayra_media_edit -B build/media-edit -DCMAKE_BUILD_TYPE=Debug
cmake --build build/media-edit --config Debug
ctest --test-dir build/media-edit -C Debug --output-on-failure
```

Use `-DAYRA_MEDIA_EDIT_SANITIZE=ON` with Clang/GCC for ASan/UBSan.
The test executable runs 29 named cases; CTest registers it as one test target.
Checks are not `assert`-based and remain active in Release builds.

## Not implemented

Project persistence/migration, media import/playback, actual transcription or word
alignment, audio DSP/recording, AV links, compound clips, full multi-track command
admission, retiming, Remotion integration, caption animations, graphics effects,
transitions, social export and the native UI adapter are roadmap work, not working
features of this prototype. No existing Lab source is silently replaced.

The owner is single-writer/control-thread only, not realtime-safe. Snapshot history
is bounded by count, not memory bytes; lookups/projection are intentionally simple.
Replace them with indexed storage and a command journal before large-project use,
while retaining the tested invariants. This is not a published file-format API.
