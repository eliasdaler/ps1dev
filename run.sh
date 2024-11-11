#!/bin/bash
set -ex
cd "$(dirname "$0")"
source build.sh
pcsx-redux -exe ${CAT_GAME_DIR}/game.elf -iso ${CAT_GAME_DIR}/game.iso -stdout -lua_stdout -interpreter -run -fastboot
