#include <gtest/gtest.h>

class SharedTestFixture: public ::testing::Test {
public:
  void SetUp();
  void TearDown();
};

#define EMPTY_STRUCT(t, n) struct t n;\
  memset(&n, 0, sizeof(n))
