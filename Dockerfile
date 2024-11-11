FROM debian:sid-slim AS builder-stage

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update

RUN apt install -y build-essential ninja-build cmake gdb-multiarch gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu 
RUN apt install -y libmagick++-dev

WORKDIR /build/

COPY cmake cmake
COPY tools tools
COPY third_party third_party
COPY CMakeLists.txt .

RUN cmake -GNinja -B build -DCMAKE_BUILD_TYPE="Release"
RUN cmake --build build

COPY games games
RUN cmake -GNinja -B build_games -S games -DBUILD_ASSETS=OFF -DCMAKE_BUILD_TYPE="Release"
RUN cmake --build build_games

FROM scratch
COPY --from=builder-stage /build/build_games/cat_adventure/game.iso / 
COPY --from=builder-stage /build/build_games/cat_adventure/game.cue /
COPY --from=builder-stage /build/build_games/cat_adventure/game.elf /
