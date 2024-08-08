#!/bin/bash
cmake --build --preset=default
pcsx-redux -exe build/game.ps-exe -gdb -debugger -interpreter -stdout -lua_stdout -run
