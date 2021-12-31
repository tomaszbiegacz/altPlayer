#include "SharedTestFixture.h"

extern "C" {
  #include "pcm_file.h"
}

TEST(pcm_file, pcm_file_format_guess) {
  enum pcm_file_format result;
  EXPECT_EQ(0, pcm_file_format_guess("/test.wav", &result));
  EXPECT_EQ(pcm_file_format_wav, result);

  EXPECT_EQ(0, pcm_file_format_guess("other.flac", &result));
  EXPECT_EQ(pcm_file_format_flac, result);

  EXPECT_NE(0, pcm_file_format_guess("other.unk", &result));
}
