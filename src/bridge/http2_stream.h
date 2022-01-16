#ifndef PLAYER_HTTP2_STREAM_H_
#define PLAYER_HTTP2_STREAM_H_

#include "http2_request.h"

/**
 * @brief http/2 stream
 *
 */
struct http2_stream;

//
// Setup
//

error_t
http2_stream_create(
  int id,
  struct http2_request* request,
  struct http2_stream** result_r);

void
http2_stream_release(struct http2_stream **result_r);

//
// Command
//

error_t
http2_stream_add_respose_header(
  struct http2_stream *stream,
  const char* name_start,
  size_t name_length,
  const char* value_start,
  size_t value_length);

//
// Query
//

int
http2_stream_get_id(const struct http2_stream *result);

const struct http2_request*
http2_stream_get_request(const struct http2_stream *result);

const struct nv_set*
http2_stream_get_response_headers(const struct http2_stream *stream);

#endif
