#include <assert.h>
#include <nghttp2/nghttp2.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include "http2_ssl.h"
#include "log.h"

// see also https://www.openssl.org/docs/man1.1.1/man3/SSL_get_error.html

inline static error_t
log_ssl_error(int error, const char *message) {
  if (error != 0) {
    char error_info[50];
    ERR_error_string_n(error, error_info, sizeof(error_info));
    log_error(message, error_info);
  }
  return error;
}

void
http2_ssl_print_last_ssl_error(const char* message) {
  log_ssl_error(ERR_peek_last_error(), message);
}

inline static error_t
log_ssl_error_last(bool condition, const char *message) {
  if (!condition) {
    int error = ERR_get_error();
    if (error == 0) {
      log_error("[Unknown error] %s", message);
      return EINVAL;
    } else {
      return log_ssl_error(error, message);
    }
  } else {
    return 0;
  }
}

static error_t
http2_on_next_protocol_cb(
  SSL *ssl,
  unsigned char **out,
  unsigned char *outlen,
  const unsigned char *in,
  unsigned int inlen,
  void *arg) {
    UNUSED(ssl);
    UNUSED(arg);

    if (nghttp2_select_next_protocol(out, outlen, in, inlen) <= 0) {
      log_error("HTTP2: Server did not advertise %s", NGHTTP2_PROTO_VERSION_ID);
    }
    return SSL_TLSEXT_ERR_OK;
  }

error_t
http2_ssl_ctx_create(http2_ssl_ctx **result_r) {
  assert(result_r != NULL);
  assert(OPENSSL_VERSION_NUMBER >= 0x10002000L);

  SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
  error_t error_r = log_ssl_error_last(
    ssl_ctx != NULL,
    "Could not create SSL/TLS context: %s");

  if (error_r == 0) {
    (void)SSL_CTX_set_options(
      ssl_ctx,
      SSL_OP_ALL
        | SSL_OP_NO_SSLv2
        | SSL_OP_NO_SSLv3
        | SSL_OP_NO_COMPRESSION
        | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

    error_r = log_ssl_error(
      SSL_CTX_set_alpn_protos(ssl_ctx, (const unsigned char *)"\x02h2", 3),
      "Unable to set alpn negotiation");
  }

  if (error_r == 0) {
    SSL_CTX_set_next_proto_select_cb(
      ssl_ctx,
      http2_on_next_protocol_cb,
      NULL);
  }

  if (error_r == 0) {
    *result_r = ssl_ctx;
  } else if (ssl_ctx != NULL) {
    SSL_CTX_free(ssl_ctx);
  }
  return error_r;
}

void
http2_ssl_ctx_release(http2_ssl_ctx **result_r) {
  assert(result_r != NULL);

  SSL_CTX* result = *result_r;
  if (result != NULL) {
    SSL_CTX_free(result);
    *result_r = NULL;
  }
}

error_t
http2_ssl_create(http2_ssl_ctx* ctx, http2_ssl **result_r) {
  assert(ctx != NULL);
  assert(result_r != NULL);

  SSL* result = SSL_new(ctx);
  error_t error_r = log_ssl_error_last(
    result != NULL,
    "Could not create SSL/TLS session: %s");

  *result_r = result;
  return error_r;
}

void
http2_ssl_release(http2_ssl **result_r) {
  assert(result_r != NULL);

  SSL* result = *result_r;
  if (result != NULL) {
    SSL_shutdown(result);
    *result_r = NULL;
  }
}

error_t
http2_ssl_select_alpn(http2_ssl *ssl) {
  assert(ssl != NULL);

  error_t error_r = 0;
  const unsigned char *alpn = NULL;
  unsigned int alpnlen = 0;

  SSL_get0_next_proto_negotiated(ssl, &alpn, &alpnlen);
  if (alpn == NULL) {
    SSL_get0_alpn_selected(ssl, &alpn, &alpnlen);
  }
  if (alpn == NULL || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
    log_error("HTTP2: h2 is not negotiated");
    error_r = EINVAL;
  }
  return error_r;
}
