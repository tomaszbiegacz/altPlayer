#include <assert.h>
#include <errno.h>
#include <uriparser/Uri.h>
#include "uri.h"
#include "log.h"
#include "mem.h"

static inline size_t
text_range_length(UriTextRangeA r) {
  return r.afterLast - r.first;
}

static inline char*
text_range_strdup(UriTextRangeA r) {
  return strndup(r.first, text_range_length(r));
}

error_t
uri_parse(const char *uri_str, struct uri_structure **result_r) {
  assert(result_r != NULL);

  UriUriA uri;
  const char *errorPos;
  error_t error_r = uriParseSingleUriA(&uri, uri_str, &errorPos);
  if (error_r != URI_SUCCESS) {
    log_error(
      "URI: Parsing URL failed with %d at position '%s'",
      error_r, errorPos);
    return EINVAL;
  }

  struct uri_structure *result = 0;
  error_r = mem_calloc(
    "uri_structure",
    sizeof(struct uri_structure),
    (void**)&result); // NOLINT

  if (error_r == 0) {
    bool is_https = strncmp(
      "https",
      uri.scheme.first,
      text_range_length(uri.scheme)) == 0;
    if  (!is_https) {
      log_error("URI: Only https protocol is supported.");
      error_r = EINVAL;
    }
  }

  if (error_r == 0) {
    result->authority.host = text_range_strdup(uri.hostText);
    if (result->authority.host == NULL) {
      log_error("URI: Cannnot allocate memory for hostname");
      error_r = ENOMEM;
    }
  }

  if (error_r == 0) {
    if (text_range_length(uri.portText) > 0) {
      result->authority.port = atoi(uri.portText.first);
      if (result->authority.port <= 0) {
        log_error("URI: Invalid port in %s", uri_str);
        error_r = EINVAL;
      }
    } else {
      result->authority.port = 443;
    }
  }

  if (error_r == 0) {
    if (text_range_length(uri.portText) > 0) {
      result->authority.full_name = strndup(
        uri.hostText.first,
        uri.portText.afterLast - uri.hostText.first);
    } else {
      result->authority.full_name = strdup(result->authority.host);
    }

    if (result->authority.full_name == NULL) {
      log_error("URI: Cannnot allocate memory for authority");
      error_r = ENOMEM;
    }
  }

  if (error_r == 0) {
    if (uri.pathHead->text.first != uri.pathTail->text.afterLast) {
      result->path = strndup(
        uri.pathHead->text.first - 1,
        uri.pathTail->text.afterLast - uri.pathHead->text.first + 1);
    } else {
      // If we don't have path in URI, we use "/" as path
      result->path = strdup("/");
    }

    if (result->path == NULL) {
      log_error("URI: Cannnot allocate memory for path");
      error_r = ENOMEM;
    }
  }

  if (error_r == 0) {
    result->query = text_range_strdup(uri.query);

    if (result->query == NULL) {
      log_error("URI: Cannnot allocate memory for query");
      error_r = ENOMEM;
    }
  }

  if (error_r == 0) {
    if (uri.pathHead->text.first != uri.query.first) {
      result->full_path = strndup(
        uri.pathHead->text.first - 1,
        uri.query.afterLast - uri.pathHead->text.first + 1);
    } else {
      // If we don't have path in URI, we use "/" as path
      result->path = strdup("/");
    }

    if (result->full_path == NULL) {
      log_error("URI: Cannnot allocate memory for path_with_query");
      error_r = ENOMEM;
    }
  }

  uriFreeUriMembersA(&uri);

  if (error_r == 0) {
    *result_r = result;
  } else {
    uri_release(&result);
  }
  return error_r;
}

void
uri_authority_release(struct uri_authority **result_r) {
  assert(result_r != NULL);
  struct uri_authority *result = *result_r;
  if (result != NULL) {
    free(result->host);
    free(result);
    *result_r = NULL;
  }
}

void
uri_release(struct uri_structure **result_r) {
  assert(result_r != NULL);
  struct uri_structure *result = *result_r;
  if (result != NULL) {
    free(result->authority.host);
    free(result->authority.full_name);
    free(result->path);
    free(result->query);
    free(result->full_path);
    free(result);
    *result_r = NULL;
  }
}

error_t
uri_authority_copy(
  const struct uri_authority *uri,
  struct uri_authority **result_r) {
    assert(uri != NULL);
    assert(result_r != NULL);

    struct uri_authority *result = calloc(sizeof(struct uri_authority), 1);
    if (result == NULL) {
      log_error("URI: Cannot allocate memory for uri_authority");
      return ENOMEM;
    }

    error_t error_r = 0;
    result->port = uri->port;

    result->host = strdup(uri->host);
    if (result->host == NULL) {
      log_error("URI: Cannot copy host of uri_authority");
      error_r = ENOMEM;
    }

    result->full_name = strdup(uri->full_name);
    if (result->full_name == NULL) {
      log_error("URI: Cannot copy full_name of uri_authority");
      error_r = ENOMEM;
    }

    if (error_r == 0) {
      *result_r = result;
    } else {
      uri_authority_release(&result);
    }
    return error_r;
  }

bool
uri_authority_equals(
  const struct uri_authority *u1,
  const struct uri_authority *u2) {
    assert(u1 != NULL);
    assert(u2 != NULL);
    return strcasecmp(u1->host, u2->host) == 0 && u1->port == u2->port;
  }
