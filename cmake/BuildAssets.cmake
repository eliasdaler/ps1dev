add_custom_target(assets)

find_program (
  TIMTOOL_EXECUTABLE
  NAMES
    timtool
  HINTS
    "${PSXTOOLS_BIN_DIR}"
  REQUIRED
)

add_custom_target(tims
  WORKING_DIRECTORY "${ASSETS_DIR}"
  COMMENT "Building tims"
  COMMAND ${TIMTOOL_EXECUTABLE} "${ASSETS_DIR}/tims.json"
)

add_dependencies(assets tims)

find_program (
  BLENDER_EXECUTABLE
  NAMES
    blender
  REQUIRED
)

find_program (
  MODEL_CONVERTER_EXECUTABLE
  NAMES
    model_converter
  HINTS
    "${PSXTOOLS_BIN_DIR}"
  REQUIRED
)

set(MODEL_BLENDER_SCRIPT "${PSXTOOLS_DIR}/blender/json_export.py")
set(MODELS_BUILD_DIR_TEMP "${CMAKE_CURRENT_BINARY_DIR}/models_tmp")
file(MAKE_DIRECTORY "${MODELS_BUILD_DIR_TEMP}")

foreach (MODEL_PATH ${models})
  get_filename_component(MODEL_FILENAME "${MODEL_PATH}" NAME_WLE)
  set(CONVERTED_MODEL_PATH "${MODELS_BUILD_DIR_TEMP}/${MODEL_FILENAME}.json")
  set(FINAL_MODEL_PATH "${ASSETS_DIR}/${MODEL_FILENAME}.bin")
  add_custom_command(
    COMMENT "Converting ${MODEL_PATH} to ${FINAL_MODEL_PATH}"
    DEPENDS "${MODEL_PATH}"
    OUTPUT "${FINAL_MODEL_PATH}"
    # Convert from .blend to .json
    # TODO: (When Blender 4.3.0 releases) add --quiet option
    COMMAND "${BLENDER_EXECUTABLE}" "${MODEL_PATH}" --python-exit-code 1 --background --python ${MODEL_BLENDER_SCRIPT} -- "${CONVERTED_MODEL_PATH}"
    # Convert from .json to .bin
    COMMAND "${MODEL_CONVERTER_EXECUTABLE}" "${CONVERTED_MODEL_PATH}" "${FINAL_MODEL_PATH}" "${ASSETS_DIR_RAW}"
  )
  list(APPEND CONVERTED_MODELS "${FINAL_MODEL_PATH}")
endforeach()

add_custom_target(models DEPENDS "${CONVERTED_MODELS}")
add_dependencies(assets models)
