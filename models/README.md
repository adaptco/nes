# Embedded model artifacts

Place trained model assets in this folder to bundle them into the backend Docker image.

At runtime, the backend shell exposes this path as:

- Image path: `/opt/nes/models`
- Env var: `NES_MODEL_DIR`
