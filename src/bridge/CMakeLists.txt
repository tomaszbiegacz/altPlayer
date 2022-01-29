include(FindOpenSSL)

if (${OPENSSL_FOUND})
  message (STATUS "Found OpenSSL: ${OPENSSL_LIBRARIES} (version ${OPENSSL_VERSION})")
else()
  message (FATAL_ERROR "OpenSSL library not found")
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
  ${EVENTOPENSSL_LIBRARY}
)