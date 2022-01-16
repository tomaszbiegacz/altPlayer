#ifndef PLAYER_URI_H_
#define PLAYER_URI_H_

#include "shrdef.h"

struct uri_authority {
  char *host;
  unsigned short port;
  char *full_name;
};

struct uri_structure {
  struct uri_authority authority;
  char *path;
  char *query;
  char *full_path;
};

//
// Setup
//

error_t
uri_parse(const char *uri_str, struct uri_structure **result_r);

void
uri_release(struct uri_structure **result_r);

void
uri_authority_release(struct uri_authority **result_r);

//
// Query
//

static inline const char*
uri_authority_get_host(const struct uri_authority *uri) {
  assert(uri != NULL);
  return uri->host;
}

static inline unsigned short
uri_authority_get_port(const struct uri_authority *uri) {
  assert(uri != NULL);
  return uri->port;
}

static inline const char*
uri_authority_get_full_name(const struct uri_authority *uri) {
  assert(uri != NULL);
  return uri->full_name;
}

bool
uri_authority_equals(
  const struct uri_authority *u1,
  const struct uri_authority *u2);


static inline const struct uri_authority*
uri_get_authority(const struct uri_structure *uri) {
  assert(uri != NULL);
  return &uri->authority;
}

static inline bool
uri_is_secure(const struct uri_structure *uri) {
  assert(uri != NULL);
  return true;
}

static inline const char*
uri_get_scheme(const struct uri_structure *uri) {
  assert(uri != NULL);
  return "https";
}

static inline const char*
uri_get_full_path(const struct uri_structure *uri) {
  assert(uri != NULL);
  return uri->full_path;
}

//
// Commands
//

error_t
uri_authority_copy(
  const struct uri_authority *uri,
  struct uri_authority **result_r);

#endif
