FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       bash \
       build-essential \
       cmake \
       ninja-build \
       libsdl2-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/nes

# Optional embedded model bundle. Keep this directory in source control
# (or copy generated models into it before building) to bake models into the image.
COPY models/ /opt/nes/models/

# Helper environment defaults for headless backend usage.
ENV NES_MODEL_DIR=/opt/nes/models \
    NES_ROM_DIR=/workspace/nes/roms

CMD ["bash"]
