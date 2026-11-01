set(WIDGET_BODIES
  "${CMAKE_CURRENT_BINARY_DIR}/widget_owner_actual.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/widget_wrapper_actual.inc"
  "${CMAKE_CURRENT_BINARY_DIR}/widget_vector_actual.inc")
add_custom_command(OUTPUT ${WIDGET_BODIES}
  COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/extract_actual.py"
    "${FRAMEWORK}" "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS extract_actual.py
    "${FRAMEWORK}/adapters/mt2/native_step_runtime.inc"
    "${FRAMEWORK}/adapters/mt2/determinism_presentation_rng.inc"
    "${FRAMEWORK}/adapters/mt2/determinism_rng_vector_trace.inc"
  VERBATIM)
add_custom_target(mt2fw_widget_rng_bodies DEPENDS ${WIDGET_BODIES})
foreach(case scope owner wrapper vector)
  add_executable(mt2fw_widget_rng_${case}_tests "${case}_tests.cpp")
  add_dependencies(mt2fw_widget_rng_${case}_tests mt2fw_widget_rng_bodies)
  target_include_directories(mt2fw_widget_rng_${case}_tests PRIVATE
    "${FRAMEWORK}/adapters/mt2" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_options(mt2fw_widget_rng_${case}_tests PRIVATE /utf-8 /EHsc /UNDEBUG)
  add_test(NAME framework_widget_rng_${case} COMMAND mt2fw_widget_rng_${case}_tests)
  set_tests_properties(framework_widget_rng_${case} PROPERTIES TIMEOUT 10)
endforeach()
