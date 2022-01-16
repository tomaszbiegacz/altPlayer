#ifndef PLAYER_HTTP2_SSL_H_
#define PLAYER_HTTP2_SSL_H_

#include "shrdef.h"

typedef struct ssl_ctx_st http2_ssl_ctx;
typedef struct ssl_st     http2_ssl;

//
// Setup
//

error_t
http2_ssl_ctx_create(http2_ssl_ctx **result_r);

void
http2_ssl_ctx_release(http2_ssl_ctx **result_r);


error_t
http2_ssl_create(http2_ssl_ctx *ctx, http2_ssl **result_r);

void
http2_ssl_release(http2_ssl **result_r);

//
// Commands
//

error_t
http2_ssl_select_alpn(http2_ssl *ssl);

//
// Queries
//

void
http2_ssl_print_last_ssl_error(const char* message);

#endif
