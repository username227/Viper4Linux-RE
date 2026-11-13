cmake_minimum_required(VERSION 3.20)
project(ViPERForLinux LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The upstream input is always copied, patched, and validated privately.
# Existing packaging can keep passing -DVIPERDSP_SOURCE_DIR while transitioning
# to a single CMake-owned preparation step.
set(VIPERDSP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ViPERDSP"
    CACHE PATH "Input directory containing upstream ViPERDSP source")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareViPERDSP.cmake")

# Upstream static DSP library, built as PIC for the LADSPA plugin.
add_subdirectory("${VIPERDSP_PREPARED_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ViPERDSP")
set_target_properties(ViPERDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(ViPERDSP PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/viperdsp_compat.h>")

find_package(Threads REQUIRED)
add_library(viper_dsp_ladspa MODULE src/viper-ladspa.cpp)
set_target_properties(viper_dsp_ladspa PROPERTIES PREFIX "" OUTPUT_NAME "viper_dsp_ladspa")
target_include_directories(viper_dsp_ladspa PRIVATE src)
target_link_libraries(viper_dsp_ladspa PRIVATE ViPERDSP Threads::Threads)
target_compile_options(viper_dsp_ladspa PRIVATE -Wall -Wextra -Wpedantic)

# About/build identity comes from a Git release tag or archive metadata.
# No version number is maintained manually in CMakeLists.txt.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake")
configure_file(src/viper-build-info.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/viper-build-info.h" @ONLY)

find_package(Qt6 6.4 COMPONENTS Widgets Network REQUIRED)
add_executable(viper-gui src/viper-gui.cpp resources/viper4linux-icons.qrc)
set_target_properties(viper-gui PROPERTIES AUTORCC ON)
target_include_directories(viper-gui PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(viper-gui PRIVATE Qt6::Widgets Qt6::Network)
# Explicit X11 startup completion is needed when the running GUI receives
# another launch request. Native Wayland continues to use Qt's compositor API.
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(viper-gui PRIVATE X11::X11)
  target_compile_definitions(viper-gui PRIVATE VIPER_HAVE_X11=1)
endif()
target_compile_options(viper-gui PRIVATE -Wall -Wextra -Wpedantic)

# Non-realtime native DSP snapshot transport for per-output settings.
# No Qt Widgets dependency and no work on PipeWire's audio thread.
add_executable(viper-output-state src/viper-output-state.cpp)
target_include_directories(viper-output-state PRIVATE src)
target_link_libraries(viper-output-state PRIVATE Qt6::Core)
target_compile_options(viper-output-state PRIVATE -Wall -Wextra -Wpedantic)

add_executable(viper-ctl src/viper-ctl.cpp)
target_include_directories(viper-ctl PRIVATE src)
target_link_libraries(viper-ctl PRIVATE Qt6::Core)
target_compile_options(viper-ctl PRIVATE -Wall -Wextra -Wpedantic)

# Standard CMake BUILD_TESTING defaults to ON. Every normal build compiles
# and registers the complete offline regression suite (including native mocks).
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# System package installation is intentionally opt-in during staging parity
# validation. It rejects direct host installs and unsupported prefixes.
option(VIPER_ENABLE_SYSTEM_INSTALL
  "Enable DESTDIR-only /usr package installation rules" OFF)
if(VIPER_ENABLE_SYSTEM_INSTALL)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallSystem.cmake")
endif()

# Developer source-checkout integration: prepare build-local routing files
# and expose explicit, non-installing verification and optional alias targets.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SourceCheckout.cmake")
