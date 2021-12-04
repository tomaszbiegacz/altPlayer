#include "SharedTestFixture.h"

extern "C" {
  #include "pcm.h"
}

TEST_F(SharedTestFixture, pcm_guess_format_TEST_basic) {
  enum pcm_format result;
  EXPECT_EQ(0, pcm_guess_format("/test.wav", &result));
  EXPECT_EQ(pcm_format_wav, result);

  EXPECT_EQ(0, pcm_guess_format("other.flac", &result));
  EXPECT_EQ(pcm_format_flac, result);

  EXPECT_NE(0, pcm_guess_format("other.unk", &result));
}

TEST_F(SharedTestFixture, pcm_validate_wav_content_TEST_basic) {
  EMPTY_STRUCT(io_rf_stream, stream);
  struct pcm_decoder *decoder = NULL;

  EXPECT_EQ(0, io_rf_stream_open_file("test.wav", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_wav_open(&stream, 1, &decoder));
  EXPECT_EQ(1, decoder->spec.channels_count);
  EXPECT_EQ(22050, decoder->spec.samples_per_sec);
  EXPECT_EQ(16, decoder->spec.bits_per_sample);
  EXPECT_EQ(0, decoder->spec.data_size % pcm_frame_size(&decoder->spec));
  EXPECT_EQ(3000000, pcm_buffer_time_us(&decoder->spec, decoder->spec.data_size));

  decoder->release(&decoder);
  EXPECT_EQ(NULL, decoder);
  io_rf_stream_free(&stream);
}
