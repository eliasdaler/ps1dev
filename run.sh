#!/bin/bash
cmake --build --preset=default
pcsx-redux -iso build/game.iso -stdout -lua_stdout -run -fastboot
