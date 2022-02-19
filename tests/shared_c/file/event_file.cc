#include "../TestFile.h"
#include "../event/TestMemorySink.h"
#include "../event/TestMemoryPipeCopy.h"
#include "../event/TestHeaderPipe.h"

extern "C" {
  #include "file/event_file.h"
  #include "log.h"
  #include "mem.h"
}

/**
 * Available tests and their scope:
 * read_direct_single:
 *  0 intermediate
 *  1 read
 * read_direct_twice:
 *  0 intermediate
 *  2 reads
 * read_intermediate_single:
 *  1 intermediate buffer
 *  1 read
 * read_intermediate_many_noPadding:
 *  1 intermediate buffer
 *  4 reads: source and interdiate buffers have different sizes
 *  no source buffer padding strategy due to source buffer size (3)
 * read_intermediate_many_padding:
 *  1 intermediate buffer
 *  2 reads: source and interdiate buffers have different sizes
 *  source buffer padding strategy
 * read_intermediate_header:
 *  1 intermediate checking stream header
 *  1 read
 * read_intermediate_header_many:
 *  1 intermediate header
 *  1 intermediate buffer
 *  6 reads: source and interdiate buffers have different sizes
 * read_write
 *  0 intermediate
 *  1 read
 *  1 write
 */

TEST(file__event_file, read_direct_single) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // run & check

  TestMemorySink sink = TestMemorySink(source, 12);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 1, event_pipe_get_read_count(source));  // with EOF

  EXPECT_TRUE(sink.IsDataEqual(data));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_direct_twice) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  source_conf.buffer_size = 3;
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // run & check

  TestMemorySink sink = TestMemorySink(source, 12);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 2, event_pipe_get_read_count(source));  // with EOF

  EXPECT_TRUE(sink.IsDataEqual(data));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_intermediate_single) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // interdiamete buffer
  TestMemoryPipeCopy copy = TestMemoryPipeCopy(source, 6);

  // run & check

  TestMemorySink sink = TestMemorySink(copy.GetPipe(), 12);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 1, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_pipe_is_empty(copy.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(copy.GetPipe()));
  EXPECT_EQ(1, event_pipe_get_read_count(copy.GetPipe()));

  EXPECT_TRUE(sink.IsDataEqual(data));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_intermediate_many_noPadding) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  source_conf.buffer_size = 3;
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // interdiamete buffer
  TestMemoryPipeCopy copy = TestMemoryPipeCopy(source, 2);

  // run & check

  TestMemorySink sink = TestMemorySink(copy.GetPipe(), 12);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 2, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_pipe_is_empty(copy.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(copy.GetPipe()));
  EXPECT_EQ(4, event_pipe_get_read_count(copy.GetPipe()));

  EXPECT_TRUE(sink.IsDataEqual(data));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_intermediate_many_padding) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  source_conf.buffer_size = 6;
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // interdiamete buffer
  TestMemoryPipeCopy copy = TestMemoryPipeCopy(source, 4);
  event_pipe_set_read_lowmark(copy.GetPipe(), 4);

  // run & check

  TestMemorySink sink = TestMemorySink(copy.GetPipe(), 20);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 3, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_pipe_is_empty(copy.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(copy.GetPipe()));
  EXPECT_EQ(3, event_pipe_get_read_count(copy.GetPipe()));

  EXPECT_TRUE(sink.IsDataEqual(data));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}


TEST(file__event_file, read_intermediate_header) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  source_conf.buffer_size = 6;
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // interdiamete header
  TestHeaderPipe header = TestHeaderPipe(source, "12345");

  // run & check

  TestMemorySink sink = TestMemorySink(header.GetPipe(), 20);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 2, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_pipe_is_empty(header.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(header.GetPipe()));
  EXPECT_EQ(1, event_pipe_get_read_count(header.GetPipe()));

  EXPECT_TRUE(sink.IsDataEqual("6123456"));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_intermediate_header_many) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  source_conf.buffer_size = 6;
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // interdiamete header
  TestHeaderPipe header = TestHeaderPipe(source, "12345");

  // interdiamete buffer
  TestMemoryPipeCopy copy = TestMemoryPipeCopy(header.GetPipe(), 5);
  event_pipe_set_read_lowmark(copy.GetPipe(), 4);

  // run & check

  TestMemorySink sink = TestMemorySink(copy.GetPipe(), 20);
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 3, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_pipe_is_empty(header.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(header.GetPipe()));
  EXPECT_EQ(1, event_pipe_get_read_count(header.GetPipe()));

  EXPECT_EQ(true, event_pipe_is_empty(copy.GetPipe()));
  EXPECT_EQ(true, event_pipe_is_end(copy.GetPipe()));
  EXPECT_EQ(2, event_pipe_get_read_count(copy.GetPipe()));

  EXPECT_TRUE(sink.IsDataEqual("6123456"));
  EXPECT_EQ(true, event_sink_is_end(sink.GetSink()));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
}

TEST(file__event_file, read_write) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());
  const char* data = "123456";

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source
  struct event_pipe *source = NULL;
  TestFile testFile = TestFile("event_file_src", data);
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // output
  struct event_sink *output = NULL;
  char *output_path = TestFile::GetTempFilePath("event_file_dst");
  EXPECT_EQ(0, event_sink_into_file(source, output_path, &output));

  // run & check
  EXPECT_EQ(0, event_loop_run(loop));

  EXPECT_EQ(true, event_pipe_is_empty(source));
  EXPECT_EQ(true, event_pipe_is_end(source));
  EXPECT_EQ(1 + 1, event_pipe_get_read_count(source));  // with EOF

  EXPECT_EQ(true, event_sink_is_end(output));
  ASSERT_EQ(testFile.GetContent(), TestFile::ReadContent(output_path));

  // release it all
  event_pipe_release(&source);
  event_loop_release(&loop);
  free(output_path);
}
