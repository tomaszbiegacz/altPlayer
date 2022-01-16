#include "log.h"
#include "mem.h"
#include "nv_set.h"
#include "qfind.h"

struct nv_set {
  size_t length;
  char **names;
  char **values;
  size_t length_used;
};

void
nv_set_release(struct nv_set **result_r) {
  assert(result_r != NULL);

  struct nv_set *result = *result_r;
  if (result != NULL) {
    mem_list_free(
      (void**)result->names,  // NOLINT
      result->length_used, free);
    mem_list_free(
      (void**)result->values, // NOLINT
      result->length_used, free);
    free(result);
    *result_r = NULL;
  }
}

error_t
nv_set_create(struct nv_set **result_r) {
  assert(result_r != NULL);

  struct nv_set *result = NULL;
  error_t error_r = mem_calloc(
    "nv_set",
    sizeof(struct nv_set),
    (void**)&result); // NOLINT

  if (error_r == 0) {
    result->length = 16;
    error_r = mem_list_extend(
      (void***) &result->names, // NOLINT
      result->length);
  }
  if (error_r == 0) {
    error_r = mem_list_extend(
      (void***) &result->values, // NOLINT
      result->length);
  }

  if (error_r == 0) {
    *result_r = result;
  } else {
    nv_set_release(&result);
  }
  return error_r;
}

size_t
nv_set_get_length(const struct nv_set *source) {
  assert(source != NULL);
  return source->length_used;
}

const char*
nv_set_get_name_at(const struct nv_set *source, size_t pos) {
  assert(source != NULL);
  assert(pos < source->length_used);
  return source->names[pos];
}

char*
nv_set_get_value_at(const struct nv_set *source, size_t pos) {
  assert(source != NULL);
  assert(pos < source->length_used);
  return source->values[pos];
}

inline static int
qfind_name_compare(const void* searched, const void* item) {
  char * const * const item_c = item;
  return strcmp((const char*)searched, *item_c);
}

inline static ssize_t
find_name_position(const struct nv_set *source, const char *name) {
  return qfind(
    source->names,
    source->length_used,
    sizeof(char*),
    qfind_name_compare,
    name);
}

char*
nv_set_get_value(const struct nv_set *source, const char *name) {
  assert(source != NULL);
  assert(name != NULL);

  ssize_t pos = find_name_position(source, name);
  if (qfind_is_found(pos)) {
    return source->values[pos];
  } else {
    return NULL;
  }
}

static error_t
resize_if_needed(struct nv_set *source) {
  error_t error_r = 0;
  if (source->length_used == source->length) {
    size_t new_length = source->length * 2;

    error_r = mem_list_extend(
      (void***)&source->names,  // NOLINT
      new_length);

    if (error_r == 0) {
      error_r = mem_list_extend(
        (void***)&source->values,  // NOLINT
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
  struct nv_set *source,
  size_t pos,
  const char *name,
  const char *value) {
    char *name_copy = NULL;
    char *value_copy = NULL;

    error_t error_r = resize_if_needed(source);
    if (error_r == 0) {
      error_r = mem_strdup(name, &name_copy);
    }
    if (error_r == 0) {
      error_r = mem_strdup(value, &value_copy);
    }
    if (error_r == 0) {
      mem_list_insert_at(
        (void**)source->names,  // NOLINT
        source->length_used, pos, name_copy);
      mem_list_insert_at(
        (void**)source->values,   // NOLINT
        source->length_used, pos, value_copy);
      source->length_used++;
    } else {
      free(name_copy);
      free(value_copy);
    }
    return error_r;
  }

error_t
nv_set_upsert(
  struct nv_set *source,
  const char *name,
  const char *value) {
    assert(source != NULL);
    assert(name != NULL);
    assert(value != NULL);

    error_t error_r = 0;
    ssize_t pos = find_name_position(source, name);
    if (qfind_is_found(pos)) {
      char* value_copy = NULL;
      error_r = mem_strdup(value, &value_copy);
      if (error_r == 0) {
        free(source->values[pos]);
        source->values[pos] = value_copy;
      }
    } else {
      error_r = insert_at(
        source,
        qfind_get_pos_to_insert(pos),
        name,
        value);
    }
    return error_r;
  }

void
nv_set_remove(struct nv_set *source, const char *name) {
  assert(source != NULL);
  assert(name != NULL);

  ssize_t pos = find_name_position(source, name);
  if (qfind_is_found(pos)) {
    free(source->names[pos]);
    free(source->values[pos]);

    mem_list_remove_at(
      (void**)source->names,  // NOLINT
      source->length_used, pos);
    mem_list_remove_at(
      (void**)source->values, // NOLINT
      source->length_used, pos);
    source->length_used--;
  }
}

void
log_verbose_nv_set_content(const struct nv_set *source) {
  assert(source != NULL);

  if (log_is_verbose()) {
    for (size_t i=0; i < source->length_used; ++i) {
      log_verbose("%s: %s", source->names[i], source->values[i]);
    }
  }
}
