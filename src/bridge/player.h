#ifndef _H_PLAYER
#define _H_PLAYER

#include "./io.h"

struct player_config {
  struct io_memory_block song;
};

void
player_free_player_config(struct player_config *config);

#endif
