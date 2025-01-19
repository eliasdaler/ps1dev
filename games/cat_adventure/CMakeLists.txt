cmake_minimum_required(VERSION 3.25)

project(
  cat_adventure
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)

add_executable(game
  ./src/Audio/MidiFile.cpp
  ./src/Audio/SongPlayer.cpp
  ./src/Audio/SoundPlayer.cpp
  ./src/Audio/VabFile.cpp

  ./src/Core/PadManager.cpp
  ./src/Core/StringHash.cpp
  ./src/Core/Timer.cpp
  
  ./src/Dev/DebugMenu.cpp

  ./src/Graphics/Armature.cpp
  ./src/Graphics/Font.cpp
  ./src/Graphics/Model.cpp
  ./src/Graphics/RainbowColors.cpp
  ./src/Graphics/Renderer.cpp
  ./src/Graphics/SkeletalAnimation.cpp
  ./src/Graphics/SkeletonAnimator.cpp
  ./src/Graphics/TimFile.cpp


  ./src/Math/Math.cpp
  ./src/Math/matrix_test.cpp
  ./src/Math/Quaternion.cpp
  ./src/Math/Transform.cpp

  ./src/UI/DialogueBox.cpp
  ./src/UI/TextLabel.cpp

  ./src/Util/Bouncer.cpp
  ./src/Util/FPSCounter.cpp

  ./src/Collision.cpp
  ./src/Object.cpp
  ./src/Level.cpp
  ./src/TileMap.cpp

  ./src/LoadingScene.cpp
  ./src/GameplayScene.cpp

  ./src/CDLoader.cpp
  ./src/Game.cpp
  ./src/main.cpp
)

target_include_directories(game PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
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
  -Wno-mismatched-new-delete
  # EASTL stuff
  -Wno-array-bounds
  -Wno-stringop-overread
  -Wno-tautological-compare
  -Wno-attributes
  -Wno-dangling-reference
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
  "${ASSETS_DIR_RAW}/cato.blend"
  "${ASSETS_DIR_RAW}/ash.blend"
  "${ASSETS_DIR_RAW}/house_psx.blend"
  "${ASSETS_DIR_RAW}/human2.blend"
  "${ASSETS_DIR_RAW}/level.blend"
)

if (BUILD_ASSETS) 
  include(BuildAssets)
  add_dependencies(build_iso assets)
endif()

add_dependencies(build_iso game)
add_dependencies(build_iso make_symlinks)
