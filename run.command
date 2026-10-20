#!/usr/bin/env bash
set -euo pipefail

# Local build/launcher. No personal paths, dependency downloads or signing changes.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
    PROJECT_ROOT="$SCRIPT_DIR"
elif [[ -f "$SCRIPT_DIR/../CMakeLists.txt" ]]; then
    PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
    printf '%s\n' 'Cannot locate AyraTimelineLab/CMakeLists.txt.' >&2
    exit 1
fi
export PATH="/usr/local/opt/cmake/bin:/usr/local/bin:/opt/homebrew/bin:/opt/homebrew/opt/cmake/bin:/Applications/CMake.app/Contents/bin:$PATH"
command -v cmake >/dev/null || { printf '%s\n' 'CMake 3.22 or newer is required.' >&2; exit 1; }
BUILD_DIR="${AYRA_LAB_BUILD_DIR:-$PROJECT_ROOT/build/native}"
CONFIGURATION="${AYRA_LAB_CONFIGURATION:-Debug}"
cd "$PROJECT_ROOT"
NCPU="${CMAKE_BUILD_PARALLEL_LEVEL:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

# Existing CMake paths can be reused; environment values take precedence.
cache_path() {
    if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        sed -n "s|^$1:[^=]*=||p" "$BUILD_DIR/CMakeCache.txt" | head -n 1
    fi
}
JUCE_DIR="${JUCE_SOURCE_DIR:-$(cache_path JUCE_SOURCE_DIR)}"
MODULES_DIR="${AYRA_MODULES_DIR:-$(cache_path AYRA_MODULES_DIR)}"
VISUAL_SOURCE_DIR="${AYRA_SOURCE_DIR:-$(cache_path AYRA_SOURCE_DIR)}"
if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    printf '%s\n' 'Set JUCE_SOURCE_DIR to an existing official JUCE 8 checkout.' >&2
    exit 1
fi
if [[ ! -f "$MODULES_DIR/ayra_curves/ayra_curves.h" || ! -f "$MODULES_DIR/ayra_timeline/ayra_timeline_presentation.h" ]]; then
    printf '%s\n' 'Set AYRA_MODULES_DIR to the directory containing ayra_curves and ayra_timeline (caa00902 or newer).' >&2
    exit 1
fi
if [[ ! -f "$PROJECT_ROOT/ayra_ui/melatonin_blur/melatonin_blur.h" ]]; then
    printf '%s\n' 'The pinned melatonin_blur submodule is missing; initialize the repository submodules.' >&2
    exit 1
fi
printf 'Building %s (%s)\n' "$PROJECT_ROOT" "$CONFIGURATION"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIGURATION" -DAYRA_LAB_BUILD_GUI=ON \
    -DJUCE_SOURCE_DIR="$JUCE_DIR" -DAYRA_MODULES_DIR="$MODULES_DIR" \
    -DAYRA_SOURCE_DIR="$VISUAL_SOURCE_DIR" \
    -DAYRA_LAB_REQUIRE_MULTIVERSE_VISUALS="${AYRA_LAB_REQUIRE_MULTIVERSE_VISUALS:-ON}"
cmake --build "$BUILD_DIR" --target AyraTimelineLab --config "$CONFIGURATION" --parallel "$NCPU"

# JUCE generators may use a configuration subdirectory. Support macOS and Linux.
for ARTIFACTS in "$BUILD_DIR/AyraTimelineLab_artefacts/$CONFIGURATION" "$BUILD_DIR/AyraTimelineLab_artefacts"; do
    for BIN in "$ARTIFACTS/Ayra Timeline Lab.app/Contents/MacOS/Ayra Timeline Lab" \
               "$ARTIFACTS/Ayra Timeline Lab" "$ARTIFACTS/AyraTimelineLab"; do
        if [[ -x "$BIN" ]]; then exec "$BIN" "$@"; fi
    done
done
printf 'Build succeeded but the application was not found under %s/AyraTimelineLab_artefacts.\n' "$BUILD_DIR" >&2
exit 1
