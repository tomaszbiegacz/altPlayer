#ifndef PLAYER_HTTP2_CONNECTION_H_
#define PLAYER_HTTP2_CONNECTION_H_

#include "event/event_loop.h"
#include "http2_ssl.h"
#include "http/http2_request.h"
#include "http/uri.h"

/**
 * @brief Single http connection is always connected to exacly one host
 *
 */
struct http2_connection;

//
// Setup
//

error_t
http2_connection_create(
  http2_ssl_ctx *ssl_ctx,
  events_loop *loop,
  struct http2_request *opening_request,
  struct http2_connection **result_r);

void
http2_connection_release(struct http2_connection **result_r);


//
// Query
//

const struct uri_authority*
http2_connection_get_authority(const struct http2_connection *conn);

//
// Commands
//

#endif
