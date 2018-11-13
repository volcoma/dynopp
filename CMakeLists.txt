cmake_minimum_required(VERSION 3.4.3)

project (dynopp)
include(CTest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/utility")

option(BUILD_DYNOPP_SHARED "Build as a shared library." ON)
option(BUILD_DYNOPP_TESTS "Build the tests" ON)
option(BUILD_DYNOPP_WITH_CODE_STYLE_CHECKS "Build with code style checks." OFF)

if(BUILD_DYNOPP_SHARED)
	set(BUILD_SHARED_LIBS ON)
endif()

if(BUILD_DYNOPP_WITH_CODE_STYLE_CHECKS)
	set(BUILD_WITH_CODE_STYLE_CHECKS ON)
endif()

add_subdirectory(3rdparty)
add_subdirectory(dynopp)

if(BUILD_DYNOPP_TESTS)
	add_subdirectory(tests)
endif()
