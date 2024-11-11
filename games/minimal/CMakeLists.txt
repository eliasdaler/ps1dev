# Minimal example with psyqo
# run:
#   pcsx-redux -exe build_games/minimal/minimal_game.elf -fastboot -run
cmake_minimum_required(VERSION 3.25)

project(
  minimal
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

add_executable(minimal_game
  main.cpp
)

target_compile_options(minimal_game PRIVATE
  -Wall
  -Wextra
  # -pedantic - FIXME: libgpu.h gets annoying with this (anonymous struct usage)
  -Wno-parentheses
  -Wno-pointer-arith # psyqo alloc.c
  -Wno-unused-parameter
  -Wno-unused-variable
  -Wno-unused-label
  -Wno-missing-field-initializers
  -Wno-sign-compare # TODO: enable
  -Wno-attributes
)

target_link_libraries(minimal_game PRIVATE
  psyqo::psyqo
)

