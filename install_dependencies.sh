#!/usr/bin/env bash

set -eu

sudo apt update
sudo apt install -y build-essential make libncurses-dev

# Install Raylib dependencies
sudo apt install -y \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxi-dev \
    libxext-dev \
    libxcursor-dev \
    libgl1-mesa-dev \
    libudev-dev

# Install Raylib
git clone https://github.com/raysan5/raylib.git /tmp/raylib
cd /tmp/raylib/src
make
sudo make install

echo "Dependencies installed, including Raylib."
