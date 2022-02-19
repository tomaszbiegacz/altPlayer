#pragma once
#include "cont_buf_read.h"

/**
 * @brief Continuos buffer
 */
struct cont_buf;

//
// Setup
//

/**
 * @brief Create continous memory buffer
 */
error_t
cont_buf_create(
  size_t size,
  struct cont_buf **result_r);

void
cont_buf_release(struct cont_buf **result_r);

//
// Query
//


/**
 * @brief Get memory size allocated for the buffer
 */
size_t
cont_buf_get_allocated_size(const struct cont_buf *src);

/**
 * @brief Get total memory available for write.
 * Not all memory might be available until
 * "cont_buf_no_padding" command is called.
 */
size_t
cont_buf_get_available_size(const struct cont_buf *src);

bool
cont_buf_is_empty(const struct cont_buf *src);

static inline bool
cont_buf_is_full(const struct cont_buf *src) {
  return cont_buf_get_available_size(src) == 0;
}

//
// Commands
//

/**
 * @brief Removes all content of the buffer
 */
void
cont_buf_clear(struct cont_buf *buf);

/**
 * @brief Change buffer's size
 */
error_t
cont_buf_resize(struct cont_buf *buf, size_t size);

/**
 * @brief Move not read data to the beginning of the buffer
 * making all "available_size" ready for write.
 */
void
cont_buf_no_padding(struct cont_buf *buf);

/**
 * @brief Begin writing to the buffer treating it as an array.
 */
void
cont_buf_write_array_begin(
  struct cont_buf *buf,
  size_t item_size,
  void **data,
  size_t *count);

/**
 * @brief Commit changes started with "cont_buf_write_array_begin".
 */
void
cont_buf_write_array_commit(
  struct cont_buf *src,
  size_t item_size,
  size_t count);

/**
 * @brief Write data up to given size.
 * Function returns number of bytes written.
 */
size_t
cont_buf_write(
  struct cont_buf *buf,
  const void *data,
  size_t size);

/**
 * @brief Get read structure associated with the buffer.
 * Received pointer is owned by writer and it should not be released.
 */
struct cont_buf_read*
cont_buf_read(struct cont_buf *src);

/**
 * @brief Move data from source to output buffer
 * and return number of transfered bytes.
 */
size_t
cont_buf_move(
  struct cont_buf *dest,
  struct cont_buf_read *src);
