#include <assert.h>
#include <errno.h>
#include <nghttp2/nghttp2.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "http2_client.h"
#include "http2_connection.h"
#include "http2_ssl.h"
#include "log.h"
#include "uri.h"

struct http2_client {
  events_loop *events_loop;
  http2_ssl_ctx *ssl_context;

  // only one connection is currently supported
  struct http2_connection *conn;
};

static bool _is_global_init = false;


static error_t
disable_signals() {
  struct sigaction act;

  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = SIG_IGN;
  error_t error_r = sigaction(SIGPIPE, &act, NULL);
  if (error_r != 0) {
    error_r = errno;
    log_error("HTTP2: Cannot reset signal due to: %d", error_r);
  }
  return error_r;
}

error_t
http2_client_global_init() {
  error_t error_r = 0;
  if (!_is_global_init) {
    error_r = disable_signals();
    if (error_r == 0) {
      error_r = events_loop_global_init();
    }
    if (error_r == 0) {
      const nghttp2_info *version = nghttp2_version(0);
      if (version != NULL) {
        log_verbose("HTTP2: nghttp2 version: %s", version->version_str);
      }
      _is_global_init = true;
    }
  }
  return error_r;
}



error_t
http2_client_create(struct http2_client **result_r) {
  assert(result_r != NULL);

  struct http2_client *result = calloc(sizeof(struct http2_client), 1);
  if (result == NULL) {
    log_error("HTTP2: Cannot allocate memory for http2_client");
    return ENOMEM;
  }

  error_t error_r =  events_loop_create(&result->events_loop);
  if (error_r == 0) {
    error_r = http2_ssl_ctx_create(&result->ssl_context);
  }
  if (error_r != 0) {
    http2_client_release(&result);
  } else {
    *result_r = result;
  }
  return error_r;
}

void
http2_client_release(struct http2_client **result_r) {
  assert(result_r != NULL);

  struct http2_client *result = *result_r;
  if (result != NULL) {
    http2_ssl_ctx_release(&result->ssl_context);
    events_loop_release(&result->events_loop);
    http2_connection_release(&result->conn);
    free(result);
    *result_r = NULL;
  }
}

error_t
http2_client_request(
  struct http2_client *client,
  struct http2_request *request) {
    assert(client != NULL);
    assert(request != NULL);

    error_t error_r = 0;
    if (client->conn == NULL) {
      error_r = http2_connection_create(
        client->ssl_context,
        client->events_loop,
        request,
        &client->conn);
    } else {
      assert(false);
/*
      const struct uri_authority *auth2 = http2_connection_get_authority(
        client->conn);
      if (!uri_authority_equals(authority, auth2)) {
        log_error("HTTP2: Only one connection per http client is supported");
        error_r = EINVAL;
      }
*/
    }

    // process all events, this is not what we want at the end
    error_r = events_loop_run(client->events_loop);
    return error_r;
  }
