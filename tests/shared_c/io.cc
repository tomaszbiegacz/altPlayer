#include "SharedTestFixture.h"
#include <iostream>
#include <fstream>

extern "C" {
  #include "log.h"
  #include "io.h"
}

char
getCharacterAt(size_t pos) {
  return '0' + pos % 7;
}

TEST_F(SharedTestFixture, io_rf_stream_read_full_TEST_once) {
  const int fileSize = 1024*1024;
  const char* filePath = "TEST_io_rf_stream_read_full.txt";

  std::ofstream testFile;
  testFile.open(filePath);
  for(int i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();

  EMPTY_STRUCT(io_rf_stream, cursor);
  EMPTY_STRUCT(io_stream_statistics, stats);

  EXPECT_EQ(0, io_rf_stream_open_file(filePath, fileSize + 1, &cursor));
  EXPECT_STREQ(filePath, cursor.name);
  EXPECT_EQ(fileSize + 1, cursor.buffer.size);
  EXPECT_EQ(0, cursor.used_size);
  EXPECT_FALSE(io_rf_stream_is_eof(&cursor));

  EXPECT_EQ(0, io_rf_stream_read_full(&cursor, 100, &stats));
  EXPECT_TRUE(io_rf_stream_is_eof(&cursor));
  EXPECT_EQ(fileSize, cursor.used_size);
  for(int i=0; i<fileSize; ++i) {
    const char* data = (char*)cursor.buffer.data;
    EXPECT_EQ(getCharacterAt(i), data[i]);
  }

  log_io_rf_stream_statistics(&stats, "test");
  io_rf_stream_free(&cursor);
  EXPECT_EQ(NULL, cursor.name);
  EXPECT_TRUE(io_memory_block_is_empty(&cursor.buffer));
  EXPECT_EQ(0, cursor.used_size);
}

TEST_F(SharedTestFixture, io_rf_stream_read_full_TEST_timeout) {
  const int fileSize = 1024*1024*4;
  const char* filePath = "TEST_io_rf_stream_read_full.txt";

  std::ofstream testFile;
  testFile.open(filePath);
  for(int i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();

  EMPTY_STRUCT(io_rf_stream, cursor);
  EMPTY_STRUCT(io_stream_statistics, stats);

  EXPECT_EQ(0, io_rf_stream_open_file(filePath, fileSize + 1, &cursor));
  EXPECT_STREQ(filePath, cursor.name);
  EXPECT_EQ(fileSize + 1, cursor.buffer.size);
  EXPECT_EQ(0, cursor.used_size);
  EXPECT_FALSE(io_rf_stream_is_eof(&cursor));

  EXPECT_EQ(0, io_rf_stream_read_full(&cursor, 1, &stats));
  EXPECT_FALSE(io_rf_stream_is_eof(&cursor));

  log_io_rf_stream_statistics(&stats, "test");
  io_rf_stream_free(&cursor);
}

TEST_F(SharedTestFixture, io_rf_stream_read_full_TEST_partial) {
  const size_t fileSize = 1024;
  const char* filePath = "TEST_io_rf_stream_read_full.txt";
  const size_t readSize = 750;

  std::ofstream testFile;
  testFile.open(filePath);
  for(size_t i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();

  EMPTY_STRUCT(io_rf_stream, cursor);
  EMPTY_STRUCT(io_stream_statistics, stats);

  EXPECT_EQ(0, io_rf_stream_open_file(filePath, readSize, &cursor));
  EXPECT_FALSE(io_rf_stream_is_eof(&cursor));
  EXPECT_EQ(0, io_rf_stream_get_unread_buffer_size(&cursor));

  EXPECT_EQ(0, io_rf_stream_read_full(&cursor, 100, &stats));
  EXPECT_FALSE(io_rf_stream_is_eof(&cursor));
  EXPECT_EQ(readSize, io_rf_stream_get_unread_buffer_size(&cursor));
  for(size_t i=0; i<cursor.used_size; ++i) {
    const char* data = (char*)cursor.buffer.data;
    EXPECT_EQ(getCharacterAt(i), data[i]);
  }

  unsigned partialSeek = readSize / 3;
  struct io_memory_block b1 = io_rf_stream_seek(&cursor, partialSeek);
  EXPECT_EQ(partialSeek, b1.size);
  EXPECT_EQ(partialSeek, cursor.cursor_offset);
  EXPECT_EQ(readSize-partialSeek, io_rf_stream_get_unread_buffer_size(&cursor));
  for(size_t i=0; i<partialSeek; ++i) {
    const char* data = (char*)b1.data;
    EXPECT_EQ(getCharacterAt(i), data[i]);
  }
  size_t check_prefix = partialSeek;

  struct io_memory_block b12 = io_rf_stream_seek(&cursor, partialSeek);
  EXPECT_EQ(partialSeek, b12.size);
  EXPECT_EQ(2 * partialSeek, cursor.cursor_offset);
  for(size_t i=0; i<partialSeek; ++i) {
    const char* data = (char*)b12.data;
    EXPECT_EQ(getCharacterAt(check_prefix + i), data[i]);
  }
  check_prefix += partialSeek;

  char* buffer;
  size_t buffer_size = 500;
  EXPECT_EQ(
    0,
    io_rf_stream_seek_block(&cursor, 100, buffer_size, (void**)&buffer, &stats));
  for(size_t i=0; i<buffer_size; ++i) {
    EXPECT_EQ(getCharacterAt(check_prefix + i), buffer[i]);
  }
  check_prefix += buffer_size;

  EXPECT_EQ(0, io_rf_stream_read_full(&cursor, 100, &stats));
  EXPECT_TRUE(io_rf_stream_is_eof(&cursor));
  EXPECT_EQ(
    fileSize - check_prefix, io_rf_stream_get_unread_buffer_size(&cursor));

  struct io_memory_block b2 = io_rf_stream_seek(&cursor, fileSize-check_prefix);
  EXPECT_EQ(0, io_rf_stream_get_unread_buffer_size(&cursor));
  for(size_t i=0; i<b2.size; ++i) {
    const char* data = (char*)b2.data;
    EXPECT_EQ(getCharacterAt(check_prefix + i), data[i]);
  }

  log_io_rf_stream_statistics(&stats, "test");
  io_rf_stream_free(&cursor);
}
