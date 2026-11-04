# Prepare a private copy of the pinned ViPERDSP source for CMake builds.
# Never edit third_party/ViPERDSP or an externally supplied source directory.
# Reuse the prepared copy when upstream inputs and patch scripts are unchanged.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT IS_DIRECTORY "${VIPERDSP_SOURCE_DIR}" OR
   NOT EXISTS "${VIPERDSP_SOURCE_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${VIPERDSP_SOURCE_DIR}/viper/ViPER.cpp")
  message(FATAL_ERROR
    "ViPERDSP source is incomplete at '${VIPERDSP_SOURCE_DIR}'. "
    "Initialize the pinned submodule or use a unified release archive.")
endif()
file(REAL_PATH "${VIPERDSP_SOURCE_DIR}" _viper_source)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" _viper_binary)
set(_viper_prepared "${_viper_binary}/_viperdsp-prepared")

# Refuse a configuration that would copy a directory into itself or remove an
# upstream tree. The private destination is always a known build subdirectory.
cmake_path(IS_PREFIX _viper_source "${_viper_prepared}" NORMALIZE _viper_dest_inside_input)
cmake_path(IS_PREFIX _viper_prepared "${_viper_source}" NORMALIZE _viper_input_inside_dest)
if(_viper_dest_inside_input OR _viper_input_inside_dest OR
   IS_SYMLINK "${_viper_prepared}")
  message(FATAL_ERROR "Unsafe ViPERDSP source/destination overlap: '${_viper_source}' -> '${_viper_prepared}'")
endif()

# Git source checkouts must use their recorded submodule revision. An explicit
# source override (as used by the existing package scripts) and unified source
# archives remain supported; strict patch-context checks apply in all cases.
file(REAL_PATH "${PROJECT_SOURCE_DIR}/third_party/ViPERDSP" _viper_default_source)
if(EXISTS "${PROJECT_SOURCE_DIR}/.git" AND
   "${_viper_source}" STREQUAL "${_viper_default_source}")
  find_package(Git REQUIRED)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD:third_party/ViPERDSP
    RESULT_VARIABLE _viper_expected_result
    OUTPUT_VARIABLE _viper_expected OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${_viper_source}" rev-parse HEAD
    RESULT_VARIABLE _viper_actual_result
    OUTPUT_VARIABLE _viper_actual OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT _viper_expected_result EQUAL 0 OR NOT _viper_actual_result EQUAL 0 OR
     NOT "${_viper_expected}" STREQUAL "${_viper_actual}")
    message(FATAL_ERROR
      "ViPERDSP submodule revision mismatch. Expected '${_viper_expected}', "
      "found '${_viper_actual}'. Run git submodule update --init --checkout "
      "-- third_party/ViPERDSP before building.")
  endif()
endif()

set(_viper_patch_scripts
  "${PROJECT_SOURCE_DIR}/patches/fix_lufs_speed.py"
  "${PROJECT_SOURCE_DIR}/patches/preallocate_stereo_imager.py"
  "${PROJECT_SOURCE_DIR}/patches/preallocate_multiband_compressor.py")

# Hash the actual source content and the patch scripts, not their mtimes. A
# matching fingerprint avoids gratuitous native DSP recompiles on reconfigure.
# CONFIGURE_DEPENDS detects additions/removals; CMAKE_CONFIGURE_DEPENDS also
# catches edits to existing files between regular cmake --build invocations.
file(GLOB_RECURSE _viper_input_files CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE
  RELATIVE "${_viper_source}" "${_viper_source}/*")
list(SORT _viper_input_files)
set(_viper_fingerprint_data "")
foreach(_viper_relative IN LISTS _viper_input_files)
  if(_viper_relative MATCHES "^\\.git(/|$)")
    continue()
  endif()
  set(_viper_input_file "${_viper_source}/${_viper_relative}")
  if(IS_SYMLINK "${_viper_input_file}")
    message(FATAL_ERROR "Unexpected symlink in ViPERDSP sources: '${_viper_relative}'")
  endif()
  file(SHA256 "${_viper_input_file}" _viper_file_hash)
  string(APPEND _viper_fingerprint_data "${_viper_relative}:${_viper_file_hash}\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_viper_input_file}")
endforeach()
foreach(_viper_patch IN LISTS _viper_patch_scripts)
  if(NOT EXISTS "${_viper_patch}")
    message(FATAL_ERROR "Missing ViPERDSP patch script: ${_viper_patch}")
  endif()
  file(SHA256 "${_viper_patch}" _viper_patch_hash)
  string(APPEND _viper_fingerprint_data "${_viper_patch}:${_viper_patch_hash}\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_viper_patch}")
endforeach()
string(SHA256 _viper_fingerprint "${_viper_fingerprint_data}")
set(_viper_stamp "${_viper_prepared}/.viper4linux-source-sha256")
set(_viper_reprepare TRUE)
if(EXISTS "${_viper_stamp}" AND EXISTS "${_viper_prepared}/CMakeLists.txt")
  file(READ "${_viper_stamp}" _viper_previous_fingerprint)
  if(_viper_fingerprint STREQUAL _viper_previous_fingerprint)
    set(_viper_reprepare FALSE)
  endif()
endif()
if(_viper_reprepare)
  message(STATUS "Preparing private patched ViPERDSP source in '${_viper_prepared}'")
  file(REMOVE_RECURSE "${_viper_prepared}")
  file(MAKE_DIRECTORY "${_viper_prepared}")
  file(COPY "${_viper_source}/" DESTINATION "${_viper_prepared}"
       PATTERN ".git" EXCLUDE)
else()
  message(STATUS "Reusing verified private ViPERDSP source")
endif()

# Every configuration verifies the three exact patch signatures. The existing
# fail-closed Python scripts accept either the pristine or already-fixed form.
foreach(_viper_patch IN LISTS _viper_patch_scripts)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "VIPERDSP_SOURCE_DIR=${_viper_prepared}"
      "${Python3_EXECUTABLE}" "${_viper_patch}"
    RESULT_VARIABLE _viper_patch_result
    OUTPUT_VARIABLE _viper_patch_output
    ERROR_VARIABLE _viper_patch_error)
  if(NOT _viper_patch_result EQUAL 0)
    file(REMOVE "${_viper_stamp}")
    message(FATAL_ERROR
      "ViPERDSP patch verification failed for '${_viper_patch}':\n"
      "${_viper_patch_output}${_viper_patch_error}")
  endif()
  string(STRIP "${_viper_patch_output}" _viper_patch_output)
  message(STATUS "${_viper_patch_output}")
endforeach()
if(_viper_reprepare)
  # file(COPY) preserves upstream timestamps, which may be older than compiled
  # objects from a previous prepared tree. Touch refreshed inputs so build
  # tools reliably recompile changed source after a reconfigure.
  foreach(_viper_relative IN LISTS _viper_input_files)
    if(NOT _viper_relative MATCHES "^\\.git(/|$)")
      file(TOUCH "${_viper_prepared}/${_viper_relative}")
    endif()
  endforeach()
  file(WRITE "${_viper_stamp}" "${_viper_fingerprint}")
endif()
set(VIPERDSP_PREPARED_SOURCE_DIR "${_viper_prepared}")
