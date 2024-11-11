#!/bin/bash
set -ex
cd "$(dirname "$0")"

if [ ! -d build ]; then
    cmake -B build
fi
cmake --build build

GAMES_BUILD_DIR="build_games"
GAMES_BUILD_TYPE="Release"

# This is not perfect but will do for now
# Basically, we build game with -O0 for debug, but have
# to do it in a separate dir
if [[ "$1" == "DEBUG" ]]; then
    GAMES_BUILD_DIR="build_games_debug"
    GAMES_BUILD_TYPE="Debug"
fi

CAT_GAME_DIR="${GAMES_BUILD_DIR}/cat_adventure"

if [ ! -d ${GAMES_BUILD_DIR} ]; then
    cmake -B ${GAMES_BUILD_DIR} -S games -DCMAKE_BUILD_TYPE=${GAMES_BUILD_TYPE}
fi
cmake --build ${GAMES_BUILD_DIR}
