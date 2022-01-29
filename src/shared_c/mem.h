#pragma once
#include "shrdef.h"

/**
 * @brief Allocate new region of memory with given size
 *
 * @param resource_name used for loging failure
 */
error_t
mem_calloc(const char *resource_name, size_t size, void **result_r);

void
mem_free(void **result_r);


/**
 * @brief Allocate private map of memory
 *
 * @param resource_name used for loggin failure
 */
error_t
mem_map_alloc(const char *resource_name, size_t size, void **result_r);

void
mem_map_free(size_t size, void **result_r);


//
// String
//

error_t
mem_strdup(const char *source, char **result_r);

error_t
mem_strndup(const char *source, size_t len, char **result_r);


//
// Pointer list
//

/**
 * @brief Extends list of pointers to new size
 *
 * @param base reference of pointer to the first element in the array
 * @param new_length new number of items in the array
 * @return error_t
 */
error_t
mem_list_extend(
  void ***base_r,
  size_t new_length);

/**
 * @brief Insert new pointer at given position in the list
 *
 * @param base pointer to the first element in the array
 * @param nitems number of items in the array
 * @param pos where to place item
 * @param item new element to insert
 * @return error_t
 */
static inline void
mem_list_insert_at(
  void **base,
  size_t nitems,
  size_t pos,
  void *item) {
    assert(base != NULL);
    assert(pos <= nitems);
    assert(item != NULL);

    size_t to_move = (nitems - pos) * sizeof(void*);
    if (to_move > 0) {
      memmove(base + pos + 1, base + pos, to_move);
    }
    base[pos] = item;
  }

/**
 * @brief Insert new pointer at given position in the list
 *
 * @param base pointer to the first element in the array
 * @param nitems number of items in the array
 * @param pos where to place item
 * @param item new element to insert
 * @return error_t
 */
static inline void
mem_list_remove_at(
  void **base,
  size_t nitems,
  size_t pos) {
    assert(base != NULL);
    assert(pos < nitems);

    size_t to_move = (nitems - pos - 1) * sizeof(void*);
    if (to_move > 0) {
      memmove(base + pos, base + pos + 1, to_move);
    }
  }

/**
 * @brief Free list of pointers.
 * Before releasing list itself, free memory occupied by each pointer
 *
 * @param base pointer to the first element in the array
 * @param length number of pointers in array that's memory should be released
 * @param ptr_free function used for freeing memory occupied by pointer
 */
void
mem_list_free(
  void **base,
  size_t length,
  void (*ptr_free)(void *ptr));


//
// int64 list
//

/**
 * @brief Extends list of int to new size
 *
 * @param base reference of pointer to the first element in the array
 * @param new_length new number of items in the array
 * @return error_t
 */
error_t
mem_list_i64_extend(
  int64_t **base_r,
  size_t new_length);

/**
 * @brief Insert int at given position in the list
 *
 * @param base pointer to the first element in the array
 * @param nitems number of items in the array
 * @param pos where to place item
 * @param item new element to insert
 * @return error_t
 */
static inline void
mem_list_i64_insert_at(
  int64_t *base,
  size_t nitems,
  size_t pos,
  int item) {
    assert(base != NULL);
    assert(pos <= nitems);

    size_t to_move = (nitems - pos) * sizeof(int64_t);
    if (to_move > 0) {
      memmove(base + pos + 1, base + pos, to_move);
    }
    base[pos] = item;
  }

/**
 * @brief Insert new pointer at given position in the list
 *
 * @param base pointer to the first element in the array
 * @param nitems number of items in the array
 * @param pos where to place item
 * @param item new element to insert
 * @return error_t
 */
static inline void
mem_list_i64_remove_at(
  int64_t *base,
  size_t nitems,
  size_t pos) {
    assert(base != NULL);
    assert(pos < nitems);

    size_t to_move = (nitems - pos - 1) * sizeof(int64_t);
    if (to_move > 0) {
      memmove(base + pos, base + pos + 1, to_move);
    }
  }
