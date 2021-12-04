file(GLOB SOURCES "*.c")

add_executable(
  altBridge
  ${SOURCES}
)

target_link_libraries(
  altBridge
  shared_c
)