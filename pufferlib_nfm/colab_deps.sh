#!/usr/bin/env bash
# Install Colab/Ubuntu packages needed to link PufferLib (OpenMP + raylib GL/X11).
set -euo pipefail
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
  build-essential \
  clang \
  ccache \
  libgomp1 \
  libomp-dev \
  libgl1-mesa-dev \
  libx11-dev \
  libxi-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxext-dev \
  unzip \
  curl
echo "deps ok"
