# Lane visual polish — executed verification, 20 October 2026

Remote base: `0a1f9304722db721b2575b8ca1808b1f3d000b47`.
This delivery includes the recovered, previously unpushed frosted lens checkpoint
`6c571a9` and the current requested corrections. Older reports retain their dates
and scopes; they are not presented as fresh runs.

Provider: `ayra_timeline` `ddf196e94de744f189b61b75277c2f9df3b8598f`.
Unchanged presentation headers were recovered and checked against original Git
blob IDs. The new provider chrome is tested directly, not copied into this Lab.

## Current executions

Linux x86_64, GNU 14.2 headless, Clang 17 native, JUCE 8.0.12, Xvfb/Openbox.
Native identity/control/input tests use the explicit local visual snapshot;
they do not claim a fresh full Multiverse checkout or full-product GUI build.
The existing native application configuration still requires direct Multiverse
visual sources by default; no fallback was silently enabled there.

| Suite | Result |
| --- | --- |
| GNU Debug core/fixture/panel CTest | 4/4 PASS |
| Clang Debug native identity/control/input CTest | 3/3 PASS |
| Native identity/render/alpha assertions | 228147 PASS |
| Native panel/input assertions | 433 PASS |
| Provider grouped-chrome exhaustive assertions | 262315 PASS |

Assertion counts include loops over pixels and visibility/capability combinations;
they are not separate end-to-end test cases. The 10,000-row input test used at
most 15 resident instrumented rows, not 10,000 real media editors.

Coverage includes card centring at 56/65/96/177/400 px, right-edge alignment,
fully transparent left material AND blur edges, single-chevron paths, bounded
button targets, two shared banks, dynamic minimum heights, compact/automatic
policies, resize/Undo, no auto-height loop, passive input, clip-aware hover latch,
scrolling and backdrop reuse. Core tests also verify unchanged owner/lane data.

The graphical outputs are real JUCE painter/component fixtures in Light/Dark
and both name positions, not screenshots of the whole application. No fonts
or private source bodies are distributed. Full revised application launch,
macOS/Windows acceptance and production audio/runtime are not claimed by these
scoped results. See the older frosted-lens report for earlier app-level evidence.

Reproduce (native tests need a display):

```sh
cmake -S tests/identity -B build/identity -DCMAKE_BUILD_TYPE=Debug \
  -DJUCE_SOURCE_DIR=/path/to/JUCE -DAYRA_MODULES_DIR=/path/to/providers \
  -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS=OFF
cmake --build build/identity --parallel 3
ctest --test-dir build/identity --output-on-failure
cmake -S . -B build/core -DAYRA_LAB_BUILD_GUI=OFF \
  -DAYRA_MODULES_DIR=/path/to/providers -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core --parallel 3
ctest --test-dir build/core --output-on-failure
```
