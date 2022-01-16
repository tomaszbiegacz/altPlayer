#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http2_client.h"
#include "log.h"

int
main() {
  struct http2_client *client = NULL;
  struct http2_request *request = NULL;

  log_set_verbose(true);
  error_t error_r = http2_client_global_init();
  if (error_r == 0) {
    error_r = http2_client_create(&client);
  }
  if (error_r == 0) {
    error_r = http2_request_create_get(
      "https://drive.google.com/uc?id=1AZYEK9dWnef9G8DcWD8p5ujBCqh9Ho6f",
      &request);
  }
  if (error_r == 0) {
    error_r = http2_client_request(client, request);
  }

  http2_client_release(&client);
  return error_r;
}
