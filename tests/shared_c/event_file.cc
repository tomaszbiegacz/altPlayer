#include "TestFile.h"
#include "TestMemorySink.h"

extern "C" {
  #include "event_file.h"
  #include "log.h"
  #include "mem.h"
}

TEST(event_file, read_single) {
  log_set_verbose(true);
  EXPECT_EQ(0, event_loop_global_init());

  struct event_base *loop = NULL;
  EXPECT_EQ(0, event_loop_create(&loop));

  // source

  TestFile testFile = TestFile("event_file", "123456");

  struct event_pipe *source = NULL;
  EMPTY_STRUCT(event_pipe_file_config, source_conf);
  source_conf.file_path = testFile.GetPath();
  EXPECT_EQ(0, event_pipe_from_file(loop, &source_conf, &source));

  // run & check

  TestMemorySink sink = TestMemorySink(source, 12);
  EXPECT_EQ(0, event_loop_run(loop));

  char* data;
  size_t count = cont_buf_read_array_begin(
    sink.GetBuffer(),
    1, (const void**)&data, 7);
  EXPECT_EQ(6, count);
  EXPECT_EQ('1', data[0]);
  EXPECT_EQ('2', data[1]);
  EXPECT_EQ('3', data[2]);
  EXPECT_EQ('4', data[3]);
  EXPECT_EQ('5', data[4]);
  EXPECT_EQ('6', data[5]);
}
