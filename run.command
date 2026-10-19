#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Ayra Timeline Lab — Build & Run Script
# Trascina questo file nel Terminale e premi Invio (oppure fai doppio clic nel Finder)
# -----------------------------------------------------------------------------

# Risolvi la directory del progetto indipendentemente da dove viene invocato
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
    PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
    PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
    echo "❌ Errore: Impossibile trovare la radice del progetto AyraTimelineLab." >&2
    exit 1
fi

cd "$PROJECT_ROOT"

# Assicurati che cmake, ninja e i tool Homebrew siano nel PATH
export PATH="/usr/local/opt/cmake/bin:/usr/local/bin:/opt/homebrew/bin:/opt/homebrew/opt/cmake/bin:/Applications/CMake.app/Contents/bin:$PATH"

if ! command -v cmake &>/dev/null; then
    echo "❌ Errore: 'cmake' non trovato nel PATH. Installa CMake o verifica l'installazione." >&2
    exit 1
fi

# Directory di build e configurazione
BUILD_DIR="$PROJECT_ROOT/build"
NCPU=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

JUCE_DIR="/Users/ayra/ayra-dsp-audit.wbZu5T/JUCE"
MODULES_DIR="/Users/Lavori/Juce/Juce Modules"

echo "=========================================================="
echo "🚀 Avvio compilazione Ayra Timeline Lab..."
echo "📂 Progetto: $PROJECT_ROOT"
echo "⚙️  Core CPU: $NCPU"
echo "=========================================================="

# Se la directory di build non è ancora configurata per la GUI, configurala
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "🔧 Configurazione CMake iniziale..."
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DAYRA_LAB_BUILD_GUI=ON \
        -DJUCE_SOURCE_DIR="$JUCE_DIR" \
        -DAYRA_MODULES_DIR="$MODULES_DIR"
fi

# Compila l'eseguibile dell'applicazione
echo "🔨 Compilazione target 'AyraTimelineLab'..."
cmake --build "$BUILD_DIR" --target AyraTimelineLab -j"$NCPU"

APP_BUNDLE="$BUILD_DIR/AyraTimelineLab_artefacts/Ayra Timeline Lab.app"
APP_BIN="$APP_BUNDLE/Contents/MacOS/Ayra Timeline Lab"

if [ ! -x "$APP_BIN" ]; then
    echo "❌ Errore: Il binario '$APP_BIN' non esiste o non è eseguibile." >&2
    exit 1
fi

echo "=========================================================="
echo "✅ Compilazione completata con successo!"
echo "🚀 Lancio di Ayra Timeline Lab in corso..."
echo "=========================================================="

# Avvia l'app passando eventuali argomenti passati allo script (es. --legacy o --session)
exec "$APP_BIN" "$@"
