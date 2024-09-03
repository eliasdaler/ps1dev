#!/bin/bash
set -ex
cd "$(dirname "$0")"
cmake --build --preset=default
duckstation -fastboot build/game.iso
