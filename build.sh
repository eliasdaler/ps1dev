#!/bin/bash
set -ex
cd "$(dirname "$0")"

if [ ! -d build ]; then
    cmake -B build
fi
cmake --build build

CAT_GAME_DIR="build_games/cat_adventure"

if [ ! -d build_games ]; then
    cmake -B build_games -S games
fi
cmake --build build_games
