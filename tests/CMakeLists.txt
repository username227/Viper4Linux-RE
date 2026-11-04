# Opt-in developer CTest groups. Each group is defined by the same dispatcher
# used by tests/check-developer.sh, so CI's legacy full-suite invocation keeps
# identical coverage without CTest running duplicate checks.
find_program(VIPER_BASH_EXECUTABLE NAMES bash REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

foreach(_viper_group IN ITEMS package mock contracts release)
  add_test(NAME viper_${_viper_group}
    COMMAND "${VIPER_BASH_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/check-developer.sh" "${_viper_group}")
  set_tests_properties(viper_${_viper_group} PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "developer;${_viper_group}")
endforeach()
