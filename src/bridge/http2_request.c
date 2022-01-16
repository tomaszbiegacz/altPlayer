#include "http2_request.h"
#include "log.h"
#include "nv_set.h"
#include "mem.h"

struct http2_request {
  char *url;
  struct uri_authority *authority;
  struct nv_set *headers;
};

#define HEADER_METHOD     ":method"
#define HEADER_SCHEME     ":scheme"
#define HEADER_AUTHORITY  ":authority"
#define HEADER_PATH       ":path"

const struct uri_authority*
http2_request_get_authority(const struct http2_request *req) {
  assert(req != NULL);
  return req->authority;
}

const struct nv_set*
http2_request_get_headers(const struct http2_request *req) {
  assert(req != NULL);
  return req->headers;
}

error_t
http2_request_create_get(
  const char *uri,
  struct http2_request **result_r) {
    assert(result_r != NULL);

    struct uri_structure *uri_parts = NULL;
    struct http2_request *result = NULL;

    error_t error_r = uri_parse(uri, &uri_parts);
    if (error_r == 0) {
      error_r = mem_calloc(
        "http2_request",
        sizeof(struct http2_request),
        (void**) &result); // NOLINT
    }
    if (error_r == 0) {
      error_r = mem_strdup(uri, &result->url);
    }
    if (error_r == 0) {
      error_r = uri_authority_copy(
        uri_get_authority(uri_parts),
        &result->authority);
    }
    if (error_r == 0) {
      error_r = nv_set_create(&result->headers);
    }
    if (error_r == 0) {
      error_r = nv_set_upsert(result->headers, HEADER_METHOD, "GET");
    }
    if (error_r == 0) {
      error_r = nv_set_upsert(
        result->headers,
        HEADER_SCHEME,
        uri_get_scheme(uri_parts));
    }
    if (error_r == 0) {
      const struct uri_authority *uri_auth = uri_get_authority(uri_parts);
      error_r = nv_set_upsert(
        result->headers,
        HEADER_AUTHORITY,
        uri_authority_get_full_name(uri_auth));
    }
    if (error_r == 0) {
      error_r = nv_set_upsert(
        result->headers,
        HEADER_PATH,
        uri_get_full_path(uri_parts));
    }

    uri_release(&uri_parts);
    if (error_r != 0) {
      http2_request_release(&result);
    } else {
      *result_r = result;
    }
    return error_r;
  }

void
http2_request_release(struct http2_request **result_r) {
  assert(result_r != NULL);
  struct http2_request *result = *result_r;
  if (result != NULL) {
    free(result->url);
    uri_authority_release(&result->authority);
    nv_set_release(&result->headers);
    free(result);
    *result_r = NULL;
  }
}

void
log_verbose_request(const struct http2_request *req) {
  assert(req != NULL);
  if (log_is_verbose()) {
    log_verbose("HTTP2: Request to %s", req->url);
    log_verbose("Headers:");
    log_verbose_nv_set_content(req->headers);
  }
}
