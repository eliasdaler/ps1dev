cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/Toolchain.cmake")

project(
  ps1_game
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # nugget uses "asm" GCC extension

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PSYQ_DIR "${CMAKE_CURRENT_LIST_DIR}/../psyq" CACHE PATH "Location of PsyQ libraries")

add_subdirectory(third_party)

set(PSYQO_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/nugget/psyqo")

add_executable(game
  ${PSYQO_DIR}/src/alloc.c

  src/Utils.cpp
  src/Camera.cpp
  src/Game.cpp
  src/main.cpp
)

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
)

if(MKPSXISO_EXECUTABLE-NOTFOUND)
  message(FATAL_ERROR "mkpsxiso was not found")
endif()

add_custom_command(TARGET game
  POST_BUILD
  BYPRODUCTS game.ps-exe game.iso game.cue
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building game.iso"
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:game> "${CMAKE_CURRENT_BINARY_DIR}/game.ps-exe"
  COMMAND ${MKPSXISO_EXECUTABLE} "${PROJECT_SOURCE_DIR}/cdlayout.xml" -y -q
)
