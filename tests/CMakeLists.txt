# BUILD_TESTING=ON registers the complete offline CTest suite by default:
# package checks, isolated native mocks, GUI/application contracts, and
# conditional release contracts. No live PipeWire or audio backend required.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# CTest is the primary developer test runner: report each independent check,
# rather than concealing dozens of checks inside four aggregate shell tests.
# The legacy all-in-one developer wrapper is retired after full-suite CTest
# validation. The standalone package and mock runners remain as needed;
# CTest directly registers each developer check independently.
function(viper_add_script_test _name _script _group)
  add_test(NAME "${_name}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${_script}")
  set_tests_properties("${_name}" PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "developer;${_group}")
endfunction()

# Same fast, offline package checks as check-package.sh (no live PipeWire).
foreach(_viper_script IN ITEMS
    check-packaging-contract
    check-launcher
    check-audio-restart
    check-arch-install
    check-cmake-source-checkout)
  string(REPLACE "-" "_" _viper_test_name "${_viper_script}")
  viper_add_script_test("viper_package_${_viper_test_name}" "${_viper_script}.py" package)
endforeach()

# Preserve the original unittest discovery pattern, but expose each test
# module independently. GLOB CONFIGURE_DEPENDS tracks future output modules.
file(GLOB _viper_package_unit_modules CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/test_output_*.py")
list(APPEND _viper_package_unit_modules
  "${CMAKE_CURRENT_SOURCE_DIR}/test_log_retention.py")
foreach(_viper_file IN LISTS _viper_package_unit_modules)
  get_filename_component(_viper_unit_module "${_viper_file}" NAME)
  get_filename_component(_viper_unit_name "${_viper_file}" NAME_WE)
  add_test(NAME "viper_package_${_viper_unit_name}"
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s tests -p "${_viper_unit_module}" -v)
  set_tests_properties("viper_package_${_viper_unit_name}" PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "developer;package")
endforeach()

# Native mock compilation and independent mock/contract CTest cases.
find_program(VIPER_BASH_EXECUTABLE NAMES bash REQUIRED)

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

# Runtime named-preset compatibility: isolated, disposable XDG data directory.
add_executable(viper_test_named_presets check-named-presets.cpp)
target_include_directories(viper_test_named_presets PRIVATE "${_viper_test_include}")
target_link_libraries(viper_test_named_presets PRIVATE Qt6::Core)
target_compile_options(viper_test_named_presets PRIVATE
  -O2 -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
list(APPEND _viper_native_mock_targets viper_test_named_presets)
add_test(NAME viper_contract_named_presets_runtime
  COMMAND "$<TARGET_FILE:viper_test_named_presets>")
set_tests_properties(viper_contract_named_presets_runtime PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "developer;contracts;presets")

# Exact native/legacy unit projections need no Qt or live DSP.
add_executable(viper_test_precision_units check-precision-units.cpp)
target_include_directories(viper_test_precision_units PRIVATE "${_viper_test_include}")
target_compile_options(viper_test_precision_units PRIVATE
  -O2 -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
list(APPEND _viper_native_mock_targets viper_test_precision_units)
add_test(NAME viper_native_precision_units
  COMMAND "$<TARGET_FILE:viper_test_precision_units>")
set_tests_properties(viper_native_precision_units PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "developer;native;settings")

add_custom_target(viper_native_mock_tests DEPENDS ${_viper_native_mock_targets})

# Exercise request-serial coalescing without starting a PipeWire process.
add_executable(viper_test_worker_request check-worker-request.cpp)
target_include_directories(viper_test_worker_request PRIVATE "${_viper_test_include}")
target_compile_options(viper_test_worker_request PRIVATE
  -O2 -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
add_test(NAME viper_native_worker_request
  COMMAND "$<TARGET_FILE:viper_test_worker_request>")
set_tests_properties(viper_native_worker_request PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "developer;native;worker")


# Static mock contracts run directly with Python. Native checks reuse their
# existing temp-config/mocked-IPC harness, selecting one check per CTest case.
foreach(_viper_script IN ITEMS
    check-dynamic-eq-ui-contract
    check-about-and-deq-handoff
    check-tray-contract
    check-native-effect-routing)
  string(REPLACE "-" "_" _viper_test_name "${_viper_script}")
  viper_add_script_test("viper_mock_${_viper_test_name}" "${_viper_script}.py" mock)
endforeach()

foreach(_viper_mock_case IN ITEMS
    eq-presets
    ir-reader
    ddc-reader
    native-effect-basic
    native-effect-precision
    cli-controls
    cli-batch
    cli-settings
    cli-eq-presets
    output-state)
  string(REPLACE "-" "_" _viper_test_name "${_viper_mock_case}")
  add_test(NAME "viper_mock_${_viper_test_name}"
    COMMAND "${CMAKE_COMMAND}" -E env
      "VIPER_MOCK_BIN_DIR=$<TARGET_FILE_DIR:viper_test_mock_check>"
      "VIPER_MOCK_SINGLE_CHECK=${_viper_mock_case}"
      "${VIPER_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check-mock.sh")
  set_tests_properties("viper_mock_${_viper_test_name}" PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "developer;mock")
endforeach()

# Build the tiny native activity-classification test once using CMake,
# instead of recompiling it as a side effect of every CTest run.
add_executable(viper_test_audio_activity check-audio-activity.cpp)
target_include_directories(viper_test_audio_activity PRIVATE "${_viper_test_include}")
target_compile_options(viper_test_audio_activity PRIVATE
  -O2 -UNDEBUG -Wall -Wextra -Werror)
add_test(NAME viper_contract_audio_activity
  COMMAND "$<TARGET_FILE:viper_test_audio_activity>")
set_tests_properties(viper_contract_audio_activity PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "developer;contracts")

foreach(_viper_script IN ITEMS
    check-native-state-contract
    check-precision-persistence-contract
    check-settings-schema-contract
    check-gui-sync-contract
    check-window-integration-contract
    check-version
    check-single-gui
    test_backend_lifecycle_logging
    check-gui-backend-exit
    check-theme-contract
    check-ui-contract
    check-named-presets-contract
    check-preset-reset-contract
    check-save-preset-choice-contract
    test_filter_idle_config)
  string(REPLACE "-" "_" _viper_test_name "${_viper_script}")
  viper_add_script_test("viper_contract_${_viper_test_name}" "${_viper_script}.py" contracts)
endforeach()

# Source archives may intentionally omit release-workflow definitions.
# Only register the corresponding contracts when the workflow is present.
if(EXISTS "${PROJECT_SOURCE_DIR}/.github/workflows/prepare-stable-release.yml")
  foreach(_viper_script IN ITEMS
      check-native-deb-workflow
      check-assembled-release
      check-stable-release)
    string(REPLACE "-" "_" _viper_test_name "${_viper_script}")
    viper_add_script_test("viper_release_${_viper_test_name}" "${_viper_script}.py" release)
  endforeach()
endif()


# Real pinned native FET Compressor, not the synthetic mock engine. These
# 25 Linux functional cases guard the floating-point sidechain regression.
# Full Android PCM parity requires separately captured Android reference files.
add_executable(viper_test_fet_native check-fet-native.cpp)
target_include_directories(viper_test_fet_native PRIVATE
  "${VIPERDSP_PREPARED_SOURCE_DIR}")
target_link_libraries(viper_test_fet_native PRIVATE ViPERDSP)
target_compile_options(viper_test_fet_native PRIVATE
  -O2 -Wall -Wextra -Wpedantic -Werror)
add_test(NAME viper_native_fet
  COMMAND "$<TARGET_FILE:viper_test_fet_native>")
set_tests_properties(viper_native_fet PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  LABELS "developer;native;fet")

# Real native Multiband initialization regression. The helper is the exact
# control-path code used by the LADSPA processor; not an Android PCM fixture.
add_executable(viper_test_multiband_init_native check-multiband-init-native.cpp)
target_include_directories(viper_test_multiband_init_native PRIVATE
  "${PROJECT_SOURCE_DIR}/src" "${VIPERDSP_PREPARED_SOURCE_DIR}")
target_link_libraries(viper_test_multiband_init_native PRIVATE ViPERDSP)
target_compile_options(viper_test_multiband_init_native PRIVATE
  -O2 -Wall -Wextra -Wpedantic -Werror)
add_test(NAME viper_native_multiband_init
  COMMAND "$<TARGET_FILE:viper_test_multiband_init_native>")
set_tests_properties(viper_native_multiband_init PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  LABELS "developer;native;multiband")
