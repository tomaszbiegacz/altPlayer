#include "SharedTestFixture.h"

extern "C" {
  #include "log.h"
}

void SharedTestFixture::SetUp() {
  log_set_verbose(true);
}

void SharedTestFixture::TearDown() {
  log_global_release();
}
