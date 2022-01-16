#ifndef PLAYER_KP_SET_H_
#define PLAYER_KP_SET_H_

#include "shrdef.h"

/**
 * @brief Key-pointer pair set
 * Key is of int64 type
 * Object is generic pointer
 */
struct kp_set;

//
// Setup
//

error_t
kp_set_create(
  struct kp_set **result_r,
  void (*free)(void *ptr));

void
kp_set_release(struct kp_set **result_r);

//
// Query
//

size_t
kp_set_get_length(const struct kp_set *source);

void*
kp_set_get_value(const struct kp_set *source, int64_t key);

//
// Commands
//

error_t
kp_set_upsert(
  struct kp_set *source,
  int64_t key,
  void *ptr);

void
kp_set_remove(struct kp_set *source, int64_t key);

#endif
