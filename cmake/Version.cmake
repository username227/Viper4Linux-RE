# Derive About/build identity without a manually maintained CMake version.
# A unified source archive carries its release version and commit revision;
# a Git checkout derives its base version from its nearest reachable vX.Y.Z tag.
get_filename_component(_v4l_source_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(V4L_BASE_VERSION "0.0.0")
set(V4L_REVISION "unknown")
find_package(Git QUIET)

if(EXISTS "${_v4l_source_root}/RELEASE-VERSION")
  file(STRINGS "${_v4l_source_root}/RELEASE-VERSION" _v4l_release_version LIMIT_COUNT 1)
  if(NOT _v4l_release_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+([.-][A-Za-z0-9.-]+)?$")
    message(FATAL_ERROR "Invalid RELEASE-VERSION: expected MAJOR.MINOR.PATCH")
  endif()
  set(V4L_BASE_VERSION "${_v4l_release_version}")
elseif(GIT_FOUND AND EXISTS "${_v4l_source_root}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match "v[0-9]*.[0-9]*.[0-9]*"
    WORKING_DIRECTORY "${_v4l_source_root}"
    OUTPUT_VARIABLE _v4l_nearest_tag OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _v4l_tag_result ERROR_QUIET)
  if(_v4l_tag_result EQUAL 0 AND
     _v4l_nearest_tag MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+([.-][A-Za-z0-9.-]+)?$")
    string(SUBSTRING "${_v4l_nearest_tag}" 1 -1 V4L_BASE_VERSION)
  endif()
endif()

if(GIT_FOUND AND EXISTS "${_v4l_source_root}/.git")
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${_v4l_source_root}"
    OUTPUT_VARIABLE V4L_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _v4l_rev_result ERROR_QUIET)
  if(NOT _v4l_rev_result EQUAL 0)
    set(V4L_REVISION "unknown")
  endif()
elseif(EXISTS "${_v4l_source_root}/RELEASE-REVISION")
  file(STRINGS "${_v4l_source_root}/RELEASE-REVISION" V4L_REVISION LIMIT_COUNT 1)
endif()
string(LENGTH "${V4L_REVISION}" _v4l_revision_length)
if(NOT V4L_REVISION MATCHES "^[0-9a-f]+$" OR
   _v4l_revision_length LESS 7 OR _v4l_revision_length GREATER 40)
  set(V4L_REVISION "unknown")
endif()
set(V4L_DEV_VERSION "${V4L_BASE_VERSION}-${V4L_REVISION}")
