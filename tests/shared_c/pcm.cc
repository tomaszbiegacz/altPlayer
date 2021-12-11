#include "SharedTestFixture.h"

extern "C" {
  #include "pcm.h"
  #include "flac.h"
}

TEST_F(SharedTestFixture, pcm_guess_format_TEST_basic) {
  enum pcm_format result;
  EXPECT_EQ(0, pcm_guess_format("/test.wav", &result));
  EXPECT_EQ(pcm_format_wav, result);

  EXPECT_EQ(0, pcm_guess_format("other.flac", &result));
  EXPECT_EQ(pcm_format_flac, result);

  EXPECT_NE(0, pcm_guess_format("other.unk", &result));
}

TEST_F(SharedTestFixture, pcm_decoder_wav_open_TEST_basic) {
  EMPTY_STRUCT(io_rf_stream, stream);
  struct pcm_decoder *decoder = NULL;

  EXPECT_EQ(0, io_rf_stream_open_file("test.wav", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_wav_open(&stream, 1, &decoder));
  EXPECT_EQ(1, decoder->spec.channels_count);
  EXPECT_EQ(false, decoder->spec.is_big_endian);
  EXPECT_EQ(true, decoder->spec.is_signed);
  EXPECT_EQ(22050, decoder->spec.samples_per_sec);
  EXPECT_EQ(16, decoder->spec.bits_per_sample);
  EXPECT_EQ(2, decoder->block_size);
  EXPECT_EQ(3000000, pcm_buffer_time_us(
    &decoder->spec,
    decoder->spec.samples_count * pcm_frame_size(&decoder->spec)));

  struct timespec recordTime = pcm_decoder_get_total_time(decoder);
  EXPECT_EQ(0, recordTime.tv_nsec);
  EXPECT_EQ(3, recordTime.tv_sec);

  decoder->release(&decoder);
  EXPECT_EQ(NULL, decoder);
  io_rf_stream_free(&stream);
}

TEST_F(SharedTestFixture, pcm_decoder_flac_open_TEST_basic) {
  EMPTY_STRUCT(io_rf_stream, stream);
  struct pcm_decoder *decoder = NULL;

  EXPECT_EQ(0, io_rf_stream_open_file("test.flac", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_flac_open(&stream, 1, &decoder));
  EXPECT_EQ(1, decoder->spec.channels_count);
  EXPECT_EQ(false, decoder->spec.is_big_endian);
  EXPECT_EQ(true, decoder->spec.is_signed);
  EXPECT_EQ(22050, decoder->spec.samples_per_sec);
  EXPECT_EQ(16, decoder->spec.bits_per_sample);
  EXPECT_GT(decoder->block_size, 2);
  EXPECT_EQ(0, decoder->block_size % 2);
  EXPECT_EQ(3000000, pcm_buffer_time_us(
    &decoder->spec,
    decoder->spec.samples_count * pcm_frame_size(&decoder->spec)));

  decoder->release(&decoder);
  EXPECT_EQ(NULL, decoder);
  io_rf_stream_free(&stream);
}
