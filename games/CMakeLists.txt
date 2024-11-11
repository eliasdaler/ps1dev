cmake_minimum_required(VERSION 3.25)

set(PS1DEV_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

list(APPEND CMAKE_MODULE_PATH "${PS1DEV_REPO_ROOT}/cmake")

set(CMAKE_TOOLCHAIN_FILE "${PS1DEV_REPO_ROOT}/cmake/PSXToolchain.cmake")

project(
  games
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # nugget uses "asm" GCC extension

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PSXTOOLS_BIN_DIR "${PS1DEV_REPO_ROOT}/build/tools" CACHE PATH "Location of built psxtools")

include(PsyqoSetup)

find_program (
  MKPSXISO_EXECUTABLE
  NAMES
    mkpsxiso
  HINTS
  "${PS1DEV_REPO_ROOT}/tools/bin"
  REQUIRED
)

add_subdirectory(cat_adventure)
