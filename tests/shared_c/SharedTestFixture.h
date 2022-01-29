#pragma once
#include <gtest/gtest.h>

extern "C" {
  #include "log.h"
}

class cSharedTestFixture: public ::testing::Test {
public:
  void SetUp() {
    log_set_verbose(true);
  }

  void TearDown() {
    log_global_release();
  }
};
