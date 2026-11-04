# Staging-only system package layout. Installed launchers hardcode /usr paths,
# so arbitrary prefix installs must not be advertised until they are supported.
# This module is opt-in and deliberately does not replace the existing packager
# script until the two staging trees have been compared by the maintainer.
include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
  message(FATAL_ERROR
    "Viper4Linux system package staging requires -DCMAKE_INSTALL_PREFIX=/usr. "
    "The installed launchers currently use fixed /usr paths.")
endif()
if(NOT CMAKE_INSTALL_BINDIR STREQUAL "bin" OR
   NOT CMAKE_INSTALL_DATADIR STREQUAL "share" OR
   NOT CMAKE_INSTALL_MANDIR STREQUAL "share/man")
  message(FATAL_ERROR
    "Viper4Linux package staging currently requires GNUInstallDirs bin, "
    "share and share/man paths for parity with the existing Arch package.")
endif()

set(VIPER_SYSTEM_PACKAGE_NAME "viper4linux-re" CACHE STRING
    "Package name for the staged Arch-style /usr/share/licenses directory")
if(NOT VIPER_SYSTEM_PACKAGE_NAME MATCHES "^[a-z0-9][a-z0-9@._+-]*$")
  message(FATAL_ERROR "Unsafe VIPER_SYSTEM_PACKAGE_NAME: '${VIPER_SYSTEM_PACKAGE_NAME}'")
endif()

file(GLOB _viper_runtime_python CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/viper_*.py")
list(LENGTH _viper_runtime_python _viper_runtime_count)
if(_viper_runtime_count LESS 5)
  message(FATAL_ERROR "Incomplete runtime Python module set in src/viper_*.py")
endif()

# The first install rule rejects host installs and unsafe staging roots before
# installing even one file. Existing Arch/deb package scripts remain untouched.
install(CODE [[
  set(_viper_destdir "$ENV{DESTDIR}")
  if(NOT IS_ABSOLUTE "${_viper_destdir}")
    message(FATAL_ERROR
      "DESTDIR must name an absolute private package staging directory; "
      "refusing to install to the host.")
  endif()
  cmake_path(NORMAL_PATH _viper_destdir)
  if(_viper_destdir STREQUAL "/" OR
     _viper_destdir STREQUAL "/usr" OR
     _viper_destdir STREQUAL "/usr/local")
    message(FATAL_ERROR "Unsafe DESTDIR '${_viper_destdir}'; refusing host installation.")
  endif()
  if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "Packaged launchers require --prefix /usr.")
  endif()
]])

# Keep build outputs named viper-gui/viper-ctl/viper-output-state for source
# users, but stage the existing package command names without modifying them.
install(PROGRAMS "$<TARGET_FILE:viper-gui>"
  DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME "viper4linux-gui")
install(PROGRAMS "$<TARGET_FILE:viper-ctl>"
  DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME "viper4linux-ctl")
install(PROGRAMS "$<TARGET_FILE:viper-output-state>"
  DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME "viper4linux-output-state")
install(PROGRAMS "$<TARGET_FILE:viper_dsp_ladspa>"
  DESTINATION "lib/ladspa")

install(FILES ${_viper_runtime_python} DESTINATION "lib/viper4linux")
install(PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/arch/start-backend"
  DESTINATION "lib/viper4linux")
install(PROGRAMS
  "${PROJECT_SOURCE_DIR}/packaging/arch/viper4linux"
  "${PROJECT_SOURCE_DIR}/packaging/arch/viper4linux-output"
  DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/routing-config/filter-chain.conf.d/99-viper-smart.conf"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/viper4linux/routing-config/filter-chain.conf.d")
install(FILES "${PROJECT_SOURCE_DIR}/packaging/arch/viper4linux.desktop"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${PROJECT_SOURCE_DIR}/resources/viper4linux-blue.png"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
  RENAME "viper4linux.png")
install(FILES "${PROJECT_SOURCE_DIR}/resources/viper4linux-blue.svg"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
  RENAME "viper4linux.svg")
install(FILES
  "${PROJECT_SOURCE_DIR}/man/viper4linux.1"
  "${PROJECT_SOURCE_DIR}/man/viper4linux-ctl.1"
  DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${VIPER_SYSTEM_PACKAGE_NAME}")
install(FILES "${PROJECT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${VIPER_SYSTEM_PACKAGE_NAME}"
  RENAME "ViPERDSP-NOTICE.md")
install(FILES "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/viper4linux")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/docs/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/viper4linux/docs")

# Relative symlinks survive package staging. Refuse conflicting destinations
# instead of replacing unrelated files in an existing staging directory.
install(CODE [[
  foreach(_viper_link IN ITEMS
      "bin/v4l-cli|viper4linux-ctl"
      "share/man/man1/v4l-cli.1|viper4linux-ctl.1")
    string(REPLACE "|" ";" _viper_link_parts "${_viper_link}")
    list(GET _viper_link_parts 0 _viper_relative)
    list(GET _viper_link_parts 1 _viper_target)
    set(_viper_link_path "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${_viper_relative}")
    if(IS_SYMLINK "${_viper_link_path}")
      file(READ_SYMLINK "${_viper_link_path}" _viper_existing)
      if(NOT _viper_existing STREQUAL _viper_target)
        message(FATAL_ERROR "Conflicting package symlink: ${_viper_link_path}")
      endif()
    elseif(EXISTS "${_viper_link_path}")
      message(FATAL_ERROR "Conflicting package file: ${_viper_link_path}")
    else()
      file(CREATE_LINK "${_viper_target}" "${_viper_link_path}" SYMBOLIC)
    endif()
  endforeach()
]])
