#pragma once
#include "uri.h"

/**
 * @brief http request part of http/2 stream
 *
 */
struct http2_request;

//
// Setup
//

error_t
http2_request_create_get(
  const char *uri,
  struct http2_request **result_r);

void
http2_request_release(struct http2_request **result_r);

//
// Query
//

const struct uri_authority*
http2_request_get_authority(const struct http2_request *req);

const struct nv_set*
http2_request_get_headers(const struct http2_request *req);

void
log_verbose_request(const struct http2_request *req);
