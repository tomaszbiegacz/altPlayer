#include "SharedTestFixture.h"

extern "C" {
  #include "nv_set.h"
  #include "log.h"
}

TEST(nv_set, basic) {
  struct nv_set *kv = NULL;

  EXPECT_EQ(0, nv_set_create(&kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k1", "v1"));
  EXPECT_STREQ("v1", nv_set_get_value(kv, "k1"));
  EXPECT_EQ(1, nv_set_get_length(kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k0", "v0"));
  EXPECT_STREQ("v0", nv_set_get_value(kv, "k0"));
  EXPECT_STREQ("v1", nv_set_get_value(kv, "k1"));
  EXPECT_EQ(2, nv_set_get_length(kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k2", "v2"));
  EXPECT_STREQ("v0", nv_set_get_value(kv, "k0"));
  EXPECT_STREQ("v1", nv_set_get_value(kv, "k1"));
  EXPECT_STREQ("v2", nv_set_get_value(kv, "k2"));
  EXPECT_EQ(3, nv_set_get_length(kv));

  nv_set_remove(kv, "k5");
  EXPECT_EQ(3, nv_set_get_length(kv));

  nv_set_remove(kv, "k1");
  EXPECT_EQ(2, nv_set_get_length(kv));
  EXPECT_STREQ("v0", nv_set_get_value(kv, "k0"));
  EXPECT_STREQ("v2", nv_set_get_value(kv, "k2"));

  EXPECT_STREQ("k0", nv_set_get_name_at(kv, 0));
  EXPECT_STREQ("k2", nv_set_get_name_at(kv, 1));

  nv_set_release(&kv);
}

TEST(nv_set, basic_random) {
  struct nv_set *kv = NULL;

  EXPECT_EQ(0, nv_set_create(&kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k01", "v01"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k05", "v05"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k03", "v03"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k09", "v09"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k10", "v10"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k11", "v11"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k17", "v17"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k16", "v16"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k15", "v15"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k02", "v02"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k04", "v04"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k06", "v06"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k08", "v08"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k12", "v12"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k13", "v13"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k14", "v14"));

  EXPECT_EQ(16, nv_set_get_length(kv));

  EXPECT_STREQ("v01", nv_set_get_value(kv, "k01"));
  EXPECT_STREQ("v02", nv_set_get_value(kv, "k02"));
  EXPECT_STREQ("v03", nv_set_get_value(kv, "k03"));
  EXPECT_STREQ("v04", nv_set_get_value(kv, "k04"));
  EXPECT_STREQ("v05", nv_set_get_value(kv, "k05"));
  EXPECT_STREQ("v06", nv_set_get_value(kv, "k06"));
  EXPECT_STREQ("v08", nv_set_get_value(kv, "k08"));
  EXPECT_STREQ("v09", nv_set_get_value(kv, "k09"));
  EXPECT_STREQ("v10", nv_set_get_value(kv, "k10"));
  EXPECT_STREQ("v11", nv_set_get_value(kv, "k11"));
  EXPECT_STREQ("v12", nv_set_get_value(kv, "k12"));
  EXPECT_STREQ("v13", nv_set_get_value(kv, "k13"));
  EXPECT_STREQ("v14", nv_set_get_value(kv, "k14"));
  EXPECT_STREQ("v15", nv_set_get_value(kv, "k15"));
  EXPECT_STREQ("v16", nv_set_get_value(kv, "k16"));
  EXPECT_STREQ("v17", nv_set_get_value(kv, "k17"));

  nv_set_release(&kv);
}

TEST(nv_set, resize) {
  struct nv_set *kv = NULL;

  EXPECT_EQ(0, nv_set_create(&kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k01", "v01"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k05", "v05"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k03", "v03"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k09", "v09"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k10", "v10"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k11", "v11"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k17", "v17"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k16", "v16"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k15", "v15"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k02", "v02"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k04", "v04"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k06", "v06"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k08", "v08"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k12", "v12"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k13", "v13"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k14", "v14"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k07", "v07"));

  EXPECT_EQ(17, nv_set_get_length(kv));

  EXPECT_STREQ("v01", nv_set_get_value(kv, "k01"));
  EXPECT_STREQ("v02", nv_set_get_value(kv, "k02"));
  EXPECT_STREQ("v03", nv_set_get_value(kv, "k03"));
  EXPECT_STREQ("v04", nv_set_get_value(kv, "k04"));
  EXPECT_STREQ("v05", nv_set_get_value(kv, "k05"));
  EXPECT_STREQ("v06", nv_set_get_value(kv, "k06"));
  EXPECT_STREQ("v07", nv_set_get_value(kv, "k07"));
  EXPECT_STREQ("v08", nv_set_get_value(kv, "k08"));
  EXPECT_STREQ("v09", nv_set_get_value(kv, "k09"));
  EXPECT_STREQ("v10", nv_set_get_value(kv, "k10"));
  EXPECT_STREQ("v11", nv_set_get_value(kv, "k11"));
  EXPECT_STREQ("v12", nv_set_get_value(kv, "k12"));
  EXPECT_STREQ("v13", nv_set_get_value(kv, "k13"));
  EXPECT_STREQ("v14", nv_set_get_value(kv, "k14"));
  EXPECT_STREQ("v15", nv_set_get_value(kv, "k15"));
  EXPECT_STREQ("v16", nv_set_get_value(kv, "k16"));
  EXPECT_STREQ("v17", nv_set_get_value(kv, "k17"));

  nv_set_release(&kv);
}

TEST(nv_set, print) {
  struct nv_set *kv = NULL;

  EXPECT_EQ(0, nv_set_create(&kv));

  EXPECT_EQ(0, nv_set_upsert(kv, "k01", "v01"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k05", "v05"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k03", "v03"));
  EXPECT_EQ(0, nv_set_upsert(kv, "k09", "v09"));

  log_set_verbose(true);
  log_verbose_nv_set_content(kv);

  nv_set_release(&kv);
}
