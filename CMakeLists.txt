cmake_minimum_required(VERSION 3.20)
project(ViPERForLinux LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ViPERDSP/CMakeLists.txt")
  message(FATAL_ERROR "ViPERDSP missing: run ./install.sh to fetch the pinned upstream revision")
endif()

# Upstream static DSP library, built as PIC for the LADSPA plugin.
add_subdirectory(third_party/ViPERDSP)
set_target_properties(ViPERDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(ViPERDSP PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/viperdsp_compat.h>")

find_package(Threads REQUIRED)
add_library(viper_dsp_ladspa MODULE src/viper-ladspa.cpp)
set_target_properties(viper_dsp_ladspa PROPERTIES PREFIX "" OUTPUT_NAME "viper_dsp_ladspa")
target_include_directories(viper_dsp_ladspa PRIVATE src)
target_link_libraries(viper_dsp_ladspa PRIVATE ViPERDSP Threads::Threads)
target_compile_options(viper_dsp_ladspa PRIVATE -Wall -Wextra -Wpedantic)

find_package(Qt6 6.4 COMPONENTS Widgets REQUIRED)
add_executable(viper-gui src/viper-gui.cpp resources/viper4linux-icons.qrc)
set_target_properties(viper-gui PROPERTIES AUTORCC ON)
target_include_directories(viper-gui PRIVATE src)
target_link_libraries(viper-gui PRIVATE Qt6::Widgets)
target_compile_options(viper-gui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(viper-ctl src/viper-ctl.cpp)
target_include_directories(viper-ctl PRIVATE src)
target_compile_options(viper-ctl PRIVATE -Wall -Wextra -Wpedantic)
