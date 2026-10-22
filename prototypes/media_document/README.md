# Source-bound caption document prototype

C++20, standard library only. This is an **isolated executable contract**, not a new canonical Ayra module, an integrated editor, a media engine or a replacement for the Lab's presentation model.

The prototype keeps word timestamps in source coordinates. A binding is owned by a specific audio clip instance; derived caption cues follow that instance. Split and duplicate allocate explicit new identities. Deleting one instance deletes only its bindings, not the shared transcript. Batch edits validate a working copy and publish one revision, or leave the original unchanged.

Supported: positive, forward **1x** source/timeline mapping in one caller-supplied integer tick domain; move; shrinking trim; split; duplicate; delete; enabled/hidden caption binding; stale-revision rejection; transactional batches. Half-open intervals prevent a word fragment from being active twice at a split boundary. A split through a word retains that word's identity and marks both truncated fragments for editorial review.

Not implemented: ASR, persistence, Undo/redo history, UI adapter, media import/playback, source-rate conversion, rational retiming, ripple planning, nested sequences, multiple caption languages per clip, style rendering, audio DSP, export or production indexing. Full-document copies and linear lookups deliberately favor a small auditable contract over production performance. The prototype does not claim these operations are fast on large projects. Promotion requires the phase-0/phase-1 design review.

## Reproduce the checks

From the repository root:

```sh
cmake -S prototypes/media_document -B build/media-document -DCMAKE_BUILD_TYPE=Debug
cmake --build build/media-document --parallel 2
ctest --test-dir build/media-document -V
```

Clang/GCC sanitizer build:

```sh
cmake -S prototypes/media_document -B build/media-document-sanitize \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DAYRA_MEDIA_DOCUMENT_SANITIZE=ON
cmake --build build/media-document-sanitize --parallel 2
ctest --test-dir build/media-document-sanitize -V
```

One CTest executable contains 20 contract cases, including an exhaustive check of all 99 interior split positions in the small fixture. Checks do not use `assert`, so they remain enabled in Release. This subproject requires no JUCE, private provider, network download or media/model asset. Root CMake and existing CI are intentionally unchanged; invoke this subproject explicitly.

See [architecture](../../docs/media-editor/ARCHITECTURE.md) and [actual verification scope](../../verification/media-editor-foundation/STATUS.md).
