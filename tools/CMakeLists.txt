cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(
  psxtools_common
  VERSION 0.1.0
  LANGUAGES CXX
)

add_library(psxtools_common STATIC
  common/ImageLoader.cpp
)

target_include_directories(psxtools_common PUBLIC "${CMAKE_CURRENT_LIST_DIR}/common")
target_link_libraries(psxtools_common PRIVATE
  stb::image
)

add_library(psxtools::common ALIAS psxtools_common)

project(
  obj2psx
  VERSION 0.1.0
  LANGUAGES CXX
)

add_executable(model_converter
  model_converter/src/PsxModel.cpp
  model_converter/src/ModelJsonFile.cpp
  model_converter/src/Json2PsxConverter.cpp
  model_converter/src/AnimationWriter.cpp
  model_converter/src/LevelJsonFile.cpp
  model_converter/src/LevelWriter.cpp
  model_converter/src/main.cpp
)

target_link_libraries(model_converter PRIVATE
  psxtools::common
  nlohmann_json::nlohmann_json
  glm::glm
  CLI11::CLI11
)

# glm
target_compile_definitions(model_converter
  PUBLIC
    GLM_FORCE_CTOR_INIT
    GLM_FORCE_XYZW_ONLY
    GLM_FORCE_EXPLICIT_CTOR
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)

project(
  timtool
  VERSION 0.1.0
  LANGUAGES CXX
)

add_executable(timtool
  timtool/src/Font.cpp
  timtool/src/TimCreateConfig.cpp
  timtool/src/TimFileRead.cpp
  timtool/src/TimFileWrite.cpp
  timtool/src/TimFileCreator.cpp
  timtool/src/main.cpp
)

target_link_libraries(timtool PRIVATE
  psxtools::common
  stb::image
  nlohmann_json::nlohmann_json
  CLI11::CLI11
  freetype::freetype
  glm::glm
)

# TODO: CMake 3.26 - use ImageMagick::Magick++

find_package(ImageMagick COMPONENTS Magick++)
if(ImageMagick_FOUND)
  target_include_directories(timtool PRIVATE ${ImageMagick_INCLUDE_DIRS})
  target_link_libraries(timtool PRIVATE ${ImageMagick_LIBRARIES})
  target_compile_definitions(timtool PRIVATE
    -DMAGICKCORE_QUANTUM_DEPTH=16 
    -DMAGICKCORE_HDRI_ENABLE=0 
    -DQUANT_SUPPORT
  )
endif()
