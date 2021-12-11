#include "SharedTestFixture.h"

extern "C" {
  #include "player.h"
}

TEST_F(SharedTestFixture, player_open_TEST_open) {
  EMPTY_STRUCT(player_parameters, params);
  EMPTY_STRUCT(io_rf_stream, stream);
  struct pcm_decoder *decoder = NULL;
  struct player *player = NULL;

  EXPECT_EQ(0, io_rf_stream_open_file("test.wav", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_wav_open(&stream, 4096, &decoder));
  EXPECT_EQ(0, player_open(&params, decoder, &player));

  pcm_decoder_decode_release(&decoder);
  io_rf_stream_free(&stream);
  player_release(&player);
}
