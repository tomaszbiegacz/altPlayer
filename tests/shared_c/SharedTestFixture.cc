#include "SharedTestFixture.h"

extern "C" {
  #include "log.h"
}

void SharedTestFixture::SetUp() {
  log_start();
}

void SharedTestFixture::TearDown() {
  log_free();
}
