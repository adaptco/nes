# LLM Canvas Contract

This canvas shell is the browser docking surface for running the NES emulator runtime in WebAssembly.

## Contract

- Canvas element id: `nes-canvas`
- Logical resolution: `256x240`
- Runtime manifest: `runtime.manifest.json`
- Input protocol: replay-log-v1 (compatible with `doc/replay_input_log.md`)

## Rebuild flow

From repository root:

```bash
./scripts/build_wasm_runtime.sh
./scripts/run_wasm_server.sh
```

Then open `http://localhost:8080`.
