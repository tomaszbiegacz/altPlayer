#include "SharedTestFixture.h"
#include <iostream>
#include <fstream>

extern "C" {
  #include "log.h"
  #include "io.h"
}

char
getCharacterAt(size_t pos) {
  return 1 + pos % 203;
}

void
prepareTestFile(const char *filePath, size_t fileSize) {
  std::ofstream testFile;
  testFile.open(filePath);
  for(size_t i=0; i<fileSize; ++i) {
    testFile << getCharacterAt(i);
  }
  testFile.close();
}

TEST_F(SharedTestFixture, io_buffer_TEST_basic) {
  EMPTY_STRUCT(io_buffer, buffer);
  int16_t *val_i;
  char *val_c;

  EXPECT_EQ(0, io_buffer_alloc(14, &buffer));
  EXPECT_EQ(14, io_buffer_get_allocated_size(&buffer));
  EXPECT_EQ(0, io_buffer_get_unread_size(&buffer));
  EXPECT_TRUE(io_buffer_is_empty(&buffer));
  EXPECT_FALSE(io_buffer_is_full(&buffer));
  EXPECT_FALSE(io_buffer_try_read(&buffer, sizeof(int16_t), (void**)&val_i));
  EXPECT_EQ(0, io_buffer_read_array(&buffer, sizeof(int16_t), (void**)&val_i, 1));

  const int16_t src_1[] = { 30, 31, 32, 33, 34 };
  EXPECT_TRUE(io_buffer_try_write(&buffer, sizeof(src_1), src_1));
  EXPECT_EQ(10, io_buffer_get_unread_size(&buffer));
  EXPECT_FALSE(io_buffer_is_empty(&buffer));
  EXPECT_FALSE(io_buffer_is_full(&buffer));

  EXPECT_TRUE(io_buffer_try_read(&buffer, sizeof(int16_t), (void**)&val_i));
  EXPECT_EQ(30, *val_i);
  EXPECT_EQ(8, io_buffer_get_unread_size(&buffer));

  const char *src_2 = "0";
  EXPECT_TRUE(io_buffer_try_write(&buffer, 1, src_2));
  EXPECT_EQ(9, io_buffer_get_unread_size(&buffer));
  EXPECT_EQ(2, io_buffer_read_array(&buffer, sizeof(int16_t), (void**)&val_i, 2));
  EXPECT_EQ(31, *val_i);
  EXPECT_EQ(32, *(val_i+1));

  size_t available_count;
  io_buffer_array_items(&buffer, sizeof(int16_t), (void**)&val_i, &available_count);
  EXPECT_EQ(2, available_count);
  EXPECT_EQ(33, *val_i);
  EXPECT_EQ(34, *(val_i+1));
  io_buffer_array_seek(&buffer, sizeof(int16_t), available_count);
  EXPECT_EQ(1, io_buffer_get_unread_size(&buffer));

  const char *src_3 = "123456789abcd";
  EXPECT_TRUE(io_buffer_try_write(&buffer, 13, src_3));
  EXPECT_TRUE(io_buffer_is_full(&buffer));
  EXPECT_EQ(14, io_buffer_read_array(&buffer, 1, (void**)&val_c, 100));
  EXPECT_EQ('0', *val_c);
  EXPECT_EQ('d', *(val_c + 13));
  EXPECT_TRUE(io_buffer_is_empty(&buffer));

  io_buffer_free(&buffer);
  EXPECT_EQ(NULL, buffer.data);
  io_buffer_free(&buffer);
}

TEST_F(SharedTestFixture, io_rf_stream_TEST_basic) {
  const char *filePath = "io_rf_stream_TEST_basic.txt";
  prepareTestFile(filePath, 14);
  char *val_c;

  EMPTY_STRUCT(io_rf_stream, buffer);
  EXPECT_EQ(0, io_rf_stream_open_file(filePath, 11, 5, &buffer));
  EXPECT_FALSE(io_rf_stream_is_eof(&buffer));
  EXPECT_FALSE(io_rf_stream_is_empty(&buffer));
  EXPECT_EQ(11, io_rf_stream_get_allocated_buffer_size(&buffer));
  EXPECT_EQ(0, io_rf_stream_get_unread_buffer_size(&buffer));

  EXPECT_EQ(0, io_rf_stream_read_with_poll(&buffer, 0));
  EXPECT_FALSE(io_rf_stream_is_eof(&buffer));
  EXPECT_EQ(5, io_rf_stream_get_unread_buffer_size(&buffer));

  EXPECT_EQ(0, io_rf_stream_read(&buffer, 9, (void**)&val_c));
  EXPECT_EQ(getCharacterAt(0), *val_c);
  EXPECT_EQ(getCharacterAt(8), *(val_c + 8));
  EXPECT_EQ(1, io_rf_stream_get_unread_buffer_size(&buffer));
  EXPECT_EQ(1, io_rf_stream_read_array(&buffer, 1, (void**)&val_c, 1));
  EXPECT_EQ(getCharacterAt(9), *val_c);
  EXPECT_EQ(0, io_rf_stream_get_unread_buffer_size(&buffer));

  EXPECT_EQ(0, io_rf_stream_read(&buffer, 4, (void**)&val_c));
  EXPECT_EQ(getCharacterAt(10), *val_c);
  EXPECT_EQ(getCharacterAt(13), *(val_c + 3));

  EXPECT_EQ(0, io_rf_stream_get_unread_buffer_size(&buffer));
  EXPECT_FALSE(io_rf_stream_is_eof(&buffer));
  EXPECT_FALSE(io_rf_stream_is_empty(&buffer));
  EXPECT_EQ(0, io_rf_stream_read_with_poll(&buffer, 0));
  EXPECT_TRUE(io_rf_stream_is_eof(&buffer));
  EXPECT_TRUE(io_rf_stream_is_empty(&buffer));

  io_rf_stream_free(&buffer);
}
