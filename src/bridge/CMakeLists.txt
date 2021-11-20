include(FindALSA)

if ( NOT ${ALSA_FOUND} )
  message ( FATAL_ERROR "Alsa libraries not found, install libasound2;libasound2-dev")
endif()

include_directories(${ALSA_INCLUDE_DIR})
file(GLOB SOURCES "*.c")

add_executable(
  altBridge
  ${SOURCES}
)

target_link_libraries(
  altBridge
  shared_c
  ${ALSA_LIBRARY}
)