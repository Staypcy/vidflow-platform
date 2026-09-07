cmake_minimum_required(VERSION 3.10)

set("/usr/bin/gcc" CACHE FILEPATH "C complier" FORCE)
set("/usr/bin/g++" CACHE FILEPATH "C++ complier" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

project(
    vidflow
    VERSION 0.1.0
    LANGUAGES CXX C
)

include(GNUInstallDirs)
#include(CMakePackageConfigHelpers)

option(VIDFLOW_BUILD_EXAMPLES "build examples" ON)
option(VIDFLOW_BUILD_TESTS "build tests" ON)
option(VIDFLOW_BUILD_TOOLS "build tools" ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_subdirectory(src)
if(HTSERVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(HTSERVER_BUILD_TOOLS)
    add_subdirectory(tools)
endif()