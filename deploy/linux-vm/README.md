# Linux VM deployment profile for NESChan WASM runtime

This folder provisions a Linux server VM that can:

1. Build the NES core into WebAssembly (via Emscripten).
2. Host a browser canvas shell for runtime play/testing.
3. Expose a deterministic API surface so another LLM can recreate the same pipeline.

## Quick start (local VM with Vagrant)

```bash
cd deploy/linux-vm
vagrant up
vagrant ssh
```

The VM bootstrap script clones/uses `/workspace/nes`, installs toolchains, and starts a static server at:

- `http://<vm-ip>:8080` for the LLM canvas shell.

## Build + deploy flow in VM

```bash
cd /workspace/nes
./scripts/build_wasm_runtime.sh
./scripts/run_wasm_server.sh
```

## Expected artifacts

- `web/llm-canvas/index.html` – front-end canvas host.
- `web/llm-canvas/neschan.js` – Emscripten output (generated).
- `web/llm-canvas/neschan.wasm` – Emscripten output (generated).

## Notes for LLM reproducibility

- Runtime config is stored in `web/llm-canvas/runtime.manifest.json`.
- Replay contract uses existing input-log format from `doc/replay_input_log.md`.
- A deterministic launch command is embedded in `scripts/build_wasm_runtime.sh`.
