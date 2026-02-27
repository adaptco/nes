#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR/web/llm-canvas"

PORT="${PORT:-8080}"
echo "Serving LLM canvas shell at http://0.0.0.0:${PORT}"
python3 -m http.server "$PORT" --bind 0.0.0.0
