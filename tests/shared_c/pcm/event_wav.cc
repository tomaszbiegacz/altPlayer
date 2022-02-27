#include "../TestFile.h"
#include "../event/TestMemorySink.h"

extern "C" {
  #include "timer.h"
  #include "file/event_file.h"
  #include "pcm/event_wav.h"
}

TEST(pcm__event_wav, read_wav) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = "test.wav";
  source_conf.buffer_size = 1;  // force resize
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // wave parser
  struct event_pcm *wav = NULL;
  EXPECT_EQ(0, event_wav_decode(source, &wav));

  // run & check
  struct event_pipe *wav_pipe = event_pcm_get_pipe(wav);
  TestMemorySink sink = TestMemorySink(wav_pipe, 200 * 1024);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));

  EXPECT_EQ(true, event_pipe_is_empty(wav_pipe));
  EXPECT_EQ(true, event_pipe_is_end(wav_pipe));

  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  struct pcm_spec *spec = NULL;
  EXPECT_EQ(0, event_pcm_get_spec(wav, &spec));
  EXPECT_EQ(1, pcm_spec_get_channels_count(spec));
  EXPECT_EQ(false, pcm_spec_is_big_endian(spec));
  EXPECT_EQ(true, pcm_spec_is_signed(spec));
  EXPECT_EQ(22050, pcm_spec_get_samples_per_sec(spec));
  EXPECT_EQ(16, pcm_spec_get_bits_per_sample(spec));
  EXPECT_EQ(3 * 1000, timespec_get_miliseconds(pcm_spec_get_time(spec)));

  // release it all
  pcm_spec_release(&spec);
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(pcm__event_wav, copy_wav) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = "test.wav";
  source_conf.buffer_size = 1;  // force resize
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // wave decoder
  struct event_pcm *wav_in = NULL;
  EXPECT_EQ(0, event_wav_decode(source, &wav_in));

  // wave encoder
  struct event_pipe *wav_out = NULL;
  EXPECT_EQ(0, event_wav_encode(wav_in, &wav_out));

  // output
  struct event_sink *output = NULL;
  char *output_path = TestFile::GetTempFilePath("output");
  EXPECT_EQ(0, event_sink_into_file(wav_out, output_path, &output));

  // run & check
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));

  struct event_pipe *wav_in_pipe = event_pcm_get_pipe(wav_in);
  EXPECT_EQ(true, event_pipe_is_empty(wav_in_pipe));
  EXPECT_EQ(true, event_pipe_is_end(wav_in_pipe));

  EXPECT_EQ(true, event_pipe_is_empty(wav_out));
  EXPECT_EQ(true, event_pipe_is_end(wav_out));

  EXPECT_EQ(true, event_sink_is_end(output));
  EXPECT_EQ(true, TestFile::CompareFiles(source_conf.file_path, output_path));

  // release it all
  free(output_path);
  event_pipe_release(&source);
  event_loop_release(&loop);
}
