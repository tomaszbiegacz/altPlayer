include(FetchContent)

function(download_test_asset url file_name)
  if (NOT EXISTS ${CMAKE_BINARY_DIR}/${file_name})
    message(STATUS "Downloading [${url}] into ${file_name}")
    file(
      DOWNLOAD ${url}
      ${CMAKE_BINARY_DIR}/${file_name} SHOW_PROGRESS)
  endif()
endfunction()

# https://github.com/google/googletest/releases/tag/release-1.11.0
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE TEST_SOURCES "*.cc")

add_executable(
  tests_shared_c
  ${TEST_SOURCES}
)

target_link_libraries(
  tests_shared_c
  gtest_main
  shared_c
)

download_test_asset("https://drive.google.com/uc?id=1fFgR6ndtY0bNkTkLYEPI3AkKjUqbsq98" test.wav)
download_test_asset("https://drive.google.com/uc?id=1AV6XvUJD8s0bKXLaVHk1La6r1Xn2Qzji" test.flac)

include(GoogleTest)
gtest_discover_tests(tests_shared_c)
