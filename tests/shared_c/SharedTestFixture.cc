#include "SharedTestFixture.h"

extern "C" {
  #include "log.h"
}

void SharedTestFixture::SetUp() {
  log_start();
  log_set_verbose(true);
}

void SharedTestFixture::TearDown() {
  log_free();
}
