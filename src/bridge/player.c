#include <assert.h>
#include <stdlib.h>
#include "./player.h"

void
player_free_config(struct player_config *config) {
  assert(config != NULL);

  if (config->file_path != NULL) {
    free(config->file_path);
    config->file_path = NULL;
  }
}
