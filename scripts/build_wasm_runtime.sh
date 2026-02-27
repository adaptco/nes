#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/web/llm-canvas"
BUILD_DIR="$ROOT_DIR/build-wasm"

if ! command -v emcc >/dev/null 2>&1; then
  echo "error: emcc not found. Source emsdk first (e.g. 'source /opt/emsdk/emsdk_env.sh')." >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# Build a minimal wasm module from the emulator core for browser integration.
em++ \
  -std=c++17 \
  -O2 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  -I"$ROOT_DIR/lib/inc" \
  "$ROOT_DIR/lib/src/nes_cpu.cpp" \
  "$ROOT_DIR/lib/src/nes_memory.cpp" \
  "$ROOT_DIR/lib/src/nes_system.cpp" \
  "$ROOT_DIR/lib/src/nes_ppu.cpp" \
  "$ROOT_DIR/lib/src/nes_input.cpp" \
  "$ROOT_DIR/lib/src/nes_mapper_mmc3.cpp" \
  "$ROOT_DIR/lib/src/mappers/nes_mapper_mmc1.cpp" \
  "$ROOT_DIR/lib/src/mappers/nes_mapper_nrom.cpp" \
  -o "$OUT_DIR/neschan.js"

cat > "$OUT_DIR/runtime.manifest.json" <<'JSON'
{
  "name": "neschan-wasm-runtime",
  "entry": "index.html",
  "wasmModule": "neschan.wasm",
  "jsLoader": "neschan.js",
  "llmContract": {
    "canvasId": "nes-canvas",
    "frameBuffer": "rgba-8888",
    "inputProtocol": "replay-log-v1"
  }
}
JSON

echo "WASM runtime built in $OUT_DIR"
