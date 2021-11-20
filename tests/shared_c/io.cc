#include "SharedTestFixture.h"
#include <iostream>
#include <fstream>

extern "C" {
  #include "log.h"
  #include "io.h"
}

char getCharacterAt(size_t pos) {
  return '0' + pos % 7;
}

TEST_F(SharedTestFixture, TEST_io_fd_cursor_read_full_stats_once) {
  const int fileSize = 1024*1024;
  const char* filePath = "TEST_io_fd_cursor_read_full_stats.txt";

  std::ofstream testFile;
  testFile.open(filePath);
  for(int i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();

  EMPTY_STRUCT(io_fd_cursor, cursor);
  EMPTY_STRUCT(io_fd_cursor_statistics, stats);

  EXPECT_EQ(0, io_fd_cursor_file(filePath, fileSize + 1, &cursor));
  EXPECT_STREQ(filePath, cursor.name);
  EXPECT_EQ(fileSize + 1, cursor.buffer.size);
  EXPECT_EQ(0, cursor.used_size);
  EXPECT_FALSE(io_fd_cursor_is_eof(&cursor));

  EXPECT_EQ(0, io_fd_cursor_read_full_stats(&cursor, &stats));
  EXPECT_TRUE(io_fd_cursor_is_eof(&cursor));
  EXPECT_EQ(fileSize, cursor.used_size);
  log_info(
    "reading: %d (waiting %d.%03ds, reading %d.%03ds)",
    stats.loops_count,
    stats.waiting_time.tv_sec,
    stats.waiting_time.tv_nsec / 1000000,
    stats.reading_time.tv_sec,
    stats.reading_time.tv_nsec / 1000000);

  for(int i=0; i<fileSize; ++i) {
    const char* data = (char*)cursor.buffer.data;
    EXPECT_EQ(getCharacterAt(i), data[i]);
  }

  io_fd_cursor_free(&cursor);
  EXPECT_EQ(NULL, cursor.name);
  EXPECT_TRUE(io_memory_block_is_empty(&cursor.buffer));
  EXPECT_EQ(0, cursor.used_size);
}

TEST_F(SharedTestFixture, TEST_io_fd_cursor_read_full_stats_partial) {
  const size_t fileSize = 1024;
  const char* filePath = "TEST_io_fd_cursor_read_full_stats.txt";
  const size_t readSize = 777;

  std::ofstream testFile;
  testFile.open(filePath);
  for(size_t i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();

  EMPTY_STRUCT(io_fd_cursor, cursor);
  EMPTY_STRUCT(io_fd_cursor_statistics, stats);

  EXPECT_EQ(0, io_fd_cursor_file(filePath, readSize, &cursor));
  EXPECT_EQ(0, cursor.used_size);
  EXPECT_FALSE(io_fd_cursor_is_eof(&cursor));

  EXPECT_EQ(0, io_fd_cursor_read_full_stats(&cursor, &stats));
  EXPECT_EQ(readSize, cursor.used_size);
  EXPECT_FALSE(io_fd_cursor_is_eof(&cursor));
  for(size_t i=0; i<cursor.used_size; ++i) {
    const char* data = (char*)cursor.buffer.data;
    EXPECT_EQ(getCharacterAt(i), data[i]);
  }

  EXPECT_EQ(0, io_fd_cursor_read_full_stats(&cursor, &stats));
  EXPECT_EQ(fileSize - readSize, cursor.used_size);
  EXPECT_TRUE(io_fd_cursor_is_eof(&cursor));
  for(size_t i=0; i<cursor.used_size; ++i) {
    const char* data = (char*)cursor.buffer.data;
    EXPECT_EQ(getCharacterAt(readSize + i), data[i]);
  }

  log_info(
    "reading: %d (waiting %d.%03ds, reading %d.%03ds)",
    stats.loops_count,
    stats.waiting_time.tv_sec,
    stats.waiting_time.tv_nsec / 1000000,
    stats.reading_time.tv_sec,
    stats.reading_time.tv_nsec / 1000000);

  io_fd_cursor_free(&cursor);
}
