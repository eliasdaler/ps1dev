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
set(MODELS_BUILD_DIR_JSON "${ASSETS_DIR}/models_json")
file(MAKE_DIRECTORY "${MODELS_BUILD_DIR_JSON}")

function (add_build_model_command MODEL_PATH FAST_MODEL)
  get_filename_component(MODEL_FILENAME "${MODEL_PATH}" NAME_WLE)
  set(CONVERTED_MODEL_PATH "${MODELS_BUILD_DIR_JSON}/${MODEL_FILENAME}.json")
  set(FINAL_MODEL_EXT "$<IF:$<BOOL:${FAST_MODEL}>,fm,bin>")
  set(FINAL_MODEL_PATH "${ASSETS_DIR}/${MODEL_FILENAME}.${FINAL_MODEL_EXT}")

  # Convert from .blend to .json
  add_custom_command(
    COMMENT "Converting ${MODEL_PATH} to ${CONVERTED_MODEL_PATH}"
    DEPENDS "${MODEL_PATH}"
    OUTPUT "${CONVERTED_MODEL_PATH}"
    # TODO: (When Blender 4.3.0 releases) add --quiet option
    COMMAND "${BLENDER_EXECUTABLE}" "${MODEL_PATH}" --python-exit-code 1 --background --python ${MODEL_BLENDER_SCRIPT} -- "${CONVERTED_MODEL_PATH}"
  )
  # Convert from .json to .bin/.fm
  add_custom_command(
    COMMENT "Converting ${CONVERTED_MODEL_PATH} to ${FINAL_MODEL_PATH}"
    DEPENDS "${CONVERTED_MODEL_PATH}"
    OUTPUT "${FINAL_MODEL_PATH}"
    COMMAND "${MODEL_CONVERTER_EXECUTABLE}" $<$<BOOL:${FAST_MODEL}>:--fast-model> "${CONVERTED_MODEL_PATH}" "${FINAL_MODEL_PATH}" "${ASSETS_DIR_RAW}"
  )
  return (PROPAGATE FINAL_MODEL_PATH)
endfunction()

foreach (MODEL_PATH ${models})
  add_build_model_command(${MODEL_PATH} FALSE)
  list(APPEND CONVERTED_MODELS "${FINAL_MODEL_PATH}")
endforeach()

foreach (MODEL_PATH ${fast_models})
  add_build_model_command(${MODEL_PATH} TRUE)
  list(APPEND CONVERTED_MODELS "${FINAL_MODEL_PATH}")
endforeach()

add_custom_target(models DEPENDS "${CONVERTED_MODELS}")
add_dependencies(assets models)
