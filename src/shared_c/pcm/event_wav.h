#pragma once

/**
 * @brief Configuration for pipe reading data from file
 * @param file_path source file path
 * @param buffer_size read buffer size, defaults to 4k
 * @param max_read_size maximum read size in one iteration, defaults to 4k
 */
struct event_pipe_file_config {
  const char *file_path;
  size_t buffer_size;
  size_t max_read_size;
};

/**
 * @brief Create pipe reading data from file
 */
error_t
event_pipe_from_file(
  struct event_base *loop,
  const struct event_pipe_file_config *config,
  struct event_pipe **result_r);

/**
 * @brief Create sink into file
 */
error_t
event_sink_into_file(
  struct event_pipe *source,
  const char *file_path,
  struct event_sink **result_r);
