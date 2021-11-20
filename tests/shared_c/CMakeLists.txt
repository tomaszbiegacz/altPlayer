# https://github.com/google/googletest/releases/tag/release-1.11.0
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "*.cc")

add_executable(
  tests_shared_c
  ${TEST_SOURCES}
)

target_link_libraries(
  tests_shared_c
  gtest_main
  shared_c
)

include(GoogleTest)
gtest_discover_tests(tests_shared_c)
