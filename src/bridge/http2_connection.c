#include <event.h>
#include <event2/bufferevent_ssl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nghttp2/nghttp2.h>
#include <sys/socket.h>
#include "http2_connection.h"
#include "http2_stream.h"
#include "kp_set.h"
#include "log.h"
#include "mem.h"
#include "nv_set.h"

struct http2_connection {
  struct uri_authority *authority;
  struct http2_request *opening_request;
  struct kp_set *streams;

  event_dns *event_dns;
  struct bufferevent *event_buffer;
  nghttp2_session *http_session;
};

//
// Release
//

static void
http2_session_close(struct http2_connection *conn) {
  kp_set_release(&conn->streams);

  const char *name = uri_authority_get_full_name(conn->authority);
  if (conn->http_session != NULL) {
    log_verbose("HTTP2: Closing session to [%s]", name);
    nghttp2_session_del(conn->http_session);
    conn->http_session = NULL;
  }

  if (conn->event_buffer != NULL) {
    log_verbose("HTTP2: Releasing event's buffer for [%s]", name);
    http2_ssl *ssl = bufferevent_openssl_get_ssl(conn->event_buffer);
    http2_ssl_release(&ssl);

    bufferevent_free(conn->event_buffer);
    conn->event_buffer = NULL;

    event_dns_release(&conn->event_dns);
  }
}

void
http2_connection_release(struct http2_connection **result_r) {
  assert(result_r != NULL);
  struct http2_connection *result = *result_r;
  if (result != NULL) {
    http2_session_close(result);
    http2_request_release(&result->opening_request);

    uri_authority_release(&result->authority);
    free(result);
    *result_r = NULL;
  }
}

//
// Nghttp2 callbacks
//

static error_t
http2_stream_get(
  const char* event_name,
  struct http2_connection *conn,
  int stream_id,
  struct http2_stream **result) {
    *result = (struct http2_stream*)kp_set_get_value(conn->streams, stream_id);
    if (*result == NULL) {
      log_error(
        "HTTP2[%s]: cannot find stream %d",
        event_name,
        stream_id);
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return 0;
  }

static ssize_t
http2_on_send(
  nghttp2_session *session,
  const uint8_t *data,
  size_t length,
  int flags,
  void *ptr) {
    assert(ptr != NULL);
    UNUSED(flags);

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);

    error_t error_r = bufferevent_write(conn->event_buffer, data, length);
    if (error_r == 0) {
      return (ssize_t)length;
    } else {
      log_error("HTTP2: Writing data to eventbuffer failed with %d", error_r);
      return 0;
    }
  }

static error_t
http2_on_begin_headers(
  nghttp2_session *session,
  const nghttp2_frame *frame,
  void *ptr) {
    assert(frame != NULL);
    assert(ptr != NULL);
    const char *callback_name = "on_begin_headers";

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);
    struct http2_stream *stream;

    error_t error_r = http2_stream_get(
      callback_name, conn, frame->hd.stream_id, &stream);
    if (error_r == 0) {
      switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
          // intentionally
          break;
        default:
          log_error(
            "HTTP2: unknown frame type %d in callback %s",
            frame->hd.type, callback_name);
          break;
      }
    }

    return error_r;
  }

static error_t
http2_on_header(
  nghttp2_session *session,
  const nghttp2_frame *frame,
  const uint8_t *name,
  size_t name_len,
  const uint8_t *value,
  size_t value_len,
  uint8_t flags,
  void *ptr) {
    assert(frame != NULL);
    assert(ptr != NULL);
    assert(name != NULL);
    assert(value != NULL);
    UNUSED(flags);
    const char *callback_name = "on_header";

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);
    struct http2_stream *stream;

    error_t error_r = http2_stream_get(
      callback_name, conn, frame->hd.stream_id, &stream);
    if (error_r == 0) {
      switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
          if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
            http2_stream_add_respose_header(
              stream,
              (const char*)name, name_len,
              (const char*)value, value_len);
          } else {
            log_error(
              "HTTP2: unknown header type %d in callback %s",
              frame->headers.cat, callback_name);
          }
          break;
        default:
          log_error(
            "HTTP2: unknown frame type %d in callback %s",
            frame->hd.type, callback_name);
          break;
      }
    }

    return error_r;
  }

static error_t
http2_on_data_chunk_recv(
  nghttp2_session *session,
  uint8_t flags,
  int32_t stream_id,
  const uint8_t *data,
  size_t len,
  void *ptr) {
    assert(data != NULL);
    assert(ptr != NULL);
    UNUSED(flags);
    const char *callback_name = "data_chunk_recv";

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);

    struct http2_stream *stream = NULL;
    error_t error_r = http2_stream_get(
      callback_name,
      conn,
      stream_id,
      &stream);

    if (error_r == 0) {
      if (log_is_verbose()) {
        char *content = NULL;
        error_r = mem_strndup(
          (const char*)data,
          len,
          &content);

        log_verbose("HTTP2: Received data (%d bytes)", len);
        log_verbose(content);
        free(content);
      }
    }

    return error_r != 0 ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
  }

static inline bool
http2_is_default_stream_frame(const nghttp2_frame *frame) {
  int stream_id = frame->hd.stream_id;
  return stream_id == 0;
}

static error_t
http2_on_frame_recv(
  nghttp2_session *session,
  const nghttp2_frame *frame,
  void *ptr) {
    assert(frame != NULL);
    assert(ptr != NULL);
    const char *callback_name = "on_frame_recv";

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);
    if (http2_is_default_stream_frame(frame)) {
      return 0;
    }

    struct http2_stream *stream = NULL;
    error_t error_r = http2_stream_get(
      callback_name,
      conn,
      frame->hd.stream_id,
      &stream);

    if (error_r == 0) {
      switch (frame->hd.type) {
        case NGHTTP2_DATA:
          log_verbose(
            "HTTP2: Receiving data completed for stream %d",
            http2_stream_get_id(stream));
          break;
        case NGHTTP2_HEADERS:
          if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
            if (log_is_verbose()) {
              log_verbose(
                "HTTP2: Response headers received for stream %d:",
                http2_stream_get_id(stream));
              log_verbose_nv_set_content(
                http2_stream_get_response_headers(stream));
            }
          } else {
            log_error(
              "HTTP2: unknown header type %d in callback %s",
              frame->headers.cat, callback_name);
          }
          break;
        default:
          log_error(
            "HTTP2: unknown frame type %d in callback %s",
            frame->hd.type, callback_name);
          break;
      }
    }

    return error_r;
  }

static error_t
http2_stream_close(
  struct http2_connection *conn,
  struct http2_stream *stream,
  uint32_t error_code) {
    // release the stream
    int stream_id = http2_stream_get_id(stream);
    kp_set_remove(conn->streams, stream_id);
    if (error_code == 0) {
      log_verbose("HTTP2: Closing stream %d", stream_id);
    } else {
      log_error("HTTP2: Closing stream %d with code %d", stream_id, error_code);
    }

    error_t error_r = 0;
    if (kp_set_get_length(conn->streams) == 0) {
      // send GOAWAY and tear down the session
      error_t error_r = nghttp2_session_terminate_session(
        conn->http_session,
        NGHTTP2_NO_ERROR);

      if (error_r == 0) {
        log_verbose(
          "HTTP2: Terminating session for [%s]",
          uri_authority_get_full_name(http2_connection_get_authority(conn)));
      } else {
        log_error(
          "HTTP2: terminating session for [%s] failed with error %d",
          uri_authority_get_full_name(http2_connection_get_authority(conn)),
          error_r);
      }
    }
    return error_r != 0 ? NGHTTP2_ERR_CALLBACK_FAILURE : 0;
  }

static error_t
http2_on_stream_close(
  nghttp2_session *session,
  int32_t stream_id,
  uint32_t error_code,
  void *ptr) {
    assert(ptr != NULL);
    const char *callback_name = "on_stream_close";

    struct http2_connection *conn = ptr;
    assert(conn->http_session == session);

    struct http2_stream *stream;
    error_t error_r = http2_stream_get(
      callback_name,
      conn,
      stream_id,
      &stream);

    if (error_r == 0) {
      error_r = http2_stream_close(conn, stream, error_code);
    }
    return error_r;
  }

//
// Event callbacks
//

static error_t
setsockopt_nodelay(int fd) {
  int val = 1;
  error_t error_r = setsockopt(
    fd,
    IPPROTO_TCP,  // level
    TCP_NODELAY,  // option name
    (char*)&val,  // NOLINT
    sizeof(val));
  if (error_r != 0) {
    log_error(
      "HTTP2: Cannot set TCP_NODELAY option due to %s",
      strerror(error_r));
  }
  return error_r;
}

static error_t
http2_session_set_callbacks(struct http2_connection *conn) {
  nghttp2_session_callbacks *callbacks = NULL;
  error_t  error_r = nghttp2_session_callbacks_new(&callbacks);
  if (error_r != 0) {
    log_error(
      "HTTP2: Cannot allocate memory for callbacks due to %s",
      nghttp2_strerror(error_r));
  }

  if (error_r == 0) {
    nghttp2_session_callbacks_set_send_callback(
      callbacks,
      http2_on_send);

    nghttp2_session_callbacks_set_on_frame_recv_callback(
      callbacks,
      http2_on_frame_recv);

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks,
      http2_on_data_chunk_recv);

    nghttp2_session_callbacks_set_on_begin_headers_callback(
      callbacks,
      http2_on_begin_headers);

    nghttp2_session_callbacks_set_on_header_callback(
      callbacks,
      http2_on_header);

    nghttp2_session_callbacks_set_on_stream_close_callback(
      callbacks,
      http2_on_stream_close);

    error_r = nghttp2_session_client_new(
      &conn->http_session,
      callbacks,
      conn);
    if (error_r != 0) {
      log_error(
        "HTTP2: Cannot set nghttp callbacks due to %s",
        nghttp2_strerror(error_r));
    }
  }

  if (callbacks != NULL) {
    nghttp2_session_callbacks_del(callbacks);
  }
  return error_r;
}

static error_t
http2_session_open(struct http2_connection *conn) {
  assert(conn->http_session == NULL);

  log_verbose(
    "HTTP2: Connected to [%s]",
    uri_authority_get_full_name(conn->authority));

  error_t error_r = http2_ssl_select_alpn(
    bufferevent_openssl_get_ssl(conn->event_buffer));
  if (error_r == 0) {
    error_r = setsockopt_nodelay(
      bufferevent_getfd(conn->event_buffer));
  }
  if (error_r == 0) {
    error_r = http2_session_set_callbacks(conn);
  }
  if (error_r == 0) {
    error_r = nghttp2_submit_settings(
      conn->http_session,
      NGHTTP2_FLAG_NONE,
      NULL,
      0);
    if (error_r != 0) {
      log_error(
        "HTTP2: Cannot submit nghttp2 session settings due to %s",
        nghttp2_strerror(error_r));
    }
  }
  if (error_r != 0) {
    http2_session_close(conn);
  }
  return error_r;
}

inline static nghttp2_nv
http2_create_nv(const char* name, const char* value) {
  assert(name != NULL);
  assert(value != NULL);
  return (nghttp2_nv) {
    .name = (uint8_t*)name, // NOLINT
    .namelen = strlen(name),
    .value = (uint8_t*)value, // NOLINT
    .valuelen = strlen(value),
    .flags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE
  }; // NOLINT
}

static error_t
http2_get_request_headers(
  const struct http2_request *request,
  nghttp2_nv **result_r,
  size_t *result_length) {
    const struct nv_set *headers = http2_request_get_headers(request);
    size_t count = nv_set_get_length(headers);
    nghttp2_nv *result = NULL;
    error_t error_r = mem_calloc(
      "nghttp2_nv",
      count * sizeof(nghttp2_nv),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      for (size_t i=0; i < count; ++i) {
        const char* name = nv_set_get_name_at(headers, i);
        result[i] = http2_create_nv(
          name,
          nv_set_get_value_at(headers, i));
      }
    }
    if (error_r == 0) {
      *result_r = result;
      *result_length = count;
    }
    return error_r;
  }

static error_t
http2_frame_serialize(struct http2_connection *conn) {
  error_t error_r = nghttp2_session_send(conn->http_session);
  if (error_r != 0) {
    log_error(
      "HTTP2: Error serializing frame: %s",
      nghttp2_strerror(error_r));
  }
  return error_r;
}

static error_t
http2_send_request(
  struct http2_connection *conn,
  struct http2_request *request,
  struct http2_stream **result_r) {
    int stream_id;
    nghttp2_nv *headers = NULL;
    size_t headers_count;
    error_t error_r = http2_get_request_headers(
      request,
      &headers, &headers_count);

    if (error_r == 0) {
      stream_id = nghttp2_submit_request(
        conn->http_session,
        NULL,
        headers, headers_count,
        NULL,
        conn);
      if (stream_id < 0) {
        log_error(
          "HTTP2: Could not submit request due to %s",
          nghttp2_strerror(stream_id));
        error_r = stream_id;
      }
    }
    if (error_r == 0) {
      error_r = http2_frame_serialize(conn);
    }
    if (error_r == 0) {
      error_r = http2_stream_create(
        stream_id,
        request,
        result_r);
    }

    free(headers);
    return error_r;
  }

static error_t
http2_connect(struct http2_connection *conn) {
  struct http2_stream *stream = NULL;
  error_t error_r = http2_session_open(conn);
  if (error_r == 0) {
    error_r = http2_send_request(
      conn,
      conn->opening_request,
      &stream);
  }

  if (error_r != 0) {
    http2_session_close(conn);
  } else {
    // this will be released with the stream
    conn->opening_request = NULL;

    kp_set_upsert(
      conn->streams,
      http2_stream_get_id(stream),
      stream);
  }
  return error_r;
}

static void
http2event_log_error_network(
  const char* name,
  int16_t events) {
  if (events & BEV_EVENT_READING) {
    log_error("HTTP2: Network error when reading from [%s]", name);
  } else if (events & BEV_EVENT_WRITING) {
    log_error("HTTP2: Network error when writing from [%s]", name);
  } else {
    log_error("HTTP2: Network error for [%s]", name);
  }

  http2_ssl_print_last_ssl_error("HTTP2: SSL last error: %s");
}

static void
http2event_on_event(
  struct bufferevent *event_buffer,
  int16_t events,
  void *ptr) {
    assert(ptr != NULL);

    struct http2_connection *conn = ptr;
    assert(conn->event_buffer == event_buffer);

    if (events & BEV_EVENT_CONNECTED) {
      assert(conn->http_session == NULL);
      http2_connect(conn);
    } else {
      const char* name = uri_authority_get_full_name(conn->authority);
      if (events & BEV_EVENT_EOF) {
        log_verbose("HTTP2: Disconnected from [%s]", name);
      } else if (events & BEV_EVENT_ERROR) {
        http2event_log_error_network(name, events);
      } else if (events & BEV_EVENT_TIMEOUT) {
        log_error("HTTP2: Timeout for remote host: [%s]", name);
      } else {
        log_error(
          "HTTP2: Unknown event %d for remote host [%s]",
          events, name);
      }
      http2_session_close(conn);
    }
  }

static void
http2event_on_read(
  struct bufferevent *event_buffer,
  void *ptr) {
    assert(ptr != NULL);

    struct http2_connection *conn = ptr;
    assert(conn->event_buffer == event_buffer);

    struct evbuffer *input = bufferevent_get_input(event_buffer);
    size_t datalen = evbuffer_get_length(input);
    unsigned char *data = evbuffer_pullup(input, -1);

    ssize_t readlen = nghttp2_session_mem_recv(
      conn->http_session, data, datalen);

    error_t error_r = 0;
    if (readlen < 0) {
      error_r = readlen;
      log_error(
        "HTTP2: Cannot process input due to %s",
        nghttp2_strerror(error_r));
    }
    if (error_r == 0) {
      error_t error_r = evbuffer_drain(input, readlen);
      if (error_r != 0) {
        log_error(
          "HTTP2: Cannot read input due to %s",
          nghttp2_strerror(error_r));
      }
    }
    if (error_r == 0) {
      error_r = http2_frame_serialize(conn);
    }

    if (error_r != 0) {
      http2_session_close(conn);
    }
  }

static void
http2event_on_write(
  struct bufferevent *event_buffer,
  void *ptr) {
    assert(ptr != NULL);

    struct http2_connection *conn = ptr;
    assert(conn->event_buffer == event_buffer);

    bool any_read = nghttp2_session_want_read(conn->http_session) > 0;
    bool any_write = nghttp2_session_want_write(conn->http_session) > 0;
    bool any_buffer = evbuffer_get_length(
      bufferevent_get_output(event_buffer)) > 0;

    if (!any_read && !any_write && !any_buffer) {
      http2_session_close(conn);
    }
  }

//
// API
//

static void
free_stream(void *ptr) {
  struct http2_stream *stream = (struct http2_stream*)ptr;
  http2_stream_release(&stream);
}

error_t
http2_connection_create(
  http2_ssl_ctx *ssl_ctx,
  events_loop *loop,
  struct http2_request *opening_request,
  struct http2_connection **result_r) {
    assert(ssl_ctx != NULL);
    assert(loop != NULL);
    assert(opening_request != NULL);
    assert(result_r != NULL);

    http2_ssl *ssl = NULL;
    struct http2_connection *result = NULL;
    error_t error_r = mem_calloc(
      "http2_connection",
      sizeof(struct http2_connection),
      (void**)&result); // NOLINT

    if (error_r == 0) {
      error_r = uri_authority_copy(
        http2_request_get_authority(opening_request),
        &result->authority);
    }
    if (error_r == 0) {
      error_r = kp_set_create(&result->streams, free_stream);
    }
    if (error_r == 0) {
      error_r = event_dns_create(loop, &result->event_dns);
    }
    if (error_r == 0) {
      error_r = http2_ssl_create(ssl_ctx, &ssl);
    }
    if (error_r == 0) {
      result->event_buffer = bufferevent_openssl_socket_new(
        loop, -1,
        ssl, BUFFEREVENT_SSL_CONNECTING,
        BEV_OPT_DEFER_CALLBACKS | BEV_OPT_CLOSE_ON_FREE);
      if (result->event_buffer == NULL) {
        log_error("HTTP2: Cannot create buffer event");
        error_r = EINVAL;
      }
    }
    if (error_r == 0) {
      bufferevent_setcb(
        result->event_buffer,
        http2event_on_read, http2event_on_write, http2event_on_event,
        result);

      error_r = bufferevent_enable(result->event_buffer, EV_READ | EV_WRITE);
      if (error_r != 0) {
        log_error("HTTP2: Cannot enabled buffer event");
        error_r = EINVAL;
      }
    }
    if (error_r == 0) {
      const char *host = uri_authority_get_host(result->authority);
      int port = uri_authority_get_port(result->authority);
      error_r = bufferevent_socket_connect_hostname(
        result->event_buffer,
        result->event_dns,
        AF_UNSPEC, host, port);
      if (error_r != 0) {
        log_error("Cannot connect to [%s]", host, port);
        error_r = EINVAL;
      }
    }

    if (error_r == 0) {
      result->opening_request = opening_request;
      *result_r = result;
    } else {
      http2_connection_release(&result);
    }
    return error_r;
  }

const struct uri_authority*
http2_connection_get_authority(const struct http2_connection *conn) {
  assert(conn != NULL);
  return conn->authority;
}

/*
error_t
http2_connection_stream_add(
  struct http2_connection *connection,
  struct http2_request_message *request) {
    const struct uri_authority *connection_auth =
      http2_connection_get_authority(connection);
    const struct uri_authority *request_auth =
      http2_request_get_authority(request);
    assert(uri_authority_equals(connection_auth, request_auth));

    error_t error_r = 0;
    if (kp_set_get_length(connection->streams) == 0) {
      connection->opening_request = request;
    } else {
      // TODO
      assert(false);
    }

    return error_r;
  }
*/
