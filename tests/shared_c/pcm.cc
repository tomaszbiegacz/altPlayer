#include "SharedTestFixture.h"

extern "C" {
  #include "pcm.h"
}

TEST_F(SharedTestFixture, pcm_validate_wav_content_TEST_basic) {
  EMPTY_STRUCT(io_rf_stream, stream);
  EMPTY_STRUCT(pcm_spec, pcm);

  EXPECT_EQ(0, io_rf_stream_open_file("test.wav", 1024, &stream));
  EXPECT_EQ(0, pcm_validate_wav_content(&stream, 100, &pcm, NULL));
  EXPECT_EQ(1, pcm.channels_count);
  EXPECT_EQ(22050, pcm.samples_per_sec);
  EXPECT_EQ(16, pcm.bits_per_sample);
  EXPECT_EQ(0, pcm.data_size % pcm_frame_size(&pcm));
  EXPECT_EQ(3000000, pcm_buffer_time_us(&pcm, pcm.data_size));

  io_rf_stream_free(&stream);
}

TEST_F(SharedTestFixture, pcm_guess_format_TEST_basic) {
  enum pcm_format result;
  EXPECT_EQ(0, pcm_guess_format("/test.wav", &result));
  EXPECT_EQ(pcm_format_wav, result);

  EXPECT_EQ(0, pcm_guess_format("other.flac", &result));
  EXPECT_EQ(pcm_format_flac, result);

  EXPECT_NE(0, pcm_guess_format("other.unk", &result));
}
