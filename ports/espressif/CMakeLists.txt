# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Check for -DBOARD=
if(NOT (DEFINED BOARD AND EXISTS "${CMAKE_SOURCE_DIR}/boards/${BOARD}/board.h") )
  file(GLOB BOARD_LIST RELATIVE ${CMAKE_SOURCE_DIR}/boards "boards/*/board.h")
  
  message("Please specify `-DBOARD=` with one of supported boards")
  foreach(board IN LISTS BOARD_LIST)
    string(REPLACE "/board.h" "" board ${board})
    message("- ${board}")
  endforeach()
  
  message(FATAL_ERROR "Invalid BOARD specified")
endif()

include(${CMAKE_SOURCE_DIR}/boards/${BOARD}/board.cmake)

# TOP is absolute path to root directory
set(TOP "../..")
get_filename_component(TOP "${TOP}" REALPATH)

set(EXTRA_COMPONENT_DIRS "${TOP}/src" boards)

set(SDKCONFIG_DEFAULTS sdkconfig.defaults boards/${BOARD}/sdkconfig)
set(SDKCONFIG ${CMAKE_BINARY_DIR}/sdkconfig)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

execute_process(COMMAND git describe --dirty --always --tags
                OUTPUT_VARIABLE GIT_VERSION
                ERROR_QUIET)
string(STRIP ${GIT_VERSION} GIT_VERSION)

execute_process(COMMAND git -C ${IDF_PATH} describe --dirty --always --tags
                OUTPUT_VARIABLE IDF_VERSION
                ERROR_QUIET)
string(STRIP ${IDF_VERSION} IDF_VERSION)

execute_process(COMMAND git -C ${IDF_PATH}/components/tinyusb/tinyusb describe --dirty --always --tags
                OUTPUT_VARIABLE TINYUSB_VERSION
                ERROR_QUIET)
string(STRIP ${TINYUSB_VERSION} TINYUSB_VERSION)

set(GIT_SUBMODULE_VERSIONS "IDF:${IDF_VERSION} - TinyUSB:${TINYUSB_VERSION}")

if(${CMAKE_VERSION} VERSION_LESS "3.12") 
    message("Please consider to switch to CMake above 3.12")
    add_definitions(-DUF2_VERSION_BASE="${GIT_VERSION}")
    add_definitions(-DUF2_VERSION="${GIT_VERSION} - ${GIT_SUBMODULE_VERSIONS}")
else()
    add_compile_definitions(UF2_VERSION_BASE="${GIT_VERSION}")
    add_compile_definitions(UF2_VERSION="${GIT_VERSION} - ${GIT_SUBMODULE_VERSIONS}")
endif()

project(tinyuf2)
