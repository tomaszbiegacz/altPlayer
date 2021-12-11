#include "SharedTestFixture.h"

extern "C" {
  #include "timer.h"
}

TEST_F(SharedTestFixture, timespec_miliseconds_TEST_basic) {
  struct timespec span;

  span.tv_sec = 1;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(1000, timespec_miliseconds(span));

  span.tv_sec = 1;
  span.tv_nsec = 1000000; // ms
  EXPECT_EQ(1001, timespec_miliseconds(span));
}

TEST_F(SharedTestFixture, timespec_get_minutes_TEST_basic) {
  struct timespec span;

  span.tv_sec = 62;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(1, timespec_get_minutes(span));
}

TEST_F(SharedTestFixture, timespec_get_remaining_seconds_TEST_basic) {
  struct timespec span;

  span.tv_sec = 62;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(2, timespec_get_remaining_seconds(span));
}

TEST_F(SharedTestFixture, timespec_elapsed_between_TEST_basic) {
  struct timespec start, end, result;

  start.tv_sec = 1;
  start.tv_nsec = 1;
  end.tv_sec = 1;
  end.tv_nsec = 1;
  result = timespec_elapsed_between(start, end);
  EXPECT_EQ(0, result.tv_sec);
  EXPECT_EQ(0, result.tv_nsec);

  start.tv_sec = 1;
  start.tv_nsec = 1;
  end.tv_sec = 2;
  end.tv_nsec = 3;
  result = timespec_elapsed_between(start, end);
  EXPECT_EQ(1, result.tv_sec);
  EXPECT_EQ(2, result.tv_nsec);

  start.tv_sec = 1;
  start.tv_nsec = 2;
  end.tv_sec = 2;
  end.tv_nsec = 1;
  result = timespec_elapsed_between(start, end);
  EXPECT_EQ(0, result.tv_sec);
  EXPECT_EQ(999999999l, result.tv_nsec);
}
