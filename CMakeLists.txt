cmake_minimum_required(VERSION 3.20)
project(ViPERForLinux LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Builds apply downstream fixes to a private copy, never the tracked submodule.
set(VIPERDSP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ViPERDSP"
    CACHE PATH "Directory containing the upstream ViPERDSP CMakeLists.txt")
if (NOT EXISTS "${VIPERDSP_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "ViPERDSP source missing: initialize the submodule or use a unified source archive")
endif()

# Upstream static DSP library, built as PIC for the LADSPA plugin.
add_subdirectory("${VIPERDSP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ViPERDSP")
set_target_properties(ViPERDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(ViPERDSP PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/viperdsp_compat.h>")

find_package(Threads REQUIRED)
add_library(viper_dsp_ladspa MODULE src/viper-ladspa.cpp)
set_target_properties(viper_dsp_ladspa PROPERTIES PREFIX "" OUTPUT_NAME "viper_dsp_ladspa")
target_include_directories(viper_dsp_ladspa PRIVATE src)
target_link_libraries(viper_dsp_ladspa PRIVATE ViPERDSP Threads::Threads)
target_compile_options(viper_dsp_ladspa PRIVATE -Wall -Wextra -Wpedantic)

# Development identity follows a SemVer placeholder plus the Git revision.
# The Git checkout determines HEAD; source-only exports may use RELEASE-REVISION.
set(V4L_BASE_VERSION "1.0.0")
set(V4L_REVISION "unknown")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE V4L_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE V4L_GIT_RESULT ERROR_QUIET)
  if(NOT V4L_GIT_RESULT EQUAL 0)
    set(V4L_REVISION "unknown")
  endif()
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RELEASE-REVISION")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/RELEASE-REVISION" V4L_REVISION LIMIT_COUNT 1)
endif()
string(LENGTH "${V4L_REVISION}" V4L_REVISION_LENGTH)
if(NOT V4L_REVISION MATCHES "^[0-9a-f]+$"
   OR V4L_REVISION_LENGTH LESS 7 OR V4L_REVISION_LENGTH GREATER 40)
  set(V4L_REVISION "unknown")
endif()
set(V4L_DEV_VERSION "${V4L_BASE_VERSION}-${V4L_REVISION}")
configure_file(src/viper-build-info.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/viper-build-info.h" @ONLY)

find_package(Qt6 6.4 COMPONENTS Widgets REQUIRED)
add_executable(viper-gui src/viper-gui.cpp resources/viper4linux-icons.qrc)
set_target_properties(viper-gui PROPERTIES AUTORCC ON)
target_include_directories(viper-gui PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(viper-gui PRIVATE Qt6::Widgets)
target_compile_options(viper-gui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(viper-ctl src/viper-ctl.cpp)
target_include_directories(viper-ctl PRIVATE src)
target_compile_options(viper-ctl PRIVATE -Wall -Wextra -Wpedantic)
