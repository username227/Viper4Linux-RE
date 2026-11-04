# Transitional test registration. Keep the proven isolated shell/Python suite
# intact while individual mock binaries are moved to native CMake test targets.
# This file is only reached when VIPER_BUILD_DEVELOPER_TESTS=ON.
find_program(VIPER_BASH_EXECUTABLE NAMES bash REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(NAME viper_developer_regressions
  COMMAND "${VIPER_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check-developer.sh")
set_tests_properties(viper_developer_regressions PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  LABELS "developer;regression")
