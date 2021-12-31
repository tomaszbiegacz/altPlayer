#include "SharedTestFixture.h"

extern "C" {
  #include "alsa_player.h"
  #include "pcm_decoder_wav.h"
  #include "timer.h"
}

TEST(alsa_player, alsa_player_open) {
  EMPTY_STRUCT(alsa_player_parameters, params);
  struct alsa_player *player = NULL;

  EXPECT_EQ(0, alsa_player_open(&params, &player));

  alsa_player_release(&player);
}

TEST(alsa_player, alsa_player_start) {
  EMPTY_STRUCT(alsa_player_parameters, params);
  struct alsa_player *player = NULL;
  struct io_rf_stream *stream = NULL;
  struct pcm_decoder *decoder = NULL;

  EXPECT_EQ(0, alsa_player_open(&params, &player));
  EXPECT_EQ(0, io_rf_stream_open_file("test.wav", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_wav_open(stream, &decoder));
  EXPECT_EQ(0, alsa_player_start(player, decoder));

  struct alsa_player_playback_status status;
  EXPECT_EQ(0, alsa_player_get_playback_status(player, &status));
  EXPECT_EQ(false, status.is_complete);
  EXPECT_EQ(0, status.position.tv_sec);

  alsa_player_release(&player);
  EXPECT_EQ(NULL, player);
}
