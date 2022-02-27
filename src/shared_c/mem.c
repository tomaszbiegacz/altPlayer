#include <sys/mman.h>
#include "log.h"
#include "mem.h"

error_t
mem_calloc(const char *resource_name, size_t size, void **result_r) {
  assert(result_r != NULL);

  void *result = calloc(size, 1);
  if (result == NULL) {
    log_error("Cannot allocate memory for %s", resource_name);
    return ENOMEM;
  } else {
    *result_r = result;
    return 0;
  }
}

void
mem_free(void **result_r) {
  assert(result_r != NULL);

  void *result = *result_r;
  if (result != NULL) {
    free(result);
    *result_r = NULL;
  }
}

error_t
mem_map_alloc(const char *resource_name, size_t size, void **result_r) {
  assert(result_r != NULL);

  void* mem_range = mmap(
    NULL,
    size,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS,
    0, 0);

  if (mem_range == MAP_FAILED) {
    log_error(
      "Failed to allocate memory of size %d for %s",
      size, resource_name);
    return errno;
  } else {
    log_verbose(
      "Allocated %dkB of memory mapping starting at %x",
      size / 1024,
      (u_int64_t)mem_range);
    *result_r = mem_range;
    return 0;
  }
}

void
mem_map_free(size_t size, void **result_r) {
  assert(result_r != NULL);

  void *result = *result_r;
  if (result != NULL) {
    if (munmap(result, size) != 0) {
      log_error(
        "Cannot release memory mapping starting at %x due to %s",
        (u_int64_t)result,
        strerror(errno));
    } else {
      *result_r = NULL;
      log_verbose(
        "Released memory mapping starting at %x",
        (u_int64_t)result);
    }
  }
}

error_t
mem_strdup(const char *source, char **result_r) {
  assert(source != NULL);
  assert(result_r != NULL);

  char *result = strdup(source);
  if (result == NULL) {
    log_error("Cannot allocate string of size %d", strlen(source));
    return ENOMEM;
  } else {
    *result_r = result;
    return 0;
  }
}

error_t
mem_strndup(const char *source, size_t len, char **result_r) {
  assert(source != NULL);
  assert(result_r != NULL);

  char *result = strndup(source, len);
  if (result == NULL) {
    log_error("Cannot allocate string of size %d", strlen(source));
    return ENOMEM;
  } else {
    *result_r = result;
    return 0;
  }
}

error_t
mem_list_extend(
  void ***base_r,
  size_t new_length) {
    assert(base_r != NULL);
    assert(new_length > 0);

    error_t error_r = 0;
    void **base = *base_r;
    void **relocated;
    size_t new_size = sizeof(void*) * new_length;
    if (base == NULL) {
      error_r = mem_calloc(
        "list",
        new_size,
        (void**)&relocated);  // NOLINT
    } else {
      relocated = (void**)realloc(  // NOLINT
        (void*)base, // NOLINT
        new_size);
    }
    if (relocated == NULL) {
      log_error("Cannot relocate list to size %d", new_size);
      error_r = ENOMEM;
    } else {
      *base_r = relocated;
    }
    return error_r;
  }

error_t
mem_list_i64_extend(
  int64_t **base_r,
  size_t new_length) {
    assert(base_r != NULL);
    assert(new_length > 0);

    error_t error_r = 0;
    int64_t *base = *base_r;
    int64_t *relocated;
    size_t new_size = sizeof(int64_t) * new_length;
    if (base == NULL) {
      error_r = mem_calloc(
        "list",
        new_size,
        (void**)&relocated);  // NOLINT
    } else {
      relocated = (int64_t*)realloc(  // NOLINT
        (void*)base, // NOLINT
        new_size);
    }
    if (relocated == NULL) {
      log_error("Cannot relocate int list to size %d", new_size);
      error_r = ENOMEM;
    } else {
      *base_r = relocated;
    }
    return error_r;
  }

void
mem_list_free(
  void **base,
  size_t length,
  void (*ptr_free)(void *ptr)) {
    assert(base != NULL);
    assert(ptr_free != NULL);

    for (size_t i=0; i < length; ++i) {
      ptr_free(base[i]);
    }
    free(base);
  }