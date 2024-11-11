#!/bin/bash
set -ex
cd "$(dirname "$0")"

# Everyone forgets to do that
# git submodule update --init --recursive

if [ ! -d build ]; then
    cmake -B build -DCMAKE_BUILD_TYPE="RelWithDebInfo"
fi
cmake --build build

GAMES_BUILD_DIR="build_games"
GAMES_BUILD_TYPE="Release"

if [[ "$1" == "DEBUG" ]]; then
    GAMES_BUILD_DIR="build_games_debug"
    GAMES_BUILD_TYPE="Debug"
fi

CAT_GAME_DIR="${GAMES_BUILD_DIR}/cat_adventure"

if [ ! -d ${GAMES_BUILD_DIR} ]; then
    cmake -B ${GAMES_BUILD_DIR} -S games -DCMAKE_BUILD_TYPE=${GAMES_BUILD_TYPE}
fi
cmake --build ${GAMES_BUILD_DIR}
