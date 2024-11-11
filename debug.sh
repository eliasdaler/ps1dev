#!/bin/bash
set -ex
cd "$(dirname "$0")"

source build.sh

# launch pcsx
pcsx-redux \
    -exe ${CAT_GAME_DIR}/game.elf \
    -iso ${CAT_GAME_DIR}/game.iso \
    -gdb -debugger -interpreter -run -fastboot -notrace &

# start gdb
gdb-multiarch --symbols="${CAT_GAME_DIR}/game.elf"

# kill launched pcsx-redux
if tty=$(tty 2>/dev/null); then
    kill -9 $(pgrep --terminal "${tty#/dev/}" pcsx-redux)
fi
