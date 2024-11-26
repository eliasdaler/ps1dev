cmake_minimum_required(VERSION 3.25)

project(
  cat_adventure
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

add_executable(game
  src/matrix_test.cpp

  src/Timer.cpp
  src/Bouncer.cpp

  src/Model.cpp
  src/TimFile.cpp
  src/MidiFile.cpp
  src/Font.cpp
  src/Renderer.cpp
  src/VabFile.cpp
  src/DialogueBox.cpp

  src/Game.cpp

  src/GameplayScene.cpp
  src/LoadingScene.cpp

  src/SoundPlayer.cpp

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
  -Wno-attributes
)

# TODO: try LTO and see how good it works
# But it can make code much easier to debug, so only enable in Release build
# set_target_properties(game PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

target_link_libraries(game PRIVATE
  psyqo::psyqo
)

set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets")
set(ASSETS_DIR_RAW "${PROJECT_SOURCE_DIR}/assets/raw")

# mkpsxiso is not very nice 
# It assumes that the current work dir = location of cdlayout.xml
# So we need to make a bunch of symlinks inside ${CMAKE_CURRENT_BINARY_DIR} for that
add_custom_target(make_symlinks
  ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink  "${ASSETS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E create_symlink  "${PROJECT_SOURCE_DIR}/system.cnf" "${CMAKE_CURRENT_BINARY_DIR}/system.cnf"
  COMMAND ${CMAKE_COMMAND} -E create_symlink  "${PROJECT_SOURCE_DIR}/cdlayout.xml" "${CMAKE_CURRENT_BINARY_DIR}/cdlayout.xml"
)

add_custom_target(build_iso
  ALL
  BYPRODUCTS game.ps-exe game.iso game.cue
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Building game.iso"
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:game> "${CMAKE_CURRENT_BINARY_DIR}/game.ps-exe"
  COMMAND "${MKPSXISO_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/cdlayout.xml" -y -q
)

set(models
  "${ASSETS_DIR_RAW}/level.blend"
  "${ASSETS_DIR_RAW}/house_psx.blend"
  "${ASSETS_DIR_RAW}/level_new.blend"
  "${ASSETS_DIR_RAW}/human.blend"
  "${ASSETS_DIR_RAW}/cato.blend"
  "${ASSETS_DIR_RAW}/car.blend"
)

if (BUILD_ASSETS) 
  include(BuildAssets)
  add_dependencies(build_iso assets)
endif()

add_dependencies(build_iso game)
add_dependencies(build_iso make_symlinks)
