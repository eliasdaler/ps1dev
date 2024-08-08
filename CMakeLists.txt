cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/Toolchain.cmake")

project(
  ps1_game
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

set(PSYQ_DIR "${CMAKE_CURRENT_LIST_DIR}/../psyq" CACHE PATH "Location of PsyQ libraries")

add_subdirectory(third_party)

add_executable(game
  src/Game.cpp
  src/main.cpp
)

target_link_libraries(game PRIVATE
  nugget::nugget
  psyq::psyq
)

target_compile_options(game PRIVATE
  -ffunction-sections
  -fdata-sections
  -mno-gpopt
  -fomit-frame-pointer
  -fno-builtin
  -fno-strict-aliasing
  -Wno-attributes
  -march=mips1
  -mabi=32
  -EL
  -fno-pic
  -mno-abicalls
  -mfp32
  -mno-llsc
  -fno-stack-protector
  -ffreestanding
  -g
  -Os
  -fno-exceptions
  -fno-rtti
)

target_link_options(game PRIVATE
  -static
  -nostdlib
  -Wl,--gc-sections
  -Wl,--oformat=elf32-tradlittlemips
)

add_custom_command(TARGET game
  POST_BUILD
  COMMENT "Making game.ps-exe..."
  COMMAND
    ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:game> game.ps-exe
)

