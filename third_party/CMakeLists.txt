set(NUGGET_PATH "${CMAKE_CURRENT_LIST_DIR}/nugget")

add_library(nugget OBJECT
  "${NUGGET_PATH}/common/crt0/cxxglue.c"
  "${NUGGET_PATH}/common/crt0/memory-c.c"
  "${NUGGET_PATH}/common/crt0/crt0cxx.s"
  "${NUGGET_PATH}/common/crt0/memory-s.s"
  "${NUGGET_PATH}/common/crt0/memory-s.s"
  "${NUGGET_PATH}/common/syscalls/printf.s"
)

target_include_directories(nugget PRIVATE
  ${NUGGET_PATH}
)

target_compile_options(nugget PRIVATE
  -fno-pic
  -mno-abicalls
)

target_include_directories(nugget INTERFACE
  "${NUGGET_PATH}/include"
)

target_link_options(nugget INTERFACE
  -T${NUGGET_PATH}/nooverlay.ld
  -T${NUGGET_PATH}/ps-exe.ld
)

add_library(nugget::nugget ALIAS nugget)

add_library(psyq INTERFACE)

if (NOT EXISTS "${PSYQ_DIR}")
  message(FATAL_ERROR "${PSYQ_DIR} doesn't exist - set the location of psyq libs via PSYQ_DIR parameter")
endif()

target_include_directories(psyq INTERFACE
  "${PSYQ_DIR}/include"
)

target_link_directories(psyq INTERFACE
  "${PSYQ_DIR}/lib"
)

set(PSYQ_LIBS
  api
  c
  c2
  card
  comb
  ds
  etc
  gpu
  gs
  gte
  gun
  hmd
  math
  mcrd
  mcx
  pad
  press
  sio
  snd
  spu
  tap
  cd
)

target_link_libraries(psyq INTERFACE
  "$<LINK_GROUP:RESCAN,${PSYQ_LIBS}>"
)


add_library(psyq::psyq ALIAS psyq)
