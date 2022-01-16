#include "http2_stream.h"
#include "log.h"
#include "mem.h"
#include "nv_set.h"

struct http2_stream {
  int id;
  struct http2_request* request;
  struct nv_set *response_headers;
};

int
http2_stream_get_id(const struct http2_stream *result) {
  assert(result != NULL);
  return result->id;
}

const struct http2_request*
http2_stream_get_request(const struct http2_stream *result) {
  assert(result != NULL);
  return result->request;
}

error_t
http2_stream_create(
  int id,
  struct http2_request* request,
  struct http2_stream** result_r) {
    assert(request != NULL);
    assert(result_r != NULL);

    struct http2_stream *result = NULL;
    error_t error_r = mem_calloc(
      "http2_stream",
      sizeof(struct http2_stream),
      (void**) &result); // NOLINT

    if (error_r == 0) {
      error_r = nv_set_create(&result->response_headers);
    }
    if (error_r == 0) {
      result->id = id;
      result->request = request;
      *result_r = result;

      log_verbose("HTTP2: Opened stream %d", id);
      log_verbose_request(result->request);
    } else {
      http2_stream_release(&result);
    }
    return error_r;
  }

void
http2_stream_release(struct http2_stream **result_r) {
  assert(result_r != NULL);

  struct http2_stream *result = *result_r;
  if (result != NULL) {
    http2_request_release(&result->request);
    nv_set_release(&result->response_headers);
    free(result);
    *result_r = NULL;
  }
}

error_t
http2_stream_add_respose_header(
  struct http2_stream *stream,
  const char* name_start,
  size_t name_length,
  const char* value_start,
  size_t value_length) {
    assert(stream != NULL);
    assert(name_start != NULL);
    assert(value_start != NULL);

    char *name = NULL;
    char *value = NULL;
    error_t error_r = mem_strndup(name_start, name_length, &name);
    if (error_r == 0) {
      error_r = mem_strndup(value_start, value_length, &value);
    }
    if (error_r == 0) {
      error_r = nv_set_upsert(stream->response_headers, name, value);
    }

    free(name);
    free(value);
    return error_r;
  }

const struct nv_set*
http2_stream_get_response_headers(const struct http2_stream *stream) {
  assert(stream != NULL);
  return stream->response_headers;
}
