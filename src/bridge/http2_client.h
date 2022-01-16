#ifndef PLAYER_HTTP2_CLIENT_H_
#define PLAYER_HTTP2_CLIENT_H_

#include "http2_request.h"

/**
 * @brief Http client that can potentially handle multiple http/2 streams
 * in parallel with the help of event loop abstraction.
 */
struct http2_client;

//
// Setup
//

error_t
http2_client_global_init();

error_t
http2_client_create(struct http2_client **result_r);

void
http2_client_release(struct http2_client **result_r);

//
// Commands
//

error_t
http2_client_request(
  struct http2_client *client,
  struct http2_request *request);

#endif
