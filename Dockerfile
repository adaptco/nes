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

CMD ["bash"]
