#include "../SharedTestFixture.h"

extern "C" {
  #include "pcm/pcm_spec.h"
}

TEST(pcm__pcm_spec, setup_minimum) {
  struct pcm_spec_builder *builder = NULL;
  EXPECT_EQ(0, pcm_spec_builder_create(&builder));
  pcm_spec_set_channels_count(builder, 1);
  pcm_spec_set_bits_per_sample(builder, 16);
  pcm_spec_set_samples_per_sec(builder, 3);
  pcm_spec_set_frames_count(builder, 4);

  struct pcm_spec *spec = NULL;
  EXPECT_EQ(0, pcm_spec_get(builder, &spec));
  pcm_spec_builder_release(&builder);

  EXPECT_EQ(false, pcm_spec_is_big_endian(spec));
  EXPECT_EQ(false, pcm_spec_is_signed(spec));
  EXPECT_EQ(1, pcm_spec_get_channels_count(spec));
  EXPECT_EQ(2, pcm_spec_get_bytes_per_sample(spec));
  EXPECT_EQ(3, pcm_spec_get_samples_per_sec(spec));
  EXPECT_EQ(4, pcm_spec_get_frames_count(spec));

  EXPECT_EQ(16, pcm_spec_get_bits_per_sample(spec));
  EXPECT_EQ(2, pcm_spec_get_frame_size(spec));
}

TEST(pcm__pcm_spec, setup_full) {
  struct pcm_spec_builder *builder = NULL;
  EXPECT_EQ(0, pcm_spec_builder_create(&builder));
  pcm_spec_set_big_endian(builder);
  pcm_spec_set_signed(builder);
  pcm_spec_set_channels_count(builder, 2);
  pcm_spec_set_bits_per_sample(builder, 24);
  pcm_spec_set_samples_per_sec(builder, 4);
  EXPECT_EQ(0, pcm_spec_set_data_size(builder, 6));

  struct pcm_spec *spec = NULL;
  EXPECT_EQ(0, pcm_spec_get(builder, &spec));
  pcm_spec_builder_release(&builder);

  EXPECT_EQ(true, pcm_spec_is_big_endian(spec));
  EXPECT_EQ(true, pcm_spec_is_signed(spec));
  EXPECT_EQ(2, pcm_spec_get_channels_count(spec));
  EXPECT_EQ(3, pcm_spec_get_bytes_per_sample(spec));
  EXPECT_EQ(4, pcm_spec_get_samples_per_sec(spec));
  EXPECT_EQ(1, pcm_spec_get_frames_count(spec));

  EXPECT_EQ(24, pcm_spec_get_bits_per_sample(spec));
  EXPECT_EQ(6, pcm_spec_get_frame_size(spec));
}
