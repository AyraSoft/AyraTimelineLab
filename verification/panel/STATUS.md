# Executed verification — 19 October 2026

Base: AyraTimelineLab `4fadddde7631087a4466519a8b698ab30dfe3c2d`.
Environment: Linux x86_64 container; GNU C++ 14.2.0 and Clang 17.0.0;
CMake 3.31.6. No JUCE or native Ayra provider checkout in this environment.

| Run | Executables | Final result |
|---|---:|---|
| Debug / GNU | 2 | 976 presentation checks + 90 fixture transaction checks, PASS |
| Release / GNU | 2 | 976 presentation checks + 90 fixture transaction checks, PASS |
| Debug / Clang ASan+UBSan, leak detection enabled | 2 | Same checks, PASS; no sanitizer diagnostics in recorded runs |
| Python source-boundary audit | 1 script | PASS; static source check only |
| Full legacy root suite, generated fixture audit, browser tests | NOT_RUN | Not part of the standalone source workspace |
| Native GUI configure/build/run, screenshots and gesture regressions | NOT_RUN | Missing JUCE/provider environment |
| Native frame-time, full eviction virtualization, audio/DSP | NOT_RUN | No performance/audio certification |

Raw logs: [Debug](debug.log), [Release](release.log), [sanitized](sanitized.log),
[source audit](source-boundary.log). Logs include the initial 974-check run and the
final 976-check rerun after adding convergence-collapse and wrong-owner regressions.
Counts include deterministic randomized viewport checks, not 976 distinct test cases.
Execution elapsed times in CTest are test-run durations, not UI performance metrics.

## Reproduce

```sh
cmake -S modules/ayra_timeline_panel -B build/panel-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/panel-debug
ctest --test-dir build/panel-debug --output-on-failure

cmake -S modules/ayra_timeline_panel -B build/panel-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/panel-release
ctest --test-dir build/panel-release --output-on-failure

cmake -S modules/ayra_timeline_panel -B build/panel-asan \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/panel-asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build/panel-asan --output-on-failure
python3 tools/check_panel_boundary.py
```

No main update, PR, explicit CI dispatch or application execution is part of this verification.
The new native source must be compiled and tested with the actual provider vector
before merging. Passing the headless tests does not certify the native adapter.
