#ifndef PLAYER_MAIN_H_
#define PLAYER_MAIN_H_

#include "pcm_file.h"
#include "alsa_player.h"

struct bridge_config {
  char *file_path;
  size_t io_buffer_size;
  enum pcm_file_format pcm_format;
  char *alsa_hadrware;
  bool alsa_resampling;
};

error_t
list_sound_cards();

error_t
play_file(struct bridge_config *config);

#endif
