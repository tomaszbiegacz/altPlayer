#include "cont_buf.h"
#include "mem.h"

struct cont_buf_read {
  struct cont_buf* src;
};

struct cont_buf {
  size_t size_allocated;
  size_t size_used;
  size_t read_offset;
  void *data;
  struct cont_buf_read reader;
};

error_t
cont_buf_create(
  size_t size,
  struct cont_buf **result_r) {
    assert(result_r != NULL);

    struct cont_buf *result = NULL;
    error_t error_r = mem_calloc(
      "cont_buf",
      sizeof(struct cont_buf),
      (void**)&result); // NOLINT
    if (error_r == 0) {
      error_r = mem_map_alloc("cont_buf", size, &result->data);
    }

    if (error_r == 0) {
      result->reader.src = result;
      result->size_allocated = size;
      result->size_used = 0;
      result->read_offset = 0;
      *result_r = result;
    } else {
      cont_buf_release(&result);
    }
    return error_r;
  }

void
cont_buf_release(struct cont_buf **result_r) {
  assert(result_r != NULL);

  struct cont_buf *result = *result_r;
  if (result != NULL) {
    mem_map_free(result->size_allocated, &result->data);
    mem_free((void**)result_r); // NOLINT
  }
}

struct cont_buf_read*
cont_buf_read(struct cont_buf *src) {
  assert(src != NULL);
  return &src->reader;
}


inline static void*
data_read_start(struct cont_buf *src) {
  return (char*)src->data + src->read_offset; // NOLINT
}

inline static size_t
data_read_size(const struct cont_buf *src) {
  return src->size_used - src->read_offset;
}


inline static void*
data_write_start(struct cont_buf *src) {
  return (char*)src->data + src->size_used; // NOLINT
}

inline static size_t
data_write_size(const struct cont_buf *src) {
  return src->size_allocated - src->size_used;
}


void
cont_buf_no_padding(struct cont_buf *src) {
  if (src->read_offset > 0) {
    size_t unread_size = data_read_size(src);
    memmove(
      src->data,
      data_read_start(src),
      unread_size);
    src->read_offset = 0;
    src->size_used = unread_size;
  }
}

void
cont_buf_clear(struct cont_buf *buf) {
  assert(buf != NULL);
  buf->read_offset = 0;
  buf->size_used = 0;
}

error_t
cont_buf_resize(struct cont_buf *buf, size_t size) {
  assert(buf != NULL);
  error_t error_r = 0;
  if (buf->size_allocated != size) {
    void* data = NULL;
    error_r = mem_map_alloc("cont_buf", size, &data);
    if (error_r == 0) {
      size_t to_copy = data_read_size(buf);
      assert(to_copy <= size);
      if (to_copy > 0) {
        memmove(data, data_read_start(buf), to_copy);
        buf->read_offset = 0;
        buf->size_used = to_copy;
      }
      mem_map_free(buf->size_allocated, &buf->data);
      buf->data = data;
      buf->size_allocated = size;
    }
  }
  return error_r;
}

void
cont_buf_write_array_begin(
  struct cont_buf *buf,
  size_t item_size,
  void **data,
  size_t *count) {
    assert(buf != NULL);
    assert(item_size > 0);
    assert(data  != NULL);
    assert(count != NULL);
    *data = data_write_start(buf);
    *count = data_write_size(buf) / item_size;
  }

void
cont_buf_write_array_commit(
  struct cont_buf *src,
  size_t item_size,
  size_t count) {
    assert(src != NULL);
    assert(item_size > 0);
    size_t written = item_size * count;
    assert(written <= data_write_size(src));
    src->size_used += written;
  }

size_t
cont_buf_write(
  struct cont_buf *buf,
  const void *data,
  size_t size) {
    assert(buf != NULL);
    assert(data != NULL);

    size_t written = min_size_t(size, data_write_size(buf));
    memmove(data_write_start(buf), data, written);
    buf->size_used += written;
    return written;
  }

static void
no_padding_on_empty(struct cont_buf *buf) {
  if (data_read_size(buf) == 0) {
    cont_buf_clear(buf);
  }
}

bool
cont_buf_read_try(
  struct cont_buf_read *buf,
  size_t item_size,
  const void **result) {
    assert(buf != NULL);
    assert(item_size > 0);
    assert(result != NULL);

    if (data_read_size(buf->src) >= item_size) {
      *result = data_read_start(buf->src);
      buf->src->read_offset += item_size;
      no_padding_on_empty(buf->src);
      return true;
    } else {
      return false;
    }
  }

size_t
cont_buf_read_array_begin(
  struct cont_buf_read *buf,
  size_t item_size,
  const void **result,
  size_t count) {
    assert(buf != NULL);
    assert(item_size > 0);
    assert(result != NULL);

    *result = data_read_start(buf->src);
    return min_size_t(count, data_read_size(buf->src) / item_size);
  }

void
cont_buf_read_array_commit(
  struct cont_buf_read *buf,
  size_t item_size,
  size_t count) {
    assert(buf != NULL);
    assert(item_size > 0);
    assert(count <= data_read_size(buf->src) / item_size);

    buf->src->read_offset += count * item_size;
    no_padding_on_empty(buf->src);
  }

size_t
cont_buf_get_allocated_size(const struct cont_buf *src) {
  assert(src != NULL);
  return src->size_allocated;
}

size_t
cont_buf_get_available_size(const struct cont_buf *src) {
  assert(src != NULL);
  return data_write_size(src);
}

bool
cont_buf_is_empty(const struct cont_buf *src) {
  assert(src != NULL);
  return cont_buf_read_is_empty(&src->reader);
}

size_t
cont_buf_get_unread_size(const struct cont_buf_read *buf) {
  assert(buf != NULL);
  return data_read_size(buf->src);
}

size_t
cont_buf_move(
  struct cont_buf *dest,
  struct cont_buf_read *src) {
    size_t written = 0;
    size_t available  = cont_buf_get_available_size(dest);
    if (available > 0) {
      const void *data = NULL;
      size_t count = cont_buf_read_array_begin(src, 1, &data, available);
      written = cont_buf_write(dest, data, count);
      cont_buf_read_array_commit(src, 1, written);
    }
    return written;
  }
