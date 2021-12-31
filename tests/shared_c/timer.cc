#include "SharedTestFixture.h"

extern "C" {
  #include "timer.h"
}

TEST(timer, timespec_miliseconds) {
  struct timespec span;

  span.tv_sec = 1;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(1000, timespec_get_miliseconds(span));

  span.tv_sec = 1;
  span.tv_nsec = 1000000; // ms
  EXPECT_EQ(1001, timespec_get_miliseconds(span));
}

TEST(timer, timespec_get_minutes) {
  struct timespec span;

  span.tv_sec = 62;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(1, timespec_get_minutes(span));
}

TEST(timer, timespec_get_remaining_seconds) {
  struct timespec span;

  span.tv_sec = 62;
  span.tv_nsec = 1000; // us
  EXPECT_EQ(2, timespec_get_seconds_remaining(span));
}

TEST(timer, timespec_elapsed_between) {
  struct timespec start, end, result;

  start.tv_sec = 1;
  start.tv_nsec = 1;
  end.tv_sec = 1;
  end.tv_nsec = 1;
  result = timespec_get_elapsed_between(start, end);
  EXPECT_EQ(0, result.tv_sec);
  EXPECT_EQ(0, result.tv_nsec);

  start.tv_sec = 1;
  start.tv_nsec = 1;
  end.tv_sec = 2;
  end.tv_nsec = 3;
  result = timespec_get_elapsed_between(start, end);
  EXPECT_EQ(1, result.tv_sec);
  EXPECT_EQ(2, result.tv_nsec);

  start.tv_sec = 1;
  start.tv_nsec = 2;
  end.tv_sec = 2;
  end.tv_nsec = 1;
  result = timespec_get_elapsed_between(start, end);
  EXPECT_EQ(0, result.tv_sec);
  EXPECT_EQ(999999999l, result.tv_nsec);
}
