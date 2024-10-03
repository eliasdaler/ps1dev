set(NUGGET_PATH "${CMAKE_CURRENT_LIST_DIR}/nugget")

add_library(nugget OBJECT
  "${NUGGET_PATH}/common/crt0/crt0cxx.s"
  "${NUGGET_PATH}/common/crt0/cxxglue.c"
  "${NUGGET_PATH}/common/crt0/memory-c.c"
  "${NUGGET_PATH}/common/crt0/memory-s.s"
  "${NUGGET_PATH}/common/syscalls/printf.s"
  "${NUGGET_PATH}/common/hardware/flushcache.s"
)

target_include_directories(nugget PRIVATE
  ${NUGGET_PATH}
)

target_include_directories(nugget INTERFACE
  "${NUGGET_PATH}"
  "${NUGGET_PATH}/include"
)

target_link_options(nugget INTERFACE
  -T${NUGGET_PATH}/nooverlay.ld
  -T${NUGGET_PATH}/ps-exe.ld
)

add_library(nugget::nugget ALIAS nugget)

add_library(psyq INTERFACE)

# EASTL comes with nugget
add_library(EASTL STATIC
  "${NUGGET_PATH}/third_party/EASTL/source/fixed_pool.cpp"
)

target_include_directories(EASTL PUBLIC
  "${NUGGET_PATH}/third_party/EABase/include/Common"
  "${NUGGET_PATH}/third_party/EASTL/include"
)

add_library(EASTL::EASTL ALIAS EASTL)

set(PSYQO_DIR "${CMAKE_CURRENT_LIST_DIR}/nugget/psyqo")
add_library(psyqo STATIC
  ${PSYQO_DIR}/src/alloc.c
  ${PSYQO_DIR}/src/xprintf.c
  ${PSYQO_DIR}/src/adler32.cpp
  ${PSYQO_DIR}/src/application.cpp
  ${PSYQO_DIR}/src/bezier.cpp
  ${PSYQO_DIR}/src/cdrom-device-cdda.cpp
  ${PSYQO_DIR}/src/cdrom-device-muteunmute.cpp
  ${PSYQO_DIR}/src/cdrom-device-readsectors.cpp
  ${PSYQO_DIR}/src/cdrom-device-reset.cpp
  ${PSYQO_DIR}/src/cdrom-device-toc.cpp
  ${PSYQO_DIR}/src/cdrom-device.cpp
  ${PSYQO_DIR}/src/cdrom.cpp
  ${PSYQO_DIR}/src/eastl-glue.cpp
  ${PSYQO_DIR}/src/fixed-point.cpp
  ${PSYQO_DIR}/src/font.cpp
  ${PSYQO_DIR}/src/gpu.cpp
  ${PSYQO_DIR}/src/iso9660-parser.cpp
  ${PSYQO_DIR}/src/kernel.cpp
  ${PSYQO_DIR}/src/msf.cpp
  ${PSYQO_DIR}/src/ordering-table.cpp
  ${PSYQO_DIR}/src/simplepad.cpp
  ${PSYQO_DIR}/src/soft-math.cpp
  ${PSYQO_DIR}/src/spu.cpp
  ${PSYQO_DIR}/src/task.cpp
  ${PSYQO_DIR}/src/trigonometry.cpp
  ${PSYQO_DIR}/src/vector.s
  ${PSYQO_DIR}/src/hardware/cdrom.cpp
  ${PSYQO_DIR}/src/hardware/cpu.cpp
  ${PSYQO_DIR}/src/hardware/gpu.cpp
  ${PSYQO_DIR}/src/hardware/sbus.cpp
)

target_include_directories(psyqo PUBLIC
  "${NUGGET_PATH}"
)

target_link_libraries(psyqo PUBLIC
  EASTL::EASTL
  nugget::nugget
)

add_library(psyqo::psyqo ALIAS psyqo)

target_compile_options(psyqo PUBLIC
  -Wno-attributes
  -Wno-switch
)

add_executable(torus_example
  ${PSYQO_DIR}/examples/torus/torus.cpp
)

target_link_libraries(torus_example PRIVATE
  nugget::nugget
  psyqo::psyqo
  EASTL::EASTL
)
