cmake_minimum_required(VERSION 3.22)
project(Player VERSION 0.1)

include(FetchContent)

if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
  message( FATAL_ERROR “In-source builds not allowed!” )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Werror)
set(CMAKE_C_CPPLINT "cpplint")
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

#
# Components
#

add_subdirectory("src/shared_c")
add_subdirectory("src/bridge")

#
# Tests
#

add_subdirectory("tests/shared_c")
enable_testing()
