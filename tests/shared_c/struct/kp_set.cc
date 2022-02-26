#include "../SharedTestFixture.h"

extern "C" {
  #include "struct/kp_set.h"
}

void
value_free(void *ptr) {
  UNUSED(ptr);
}

TEST(structs__kp_set, basic) {
  struct kp_set *kv = NULL;

  EXPECT_EQ(0, kp_set_create(&kv, value_free));

  EXPECT_EQ(0, kp_set_upsert(kv, 1, (void*)"v1"));
  EXPECT_STREQ("v1", (const char*)kp_set_get_value(kv, 1));
  EXPECT_EQ(1, kp_set_get_length(kv));

  EXPECT_EQ(0, kp_set_upsert(kv, 0, (void*)"v0"));
  EXPECT_STREQ("v0", (const char*)kp_set_get_value(kv, 0));
  EXPECT_STREQ("v1", (const char*)kp_set_get_value(kv, 1));
  EXPECT_EQ(2, kp_set_get_length(kv));

  EXPECT_EQ(0, kp_set_upsert(kv, 2, (void*)"v2"));
  EXPECT_STREQ("v0", (const char*)kp_set_get_value(kv, 0));
  EXPECT_STREQ("v1", (const char*)kp_set_get_value(kv, 1));
  EXPECT_STREQ("v2", (const char*)kp_set_get_value(kv, 2));
  EXPECT_EQ(3, kp_set_get_length(kv));

  kp_set_remove(kv, 5);
  EXPECT_EQ(3, kp_set_get_length(kv));

  kp_set_remove(kv, 1);
  EXPECT_EQ(2, kp_set_get_length(kv));
  EXPECT_STREQ("v0", (const char*)kp_set_get_value(kv, 0));
  EXPECT_STREQ("v2", (const char*)kp_set_get_value(kv, 2));

  kp_set_release(&kv);
}

TEST(structs__kp_set, basic_random) {
  struct kp_set *kv = NULL;

  EXPECT_EQ(0, kp_set_create(&kv, value_free));

  EXPECT_EQ(0, kp_set_upsert(kv, 1, (void*)"v01"));
  EXPECT_EQ(0, kp_set_upsert(kv, 5, (void*)"v05"));
  EXPECT_EQ(0, kp_set_upsert(kv, 3, (void*)"v03"));
  EXPECT_EQ(0, kp_set_upsert(kv, 9, (void*)"v09"));
  EXPECT_EQ(0, kp_set_upsert(kv, 10, (void*)"v10"));
  EXPECT_EQ(0, kp_set_upsert(kv, 11, (void*)"v11"));
  EXPECT_EQ(0, kp_set_upsert(kv, 17, (void*)"v17"));
  EXPECT_EQ(0, kp_set_upsert(kv, 16, (void*)"v16"));
  EXPECT_EQ(0, kp_set_upsert(kv, 15, (void*)"v15"));
  EXPECT_EQ(0, kp_set_upsert(kv, 2, (void*)"v02"));
  EXPECT_EQ(0, kp_set_upsert(kv, 4, (void*)"v04"));
  EXPECT_EQ(0, kp_set_upsert(kv, 6, (void*)"v06"));
  EXPECT_EQ(0, kp_set_upsert(kv, 8, (void*)"v08"));
  EXPECT_EQ(0, kp_set_upsert(kv, 12, (void*)"v12"));
  EXPECT_EQ(0, kp_set_upsert(kv, 13, (void*)"v13"));
  EXPECT_EQ(0, kp_set_upsert(kv, 14, (void*)"v14"));

  EXPECT_EQ(16, kp_set_get_length(kv));

  EXPECT_STREQ("v01", (const char*)kp_set_get_value(kv, 1));
  EXPECT_STREQ("v02", (const char*)kp_set_get_value(kv, 2));
  EXPECT_STREQ("v03", (const char*)kp_set_get_value(kv, 3));
  EXPECT_STREQ("v04", (const char*)kp_set_get_value(kv, 4));
  EXPECT_STREQ("v05", (const char*)kp_set_get_value(kv, 5));
  EXPECT_STREQ("v06", (const char*)kp_set_get_value(kv, 6));
  EXPECT_STREQ("v08", (const char*)kp_set_get_value(kv, 8));
  EXPECT_STREQ("v09", (const char*)kp_set_get_value(kv, 9));
  EXPECT_STREQ("v10", (const char*)kp_set_get_value(kv, 10));
  EXPECT_STREQ("v11", (const char*)kp_set_get_value(kv, 11));
  EXPECT_STREQ("v12", (const char*)kp_set_get_value(kv, 12));
  EXPECT_STREQ("v13", (const char*)kp_set_get_value(kv, 13));
  EXPECT_STREQ("v14", (const char*)kp_set_get_value(kv, 14));
  EXPECT_STREQ("v15", (const char*)kp_set_get_value(kv, 15));
  EXPECT_STREQ("v16", (const char*)kp_set_get_value(kv, 16));
  EXPECT_STREQ("v17", (const char*)kp_set_get_value(kv, 17));

  kp_set_release(&kv);
}

TEST(structs__kp_set, resize) {
  struct kp_set *kv = NULL;

  EXPECT_EQ(0, kp_set_create(&kv, value_free));

  EXPECT_EQ(0, kp_set_upsert(kv, 1, (void*)"v01"));
  EXPECT_EQ(0, kp_set_upsert(kv, 5, (void*)"v05"));
  EXPECT_EQ(0, kp_set_upsert(kv, 3, (void*)"v03"));
  EXPECT_EQ(0, kp_set_upsert(kv, 9, (void*)"v09"));
  EXPECT_EQ(0, kp_set_upsert(kv, 10, (void*)"v10"));
  EXPECT_EQ(0, kp_set_upsert(kv, 11, (void*)"v11"));
  EXPECT_EQ(0, kp_set_upsert(kv, 17, (void*)"v17"));
  EXPECT_EQ(0, kp_set_upsert(kv, 16, (void*)"v16"));
  EXPECT_EQ(0, kp_set_upsert(kv, 15, (void*)"v15"));
  EXPECT_EQ(0, kp_set_upsert(kv, 2, (void*)"v02"));
  EXPECT_EQ(0, kp_set_upsert(kv, 4, (void*)"v04"));
  EXPECT_EQ(0, kp_set_upsert(kv, 6, (void*)"v06"));
  EXPECT_EQ(0, kp_set_upsert(kv, 8, (void*)"v08"));
  EXPECT_EQ(0, kp_set_upsert(kv, 12, (void*)"v12"));
  EXPECT_EQ(0, kp_set_upsert(kv, 13, (void*)"v13"));
  EXPECT_EQ(0, kp_set_upsert(kv, 14, (void*)"v14"));
  EXPECT_EQ(0, kp_set_upsert(kv, 7, (void*)"v07"));

  EXPECT_EQ(17, kp_set_get_length(kv));

  EXPECT_STREQ("v01", (const char*)kp_set_get_value(kv, 1));
  EXPECT_STREQ("v02", (const char*)kp_set_get_value(kv, 2));
  EXPECT_STREQ("v03", (const char*)kp_set_get_value(kv, 3));
  EXPECT_STREQ("v04", (const char*)kp_set_get_value(kv, 4));
  EXPECT_STREQ("v05", (const char*)kp_set_get_value(kv, 5));
  EXPECT_STREQ("v06", (const char*)kp_set_get_value(kv, 6));
  EXPECT_STREQ("v07", (const char*)kp_set_get_value(kv, 7));
  EXPECT_STREQ("v08", (const char*)kp_set_get_value(kv, 8));
  EXPECT_STREQ("v09", (const char*)kp_set_get_value(kv, 9));
  EXPECT_STREQ("v10", (const char*)kp_set_get_value(kv, 10));
  EXPECT_STREQ("v11", (const char*)kp_set_get_value(kv, 11));
  EXPECT_STREQ("v12", (const char*)kp_set_get_value(kv, 12));
  EXPECT_STREQ("v13", (const char*)kp_set_get_value(kv, 13));
  EXPECT_STREQ("v14", (const char*)kp_set_get_value(kv, 14));
  EXPECT_STREQ("v15", (const char*)kp_set_get_value(kv, 15));
  EXPECT_STREQ("v16", (const char*)kp_set_get_value(kv, 16));
  EXPECT_STREQ("v17", (const char*)kp_set_get_value(kv, 17));

  kp_set_release(&kv);
}
