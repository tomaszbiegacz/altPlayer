include(FindOpenSSL)

if (${OPENSSL_FOUND})
  message (STATUS "Found OpenSSL: ${OPENSSL_LIBRARIES} (version ${OPENSSL_VERSION})")
else()
  message (FATAL_ERROR "OpenSSL library not found")
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

find_path(NGHTTP2_INCLUDE_DIR NAMES nghttp2/nghttp2.h)
find_library(NGHTTP2_LIBRARY NAMES nghttp2)
if (${NGHTTP2_INCLUDE_DIR} STREQUAL "NGHTTP2_INCLUDE_DIR-NOTFOUND"
    OR ${NGHTTP2_LIBRARY} STREQUAL "NGHTTP2_LIBRARY-NOTFOUND")
  message (FATAL_ERROR "Nghttp2 library not found, install libnghttp2-dev")
else()
  message (STATUS "Found Nghttp2: ${NGHTTP2_LIBRARY}")
endif()


file(GLOB SOURCES "*.c")

add_executable(
  altBridge
  ${SOURCES}
)

target_link_libraries(
  altBridge
  shared_c
  ${OPENSSL_LIBRARIES}
  ${NGHTTP2_LIBRARY}
  ${EVENT_LIBRARY}
  ${EVENTOPENSSL_LIBRARY}
)