add_subdirectory(stb)
add_subdirectory(glm)

## json
option(JSON_MultipleHeaders "Use non-amalgamated version of the library." ON)
option(JSON_Install         "Install CMake targets during install step." OFF)
add_subdirectory(json)

## CLI11
add_subdirectory(CLI11)

## freetype
option(FT_DISABLE_ZLIB
  "Disable use of system zlib and use internal zlib library instead." ON)
option(FT_DISABLE_BZIP2
  "Disable support of bzip2 compressed fonts." ON)
option(FT_DISABLE_BROTLI
  "Disable support of compressed WOFF2 fonts." ON)
option(FT_DISABLE_HARFBUZZ
  "Disable HarfBuzz (used for improving auto-hinting of OpenType fonts)." ON)
option(FT_DISABLE_PNG
  "Disable support of PNG compressed OpenType embedded bitmaps." ON)
option(SKIP_INSTALL_ALL "Skip install all" ON)
option(FT_ENABLE_ERROR_STRINGS
  "Enable support for meaningful error descriptions." ON)
add_subdirectory(freetype)
add_library(freetype::freetype ALIAS freetype)

