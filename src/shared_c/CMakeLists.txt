include(FindALSA)

find_path(FLAC_INCLUDE_DIR NAMES FLAC/stream_decoder.h)
find_library(FLAC_LIBRARY NAMES FLAC)
if (${FLAC_INCLUDE_DIR} STREQUAL "FLAC_INCLUDE_DIR-NOTFOUND"
    OR ${FLAC_LIBRARY} STREQUAL "FLAC_LIBRARY-NOTFOUND")
  message (FATAL_ERROR "Flac library not found, install libflac-dev")
else()
  message (STATUS "Found FLAC: ${FLAC_LIBRARY}")
endif()

if (NOT ${ALSA_FOUND})
  message (FATAL_ERROR "Alsa library not found, install libasound2;libasound2-dev")
endif()
include_directories(
  ${ALSA_INCLUDE_DIR}
)

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
  ${FLAC_INCLUDE_DIR}
)

target_link_libraries(
  shared_c
  ${FLAC_LIBRARY}
  ${ALSA_LIBRARY}
)
