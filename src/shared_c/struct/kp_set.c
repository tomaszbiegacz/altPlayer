#include "../log.h"
#include "../mem.h"
#include "kp_set.h"
#include "qfind.h"

typedef void (*free_f)(void *ptr);

struct kp_set {
  size_t length;
  int64_t *keys;
  void **values;
  size_t length_used;
  free_f ptr_free;
};

void
kp_set_release(struct kp_set **result_r) {
  assert(result_r != NULL);

  struct kp_set *result = *result_r;
  if (result != NULL) {
    free(result->keys);
    mem_list_free(
      result->values,
      result->length_used,
      result->ptr_free);
    free(result);
    *result_r = NULL;
  }
}

error_t
kp_set_create(
  struct kp_set **result_r,
  free_f object_free) {
    assert(result_r != NULL);
    assert(object_free != NULL);

    struct kp_set *result = NULL;
    error_t error_r = mem_calloc(
      "kp_set",
      sizeof(struct kp_set),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      result->length = 16;
      error_r = mem_list_i64_extend(
        &result->keys,
        result->length);
    }
    if (error_r == 0) {
      error_r = mem_list_extend(
        &result->values,
        result->length);
    }

    if (error_r == 0) {
      result->ptr_free = object_free;
      *result_r = result;
    } else {
      kp_set_release(&result);
    }
    return error_r;
  }

size_t
kp_set_get_length(const struct kp_set *source) {
  assert(source != NULL);
  return source->length_used;
}

inline static int
qfind_key_compare(const void* searched, const void* item) {
  const int64_t *searched_i = (const int64_t*)searched; // NOLINT
  const int64_t *item_i = (const int64_t*)item;   // NOLINT
  int64_t r = *searched_i - *item_i;
  return r == 0 ? 0 : ( (r < 0) ? -1 : 1);
}

inline static ssize_t
find_key_position(const struct kp_set *source, int64_t *key) {
  return qfind(
    source->keys,
    source->length_used,
    sizeof(int64_t),
    qfind_key_compare,
    key);
}

void*
kp_set_get_value(const struct kp_set *source, int64_t key) {
  assert(source != NULL);

  ssize_t pos = find_key_position(source, &key);
  if (qfind_is_found(pos)) {
    return source->values[pos];
  } else {
    return NULL;
  }
}

static error_t
resize_if_needed(struct kp_set *source) {
  error_t error_r = 0;
  if (source->length_used == source->length) {
    size_t new_length = source->length * 2;

    error_r = mem_list_i64_extend(
      &source->keys,
      new_length);

    if (error_r == 0) {
      error_r = mem_list_extend(
        &source->values,
        new_length);
    }

    if (error_r == 0) {
      source->length = new_length;
    }
  }
  return error_r;
}

static error_t
insert_at(
  struct kp_set *source,
  size_t pos,
  int64_t key,
  void *value) {
    error_t error_r = resize_if_needed(source);
    if (error_r == 0) {
      mem_list_i64_insert_at(
        source->keys,
        source->length_used, pos, key);
      mem_list_insert_at(
        source->values,
        source->length_used, pos, value);
      source->length_used++;
    }
    return error_r;
  }

error_t
kp_set_upsert(
  struct kp_set *source,
  int64_t key,
  void *ptr) {
    assert(source != NULL);
    assert(ptr != NULL);

    error_t error_r = 0;
    ssize_t pos = find_key_position(source, &key);
    if (qfind_is_found(pos)) {
      source->ptr_free(source->values[pos]);
      source->values[pos] = ptr;
    } else {
      error_r = insert_at(
        source,
        qfind_get_pos_to_insert(pos),
        key,
        ptr);
    }
    return error_r;
  }

void
kp_set_remove(struct kp_set *source, int64_t key) {
  assert(source != NULL);

  ssize_t pos = find_key_position(source, &key);
  if (qfind_is_found(pos)) {
    source->ptr_free(source->values[pos]);

    mem_list_i64_remove_at(
      source->keys,
      source->length_used, pos);
    mem_list_remove_at(
      source->values,
      source->length_used, pos);
    source->length_used--;
  }
}
