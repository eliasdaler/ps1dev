cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/Toolchain.cmake")

project(
  ps1_game
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # nugget uses "asm" GCC extension

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PSYQ_DIR "${CMAKE_CURRENT_LIST_DIR}/../psyq" CACHE PATH "Location of PsyQ libraries")

set(PSXTOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../psxtools" CACHE PATH "Location of psxtools")
set(PSXTOOLS_BIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../psxtools/build" CACHE PATH "Location of built psxtools")

add_subdirectory(third_party)

set(PSYQO_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/nugget/psyqo")

add_executable(game
  ${PSYQO_DIR}/src/alloc.c

  src/Trig.cpp
  src/Clip.cpp
  src/Utils.cpp
  src/Camera.cpp
  src/Model.cpp
  src/FastModel.cpp
  src/Renderer.cpp
  src/SoundPlayer.cpp
  src/Game.cpp
  src/main.cpp
)

target_compile_options(game PRIVATE
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
  # sadly GCC static analyzer is not yet very helpful
  # -fanalyzer
  # -Wno-analyzer-possible-null-dereference
  # -Wno-analyzer-null-argument
  # -Wno-analyzer-null-dereference
  # -Wno-analyzer-possible-null-argument
  # -Wno-unused-function
)

# TODO: try LTO and see how good it works
# But it can make code much easier to debug, so only enable in Release build
# set_target_properties(game PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

# target_link_libraries(game PRIVATE
#  -fanalyzer
#  -Wno-analyzer-possible-null-dereference
#  -Wno-analyzer-null-argument
#  -Wno-analyzer-null-dereference
#  -Wno-analyzer-possible-null-argument
#  -Wno-analyzer-out-of-bounds
#  -Wno-analyzer-allocation-size
#)

target_include_directories(game PRIVATE "${PSYQO_DIR}")

target_link_libraries(game PRIVATE
  nugget::nugget
  psyq::psyq
  EASTL::EASTL
)

find_program (
  MKPSXISO_EXECUTABLE
  NAMES
    mkpsxiso
  HINTS
    "${PROJECT_SOURCE_DIR}/tools/bin"
  REQUIRED
)

add_custom_target(build_iso
  ALL
  BYPRODUCTS game.ps-exe game.iso game.cue
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building game.iso"
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:game> "${CMAKE_CURRENT_BINARY_DIR}/game.ps-exe"
  COMMAND "${MKPSXISO_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cdlayout.xml" -y -q
)

set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets")
set(ASSETS_DIR_RAW "${PROJECT_SOURCE_DIR}/assets/raw")

set(models
  "${ASSETS_DIR}/raw/level.blend"
  "${ASSETS_DIR}/raw/house_psx.blend"
  "${ASSETS_DIR}/raw/human.blend"
  "${ASSETS_DIR}/raw/cato.blend"
)

set(fast_models
  "${ASSETS_DIR}/raw/roll.blend"
)

include(BuildAssets)

add_dependencies(build_iso game)
add_dependencies(build_iso assets)
