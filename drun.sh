#!/bin/bash
set -ex
cd "$(dirname "$0")"
source build.sh
duckstation -fastboot ${CAT_GAME_DIR}/game.iso
