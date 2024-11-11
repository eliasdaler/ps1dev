cmake_minimum_required(VERSION 3.25)

project(
  ps1dev
  LANGUAGES C CXX ASM
  VERSION 1.0.0
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party)
add_subdirectory(tools)
