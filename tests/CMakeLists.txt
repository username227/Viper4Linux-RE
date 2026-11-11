# Developer-only tests. Ordinary production builds never enter this directory.
# Legacy check-mock.sh remains a standalone full-suite fallback for packaging
# and existing CI while CTest consumes CMake-built mock executables.
find_program(VIPER_BASH_EXECUTABLE NAMES bash REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Never use the production IPC namespace or load the real ViPERDSP in mocks.
# -UNDEBUG preserves assertions in Release builds (unlike the CMake defaults).
set(_viper_test_include "${PROJECT_SOURCE_DIR}/src")
set(_viper_mock_include "${PROJECT_SOURCE_DIR}/tests/mock")
set(_viper_native_mock_targets)

add_library(viper_test_mock_ladspa MODULE
  "${PROJECT_SOURCE_DIR}/src/viper-ladspa.cpp"
  "${_viper_mock_include}/mock_viper.cpp")
set_target_properties(viper_test_mock_ladspa PROPERTIES
  PREFIX "" OUTPUT_NAME "viper_mock")
target_include_directories(viper_test_mock_ladspa PRIVATE
  "${_viper_test_include}" "${_viper_mock_include}")
target_compile_definitions(viper_test_mock_ladspa PRIVATE VIPER_MOCK_TEST_IPC)
target_compile_options(viper_test_mock_ladspa PRIVATE -O2 -UNDEBUG -Wall -Wextra)
target_link_libraries(viper_test_mock_ladspa PRIVATE Threads::Threads)
list(APPEND _viper_native_mock_targets viper_test_mock_ladspa)

add_executable(viper_test_mock_check "${_viper_mock_include}/check.cpp")
set_target_properties(viper_test_mock_check PROPERTIES OUTPUT_NAME "check")
target_include_directories(viper_test_mock_check PRIVATE
  "${_viper_test_include}" "${_viper_mock_include}")
target_compile_definitions(viper_test_mock_check PRIVATE VIPER_MOCK_TEST_IPC)
target_compile_options(viper_test_mock_check PRIVATE -O2 -UNDEBUG -Wall -Wextra)
target_link_libraries(viper_test_mock_check PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
list(APPEND _viper_native_mock_targets viper_test_mock_check)

# Standalone reader/EQ/CLI tests have the same sources and warning rules as
# their established g++ invocations in tests/check-mock.sh.
foreach(_viper_name IN ITEMS eq-presets ir-reader ddc-reader cli-batch)
  string(REPLACE "-" "_" _viper_target "viper_test_${_viper_name}")
  add_executable(${_viper_target} "check-${_viper_name}.cpp")
  set_target_properties(${_viper_target} PROPERTIES OUTPUT_NAME "check-${_viper_name}")
  target_include_directories(${_viper_target} PRIVATE "${_viper_test_include}")
  target_compile_options(${_viper_target} PRIVATE -O2 -UNDEBUG -Wall -Wextra -Werror)
  list(APPEND _viper_native_mock_targets ${_viper_target})
endforeach()

# These embed production CLI/output-state sources behind a mock-only IPC
# namespace and use the existing Qt6::Core dependency already found by CMake.
foreach(_viper_name IN ITEMS cli-controls cli-settings cli-eq-presets output-state)
  string(REPLACE "-" "_" _viper_target "viper_test_${_viper_name}")
  add_executable(${_viper_target} "check-${_viper_name}.cpp")
  set_target_properties(${_viper_target} PROPERTIES OUTPUT_NAME "check-${_viper_name}")
  target_include_directories(${_viper_target} PRIVATE "${_viper_test_include}")
  target_compile_definitions(${_viper_target} PRIVATE VIPER_MOCK_TEST_IPC)
  target_compile_options(${_viper_target} PRIVATE -O2 -UNDEBUG -Wall -Wextra -Werror)
  target_link_libraries(${_viper_target} PRIVATE Qt6::Core Threads::Threads)
  list(APPEND _viper_native_mock_targets ${_viper_target})
endforeach()

add_custom_target(viper_native_mock_tests DEPENDS ${_viper_native_mock_targets})

# CTest runs each existing regression group only once. The mock group uses
# prebuilt native binaries; build them before invoking CTest. The standalone
# check-developer.sh command still supports the original direct-compiler path.
foreach(_viper_group IN ITEMS package mock contracts release)
  if(_viper_group STREQUAL "mock")
    add_test(NAME viper_mock
      COMMAND "${CMAKE_COMMAND}" -E env
        "VIPER_MOCK_BIN_DIR=$<TARGET_FILE_DIR:viper_test_mock_check>"
        "${VIPER_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check-developer.sh" mock)
  else()
    add_test(NAME viper_${_viper_group}
      COMMAND "${VIPER_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/check-developer.sh" "${_viper_group}")
  endif()
  set_tests_properties(viper_${_viper_group} PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "developer;${_viper_group}")
endforeach()


# Real pinned native FET Compressor, not the synthetic mock engine. These
# 25 Linux functional cases guard the floating-point sidechain regression.
# Full Android PCM parity requires separately captured Android reference files.
add_executable(viper_test_fet_native check-fet-native.cpp)
target_include_directories(viper_test_fet_native PRIVATE
  "${VIPERDSP_PREPARED_SOURCE_DIR}")
target_link_libraries(viper_test_fet_native PRIVATE ViPERDSP)
target_compile_options(viper_test_fet_native PRIVATE
  -O2 -Wall -Wextra -Wpedantic -Werror)
add_test(NAME viper_fet_native COMMAND viper_test_fet_native)
set_tests_properties(viper_fet_native PROPERTIES
  LABELS "developer;native;fet")
