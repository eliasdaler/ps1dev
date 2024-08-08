#!/bin/bash
pcsx-redux -exe build/game.ps-exe -gdb -debugger -interpreter -stdout -lua_stdout -run &
gdb-multiarch
trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT
