#ifndef _H_PLAYER
#define _H_PLAYER

struct player_config {
  char* file_path;
};

void
player_free_config(struct player_config *config);

#endif
