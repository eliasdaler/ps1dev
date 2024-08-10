#!/bin/bash
cd "$(dirname "$0")"
cmake --build --preset=default
pcsx-redux -iso build/game.iso -stdout -lua_stdout -interpreter -run -fastboot
# pcsx-redux -iso build/game.iso -stdout -lua_stdout -dynarec -run -fastboot
