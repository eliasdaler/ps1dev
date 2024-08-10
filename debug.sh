#!/bin/bash
set -ex
cd "$(dirname "$0")"
cmake --build --preset=default
pcsx-redux -iso build/game.iso -gdb -debugger -interpreter -stdout -lua_stdout -run -fastboot &
gdb-multiarch
trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT
