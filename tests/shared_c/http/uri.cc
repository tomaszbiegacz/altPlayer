#include "../SharedTestFixture.h"

extern "C" {
  #include "http/uri.h"
}

TEST(http__uri, basic) {
  struct uri_structure *uri = NULL;

  EXPECT_EQ(0, uri_parse("https://drive.google.com/uc?id=XXX", &uri));
  const struct uri_authority *auth = uri_get_authority(uri);

  EXPECT_STREQ("https", uri_get_scheme(uri));
  EXPECT_EQ(true, uri_is_secure(uri));
  EXPECT_STREQ("/uc?id=XXX", uri_get_full_path(uri));

  EXPECT_STREQ("drive.google.com", uri_authority_get_host(auth));
  EXPECT_EQ(443, uri_authority_get_port(auth));

  struct uri_authority *auth2 = NULL;
  EXPECT_EQ(0, uri_authority_copy(auth, &auth2));
  EXPECT_NE(auth, auth2);

  EXPECT_STREQ("drive.google.com", uri_authority_get_host(auth2));
  EXPECT_EQ(443, uri_authority_get_port(auth2));
  EXPECT_EQ(true, uri_authority_equals(auth, auth2));

  uri_release(&uri);
  uri_authority_release(&auth2);
}
