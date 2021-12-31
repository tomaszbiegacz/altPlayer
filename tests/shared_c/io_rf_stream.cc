#include <iostream>
#include <fstream>
#include "SharedTestFixture.h"

extern "C" {
  #include "io_rf_stream.h"
}

static char
getCharacterAt(size_t pos) {
  return 1 + pos % 203;
}

static void
prepareTestFile(const char *filePath, size_t fileSize) {
  std::ofstream testFile;
  testFile.open(filePath);
  for(size_t i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();
}

TEST(io_rf_stream, basic) {
  const char *filePath = "io_rf_stream_TEST_basic.txt";
  prepareTestFile(filePath, 14);
  char *val_c;

  struct io_rf_stream *st = NULL;
  EXPECT_EQ(0, io_rf_stream_open_file(filePath, 11, 5, &st));

  const struct io_buffer* buff = io_rf_stream_get_buffer(st);
  EXPECT_FALSE(io_rf_stream_is_eof(st));
  EXPECT_FALSE(io_rf_stream_is_empty(st));
  EXPECT_EQ(11, io_buffer_get_allocated_size(buff));
  EXPECT_EQ(0, io_buffer_get_unread_size(buff));

  EXPECT_EQ(0, io_rf_stream_read_with_poll(st, 0));
  EXPECT_FALSE(io_rf_stream_is_eof(st));
  EXPECT_EQ(5, io_buffer_get_unread_size(buff));

  EXPECT_EQ(0, io_rf_stream_read(st, 9, (void**)&val_c));
  EXPECT_EQ(getCharacterAt(0), *val_c);
  EXPECT_EQ(getCharacterAt(8), *(val_c + 8));
  EXPECT_EQ(1, io_buffer_get_unread_size(buff));
  EXPECT_EQ(1, io_rf_stream_read_array(st, 1, (void**)&val_c, 1));
  EXPECT_EQ(getCharacterAt(9), *val_c);
  EXPECT_EQ(0, io_buffer_get_unread_size(buff));

  EXPECT_EQ(0, io_rf_stream_read(st, 4, (void**)&val_c));
  EXPECT_EQ(getCharacterAt(10), *val_c);
  EXPECT_EQ(getCharacterAt(13), *(val_c + 3));

  EXPECT_EQ(0, io_buffer_get_unread_size(buff));
  EXPECT_FALSE(io_rf_stream_is_eof(st));
  EXPECT_FALSE(io_rf_stream_is_empty(st));
  EXPECT_EQ(0, io_rf_stream_read_with_poll(st, 0));
  EXPECT_TRUE(io_rf_stream_is_eof(st));
  EXPECT_TRUE(io_rf_stream_is_empty(st));

  io_rf_stream_release(&st);
}
