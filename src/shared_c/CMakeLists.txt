include(FindALSA)

if (${ALSA_FOUND})
  message (STATUS "Found ALSA: ${ALSA_LIBRARY}")
else()
  message (FATAL_ERROR "Alsa library not found, install libasound2-dev")
endif()

find_path(EVENT_INCLUDE_DIR NAMES event2/event.h)
find_library(EVENT_LIBRARY NAMES event)
find_library(EVENTOPENSSL_LIBRARY NAMES event_openssl)
if (${EVENT_INCLUDE_DIR} STREQUAL "EVENT_INCLUDE_DIR-NOTFOUND"
    OR ${EVENT_LIBRARY} STREQUAL "EVENT_LIBRARY-NOTFOUND"
    OR ${EVENTOPENSSL_LIBRARY} STREQUAL "EVENTOPENSSL_LIBRARY-NOTFOUND")
  message (FATAL_ERROR "event library not found, install libevent-dev")
else()
  message (STATUS "Found event: ${EVENT_LIBRARY};${EVENTOPENSSL_LIBRARY}")
endif()

find_path(FLAC_INCLUDE_DIR NAMES FLAC/stream_decoder.h)
find_library(FLAC_LIBRARY NAMES FLAC)
if (${FLAC_INCLUDE_DIR} STREQUAL "FLAC_INCLUDE_DIR-NOTFOUND"
    OR ${FLAC_LIBRARY} STREQUAL "FLAC_LIBRARY-NOTFOUND")
  message (FATAL_ERROR "Flac library not found, install libflac-dev")
else()
  message (STATUS "Found FLAC: ${FLAC_LIBRARY}")
endif()

find_path(URIPARSER_INCLUDE_DIR NAMES uriparser/Uri.h)
find_library(URIPARSER_LIBRARY NAMES uriparser)
if (${URIPARSER_INCLUDE_DIR} STREQUAL "URIPARSER_INCLUDE_DIR-NOTFOUND"
    OR ${URIPARSER_LIBRARY} STREQUAL "URIPARSER_LIBRARY-NOTFOUND")
  message (FATAL_ERROR "uriparser library not found, install liburiparser-dev")
else()
  message (STATUS "Found uriparser: ${URIPARSER_LIBRARY}")
endif()

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
  ${ALSA_LIBRARY}
  ${EVENT_LIBRARY}
  ${FLAC_LIBRARY}
  ${URIPARSER_LIBRARY}
)
