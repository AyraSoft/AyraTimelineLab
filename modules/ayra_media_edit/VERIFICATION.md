# Verification — 22 October 2026

## Scope

Executed locally on Linux against the added standalone module files only.
This is not a build or regression run of the existing JUCE application, private
Ayra providers, or a full repository checkout. No user media was processed.

Baseline inspected using the GitHub connector:
`48502a95c92ce10a8a974dc792bae45027ab0ab0`.
A terminal `git clone` failed with `Could not resolve host: github.com`.
Repository reads and publication use the connected GitHub actions instead;
the terminal DNS error is not a connector permission failure.

## Executed matrix

| Configuration | Toolchain | Result |
|---|---|---|
| Debug | GCC 14.2.0, CMake 3.31.6 | 29/29 cases, 1/1 CTest target passed |
| Release | GCC 14.2.0, CMake 3.31.6 | 29/29 cases, 1/1 CTest target passed |
| Debug + ASan/UBSan | Clang 17.0.0, CMake 3.31.6 | 29/29 cases, 1/1 CTest target passed; no sanitizer report |

All builds used `-Wall -Wextra -Wpedantic -Werror`. Test checks remain active with
NDEBUG; no Release success is inferred from disabled assertions.

Representative commands (repository root):

```sh
cmake -S modules/ayra_media_edit -B build/media-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/media-debug
ctest --test-dir build/media-debug -V

cmake -S modules/ayra_media_edit -B build/media-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/media-release
ctest --test-dir build/media-release -V

cmake -S modules/ayra_media_edit -B build/media-sanitize \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  -DAYRA_MEDIA_EDIT_SANITIZE=ON
cmake --build build/media-sanitize
ctest --test-dir build/media-sanitize -V
```

## Tested contracts

Source-local/half-open word projection; independent clip placement; move and trim;
split in a gap and inside a word; moving/deleting either split; dependency cascade;
shared transcript retention; Undo/Redo restoration and monotonic revisions;
stale/cross-session rejection; invalid/duplicate references and source/stream
mismatches; invalid words, overflow, negative time and invalid split/trim rollback;
bounded history; explicit mute versus caption visibility; valid routing and
rejection of self/indirect/mixed routing feedback, missing/duplicate buses/routes,
and invalid/nonfinite parameters; clock divisibility; 99 consecutive source-aware
splits followed by deletion of 50 pieces without shifting remaining source cues.

## Git blob hashes of the executed code

| Path | SHA-1 Git blob |
|---|---|
| `CaptionDocument.h` | `6759535e07afe4af2c9644ed8b79391fe7e20f27` |
| `AudioRouting.h` | `87d8b97aed0039d2cb6e18aa3826b87dcb7dc106` |
| `CMakeLists.txt` | `f059a0916c6b257300347d1179829ba59c57a440` |
| `tests/MediaEditTests.cpp` | `962ab6ae9f6b180acb0cc7e090c9ee000f8905d8` |

## Not verified / not implemented

No macOS or Windows build, native timeline integration, GUI run, real audio/video
playback, persistence/recovery, ASR accuracy, DSP/ducking processing, graphics or
Remotion render, export, hardware performance or CI outcome is claimed here.
The tests establish the narrow contracts above, not production readiness.

The isolated module is deliberately not included in the root CMake targets yet;
its own CMake entry point is the reproducible test boundary. Integration and
regression testing of the native application are explicit roadmap work.
