#!/bin/bash
set -ex
cd "$(dirname "$0")"

# rebuild
cmake --build --preset=default

# launch pcsx
pcsx-redux -iso build/game.iso -gdb -debugger -interpreter -run -fastboot -notrace &
# start gdb
gdb-multiarch

# kill launched pcsx-redux
if tty=$(tty 2>/dev/null); then
    kill -9 $(pgrep --terminal "${tty#/dev/}" pcsx-redux)
fi
