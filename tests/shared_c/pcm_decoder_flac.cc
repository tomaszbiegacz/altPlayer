#include "SharedTestFixture.h"

extern "C" {
  #include "pcm_decoder_flac.h"
  #include "timer.h"
}

TEST(pcm_decoder_flac, pcm_decoder_flac_open) {
  struct io_rf_stream *stream = NULL;
  struct pcm_decoder *decoder = NULL;

  EXPECT_EQ(0, io_rf_stream_open_file("test.flac", 1024, 4096, &stream));
  EXPECT_EQ(0, pcm_decoder_flac_open(stream, &decoder));

  const struct pcm_spec* spec = pcm_decoder_get_spec(decoder);
  EXPECT_EQ(1, pcm_spec_get_channels_count(spec));
  EXPECT_EQ(false, pcm_spec_is_big_endian(spec));
  EXPECT_EQ(true, pcm_spec_is_signed(spec));
  EXPECT_EQ(22050, pcm_spec_get_samples_per_sec(spec));
  EXPECT_EQ(16, pcm_spec_get_bits_per_sample(spec));

  EXPECT_GT(decoder->block_size, 2);
  EXPECT_EQ(0, decoder->block_size % 2);
  EXPECT_EQ(3 * 1000, timespec_get_miliseconds(
    pcm_spec_get_time(pcm_decoder_get_spec(decoder))));

  pcm_decoder_release(&decoder);
  EXPECT_EQ(NULL, decoder);
}
