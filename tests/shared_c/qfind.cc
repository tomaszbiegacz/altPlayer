#include "SharedTestFixture.h"

extern "C" {
  #include "qfind.h"
}

static int
compare(const void* searched, const void* item) {
  int *searched_i = (int*)searched;
  int *item_i = (int*)item;
  return *searched_i - *item_i;
}

TEST(qfind, basic) {
  int items[] = { 2, 4, 6 };
  size_t nitems = 3;
  size_t size = sizeof(int);
  int searched;

  searched = 1;
  EXPECT_EQ(-1, qfind(items, nitems, size, compare, &searched));

  searched = 2;
  EXPECT_EQ(0, qfind(items, nitems, size, compare, &searched));

  searched = 3;
  EXPECT_EQ(-2, qfind(items, nitems, size, compare, &searched));

  searched = 4;
  EXPECT_EQ(1, qfind(items, nitems, size, compare, &searched));

  searched = 5;
  EXPECT_EQ(-3, qfind(items, nitems, size, compare, &searched));

  searched = 6;
  EXPECT_EQ(2, qfind(items, nitems, size, compare, &searched));

  searched = 7;
  EXPECT_EQ(-4, qfind(items, nitems, size, compare, &searched));
}
