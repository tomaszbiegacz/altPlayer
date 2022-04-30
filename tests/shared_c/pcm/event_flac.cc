#include "../TestFile.h"

extern "C" {
  #include "file/event_file.h"
  #include "pcm/event_flac.h"
  #include "pcm/event_wav.h"
}

TEST(pcm__event_flac, flac_2_wav) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = "test.flac";
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // flac decoder
  struct event_pcm *flac_in = NULL;
  EXPECT_EQ(0, event_flac_decode(source, &flac_in));

  // wave encoder
  struct event_pipe *wav_out = NULL;
  EXPECT_EQ(0, event_wav_encode(flac_in, &wav_out));

  // output
  struct event_sink *output = NULL;
  char *output_path = TestFile::GetTempFilePath("output");
  EXPECT_EQ(0, event_sink_into_file(wav_out, output_path, &output));

  // run & check
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));

  struct event_pipe *flac_in_pipe = event_pcm_get_pipe(flac_in);
  EXPECT_EQ(true, event_pipe_is_empty(flac_in_pipe));
  EXPECT_EQ(true, event_pipe_is_end(flac_in_pipe));

  EXPECT_EQ(true, event_pipe_is_empty(wav_out));
  EXPECT_EQ(true, event_pipe_is_end(wav_out));

  EXPECT_EQ(true, event_sink_is_end(output));
  EXPECT_EQ(true, TestFile::CompareFiles("test.wav", output_path));

  // release it all
  free(output_path);
  event_pipe_release(&source);
  event_loop_release(&loop);
}
