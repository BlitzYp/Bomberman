#!/usr/bin/env bash

set -eu

sudo apt update
sudo apt install -y build-essential make libncurses-dev

echo "Dependencies installed."
