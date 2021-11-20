file(GLOB SOURCES "*.c")

add_library(
  shared_c
  STATIC
  ${SOURCES}
)

target_include_directories(
  shared_c
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)