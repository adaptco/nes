#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  git \
  curl \
  unzip \
  nodejs \
  npm

# Install emsdk for wasm builds.
if [ ! -d /opt/emsdk ]; then
  sudo git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk
fi

cd /opt/emsdk
sudo ./emsdk install latest
sudo ./emsdk activate latest

# Make emsdk available to interactive shells.
if ! grep -q "EMSDK" /home/vagrant/.bashrc; then
  cat <<'EOS' >> /home/vagrant/.bashrc
source /opt/emsdk/emsdk_env.sh >/dev/null 2>&1
EOS
fi

cd /workspace/nes
chmod +x scripts/build_wasm_runtime.sh scripts/run_wasm_server.sh

# Build once at provision time if possible.
sudo -u vagrant bash -lc "cd /workspace/nes && ./scripts/build_wasm_runtime.sh || true"
