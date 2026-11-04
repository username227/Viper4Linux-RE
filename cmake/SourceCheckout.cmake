# CMake source-checkout setup. Never installs system files or launches audio.
# Also usable with cmake -P for isolated checks; no shell installer required.

function(_viper_source_validate)
  if(NOT IS_DIRECTORY "${VIPER_SOURCE_ROOT}" OR
     NOT IS_DIRECTORY "${VIPER_SOURCE_BUILD_DIR}")
    message(FATAL_ERROR "Source and build directories must exist")
  endif()
  foreach(_name IN ITEMS viper-gui viper-ctl viper-output-state viper_dsp_ladspa.so)
    if(NOT EXISTS "${VIPER_SOURCE_BUILD_DIR}/${_name}" OR
       IS_DIRECTORY "${VIPER_SOURCE_BUILD_DIR}/${_name}")
      message(FATAL_ERROR "Missing source build output: ${VIPER_SOURCE_BUILD_DIR}/${_name}")
    endif()
    file(SIZE "${VIPER_SOURCE_BUILD_DIR}/${_name}" _size)
    if(_size EQUAL 0)
      message(FATAL_ERROR "Empty source build output: ${VIPER_SOURCE_BUILD_DIR}/${_name}")
    endif()
  endforeach()
  if(NOT EXISTS "${VIPER_SOURCE_ROOT}/viper4linux")
    message(FATAL_ERROR "Missing source launcher")
  endif()
  if(EXISTS "${VIPER_SOURCE_ROOT}/routing-config/filter-chain.conf")
    set(_routing "${VIPER_SOURCE_ROOT}/routing-config")
  else()
    set(_routing "${VIPER_SOURCE_BUILD_DIR}/routing-config")
  endif()
  if(NOT EXISTS "${_routing}/filter-chain.conf" OR
     NOT EXISTS "${_routing}/filter-chain.conf.d/99-viper-smart.conf")
    message(FATAL_ERROR "Source routing configuration missing in ${_routing}")
  endif()
  message(STATUS "Source checkout ready; routing config: ${_routing}")
endfunction()

function(_viper_source_link_commands)
  _viper_source_validate()
  if(NOT IS_ABSOLUTE "${VIPER_SOURCE_LINK_DIRECTORY}" OR
     NOT IS_ABSOLUTE "${VIPER_SOURCE_HOME}")
    message(FATAL_ERROR "Source command aliases need absolute private HOME and bin directory")
  endif()
  file(REAL_PATH "${VIPER_SOURCE_HOME}" _home)
  set(_dir "${VIPER_SOURCE_LINK_DIRECTORY}")
  cmake_path(NORMAL_PATH _dir)
  cmake_path(IS_PREFIX _home "${_dir}" NORMALIZE _in_home)
  if(NOT _in_home OR _dir STREQUAL _home)
    message(FATAL_ERROR "Refusing command links outside a private home subdirectory: ${_dir}")
  endif()
  set(_current "${_dir}")
  while(NOT _current STREQUAL _home)
    if(IS_SYMLINK "${_current}")
      message(FATAL_ERROR "Refusing symlinked command directory/ancestor: ${_current}")
    endif()
    cmake_path(GET _current PARENT_PATH _parent)
    if(_parent STREQUAL _current OR _parent STREQUAL "")
      message(FATAL_ERROR "Unable to validate command directory ancestors: ${_dir}")
    endif()
    set(_current "${_parent}")
  endwhile()
  foreach(_name IN ITEMS viper4linux v4l-cli)
    if(_name STREQUAL "viper4linux")
      set(_target "${VIPER_SOURCE_ROOT}/viper4linux")
    else()
      set(_target "${VIPER_SOURCE_BUILD_DIR}/viper-ctl")
    endif()
    set(_dest "${_dir}/${_name}")
    if(IS_SYMLINK "${_dest}")
      file(READ_SYMLINK "${_dest}" _existing)
      if(NOT _existing STREQUAL _target)
        message(FATAL_ERROR "Unrelated command symlink exists: ${_dest}")
      endif()
    elseif(EXISTS "${_dest}")
      message(FATAL_ERROR "Unrelated command already exists: ${_dest}")
    endif()
  endforeach()
  file(MAKE_DIRECTORY "${_dir}")
  if(IS_SYMLINK "${_dir}")
    message(FATAL_ERROR "Refusing symlinked destination directory: ${_dir}")
  endif()
  if(NOT IS_SYMLINK "${_dir}/viper4linux")
    file(CREATE_LINK "${VIPER_SOURCE_ROOT}/viper4linux" "${_dir}/viper4linux" SYMBOLIC RESULT _result)
    if(NOT _result STREQUAL "0")
      message(FATAL_ERROR "Failed to create source launcher alias: ${_result}")
    endif()
  endif()
  if(NOT IS_SYMLINK "${_dir}/v4l-cli")
    file(CREATE_LINK "${VIPER_SOURCE_BUILD_DIR}/viper-ctl" "${_dir}/v4l-cli" SYMBOLIC RESULT _result)
    if(NOT _result STREQUAL "0")
      message(FATAL_ERROR "Failed to create CLI alias: ${_result}")
    endif()
  endif()
  message(STATUS "Created/verified private source aliases in ${_dir}")
endfunction()

if(DEFINED VIPER_SOURCE_ACTION)
  if(VIPER_SOURCE_ACTION STREQUAL "check")
    _viper_source_validate()
  elseif(VIPER_SOURCE_ACTION STREQUAL "links")
    _viper_source_link_commands()
  else()
    message(FATAL_ERROR "Unknown source checkout action: ${VIPER_SOURCE_ACTION}")
  endif()
  return()
endif()

# Host source builds need a filter-chain template; distro package builds do not.
if(VIPER_ENABLE_SYSTEM_INSTALL)
  set(_viper_source_default OFF)
else()
  set(_viper_source_default ON)
endif()
option(VIPER_SOURCE_PREPARE_ROUTING
  "Prepare source-launcher PipeWire filter-chain config in the build directory" "${_viper_source_default}")
set(VIPER_SOURCE_ROUTING_TEMPLATE "/usr/share/pipewire/filter-chain.conf" CACHE FILEPATH
  "Stock PipeWire filter-chain template for source-checkout builds")
option(VIPER_ENABLE_SOURCE_LINK_TARGET
  "Enable explicit viper_source_links target for private ~/.local/bin aliases" OFF)

if(VIPER_SOURCE_PREPARE_ROUTING)
  if(NOT EXISTS "${VIPER_SOURCE_ROUTING_TEMPLATE}" OR IS_DIRECTORY "${VIPER_SOURCE_ROUTING_TEMPLATE}")
    message(FATAL_ERROR
      "Missing PipeWire source-routing template: ${VIPER_SOURCE_ROUTING_TEMPLATE}. "
      "Install PipeWire or set -DVIPER_SOURCE_PREPARE_ROUTING=OFF for packaging-only builds.")
  endif()
  set(_viper_routing_build_dir "${PROJECT_BINARY_DIR}/routing-config")
  file(MAKE_DIRECTORY "${_viper_routing_build_dir}/filter-chain.conf.d")
  configure_file("${VIPER_SOURCE_ROUTING_TEMPLATE}"
    "${_viper_routing_build_dir}/filter-chain.conf" COPYONLY)
  configure_file("${PROJECT_SOURCE_DIR}/routing-config/filter-chain.conf.d/99-viper-smart.conf"
    "${_viper_routing_build_dir}/filter-chain.conf.d/99-viper-smart.conf" COPYONLY)
  message(STATUS "Generated source routing configuration in ${_viper_routing_build_dir}")
endif()

# Deliberately outside ALL. A normal build doesn't start audio or install links.
add_custom_target(viper_source_check
  COMMAND "${CMAKE_COMMAND}"
    "-DVIPER_SOURCE_ACTION=check"
    "-DVIPER_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    "-DVIPER_SOURCE_BUILD_DIR=${PROJECT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_LIST_FILE}"
  DEPENDS viper-gui viper-ctl viper-output-state viper_dsp_ladspa
  VERBATIM COMMENT "Verify source-checkout runtime files (no audio started)")

if(VIPER_ENABLE_SOURCE_LINK_TARGET)
  if(DEFINED ENV{XDG_BIN_HOME} AND NOT "$ENV{XDG_BIN_HOME}" STREQUAL "")
    set(_viper_default_links "$ENV{XDG_BIN_HOME}")
  else()
    set(_viper_default_links "$ENV{HOME}/.local/bin")
  endif()
  set(VIPER_SOURCE_LINK_DIRECTORY "${_viper_default_links}" CACHE PATH
    "User-private command directory for explicit source alias creation")
  add_custom_target(viper_source_links
    COMMAND "${CMAKE_COMMAND}"
      "-DVIPER_SOURCE_ACTION=links"
      "-DVIPER_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
      "-DVIPER_SOURCE_BUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DVIPER_SOURCE_LINK_DIRECTORY=${VIPER_SOURCE_LINK_DIRECTORY}"
      "-DVIPER_SOURCE_HOME=$ENV{HOME}"
      -P "${CMAKE_CURRENT_LIST_FILE}"
    DEPENDS viper_source_check
    VERBATIM COMMENT "Opt-in: create only nonconflicting private source launcher aliases")
endif()
