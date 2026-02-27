#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"

echo "[nes-backend] Building backend image with embedded models from ./models ..."
docker compose -f docker-compose.backend.yml build nes-backend

echo "[nes-backend] Starting interactive backend shell ..."
docker compose -f docker-compose.backend.yml run --rm nes-backend
