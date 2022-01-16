#ifndef PLAYER_NV_SET_H_
#define PLAYER_NV_SET_H_

#include "shrdef.h"

/**
 * @brief Name-value pair set
 * Both pair of value is of string type
 */
struct nv_set;

//
// Setup
//

error_t
nv_set_create(struct nv_set **result_r);

void
nv_set_release(struct nv_set **result_r);

//
// Query
//

size_t
nv_set_get_length(const struct nv_set *source);

char*
nv_set_get_value(const struct nv_set *source, const char *name);

const char*
nv_set_get_name_at(const struct nv_set *source, size_t pos);

char*
nv_set_get_value_at(const struct nv_set *source, size_t pos);


//
// Commands
//

error_t
nv_set_upsert(
  struct nv_set *source,
  const char *name,
  const char *value);

void
nv_set_remove(struct nv_set *source, const char *name);

void
log_verbose_nv_set_content(const struct nv_set *source);

#endif
