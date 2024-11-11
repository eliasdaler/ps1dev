#!/bin/bash
set -ex
cd "$(dirname "$0")"
source build.sh
pcsx-redux -exe build_games/minimal/minimal_game.elf -stdout -lua_stdout -interpreter -run -fastboot
