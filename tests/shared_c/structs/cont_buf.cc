#include "../SharedTestFixture.h"

extern "C" {
  #include "structs/cont_buf.h"
}

TEST(structs__cont_buf, array) {
  struct cont_buf *write = NULL;

  EXPECT_EQ(4, sizeof(int));
  EXPECT_EQ(0, cont_buf_create(4*6+1, &write));

  EXPECT_EQ(25, cont_buf_get_allocated_size(write));
  EXPECT_EQ(25, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));

  struct cont_buf_read *read = cont_buf_read(write);
  EXPECT_EQ(0, cont_buf_get_unread_size(read));
  EXPECT_EQ(true, cont_buf_is_empty(write));

  int *data = NULL;
  size_t count = 0;
  cont_buf_write_array_begin(write, sizeof(int), (void**)&data, &count);
  EXPECT_EQ(6, count);
  data[0] = 1;
  data[1] = 2;
  data[2] = 3;
  cont_buf_write_array_commit(write, sizeof(int), 3);

  EXPECT_EQ(25, cont_buf_get_allocated_size(write));
  EXPECT_EQ(25-4*3, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));
  EXPECT_EQ(4*3, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  data = NULL;
  count = cont_buf_read_array_begin(read, sizeof(int), (const void**)&data, 10);
  EXPECT_EQ(3, count);
  EXPECT_EQ(1, data[0]);
  EXPECT_EQ(2, data[1]);
  EXPECT_EQ(3, data[2]);
  cont_buf_read_array_commit(read, sizeof(int), 2);

  EXPECT_EQ(25, cont_buf_get_allocated_size(write));
  EXPECT_EQ(13, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));
  EXPECT_EQ(4, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  data = NULL;
  count = cont_buf_read_array_begin(read, sizeof(int), (const void**)&data, 1);
  EXPECT_EQ(1, count);
  EXPECT_EQ(3, data[0]);
  cont_buf_read_array_commit(read, sizeof(int), 1);

  EXPECT_EQ(25, cont_buf_get_allocated_size(write));
  EXPECT_EQ(25, cont_buf_get_available_size(write));
  EXPECT_EQ(0, cont_buf_get_unread_size(read));
  EXPECT_EQ(true, cont_buf_read_is_empty(read));

  cont_buf_release(&write);
  EXPECT_EQ(NULL, write);
  cont_buf_release(&write);
}

TEST(structs__cont_buf, struct) {
  struct cont_buf *write = NULL;

  EXPECT_EQ(4, sizeof(int));
  EXPECT_EQ(0, cont_buf_create(4+1, &write));

  EXPECT_EQ(5, cont_buf_get_allocated_size(write));
  EXPECT_EQ(5, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));

  struct cont_buf_read *read = cont_buf_read(write);
  EXPECT_EQ(0, cont_buf_get_unread_size(read));
  EXPECT_EQ(true, cont_buf_is_empty(write));

  const int data[] = { 1, 2 };
  EXPECT_EQ(4, cont_buf_write(write, (void*)data, sizeof(int)));

  EXPECT_EQ(5, cont_buf_get_allocated_size(write));
  EXPECT_EQ(5-4, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));
  EXPECT_EQ(4, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  int *r = NULL;
  EXPECT_EQ(false, cont_buf_read_try(read, sizeof(size_t), (const void**)&r));
  EXPECT_EQ(4, cont_buf_get_unread_size(read));

  EXPECT_EQ(true, cont_buf_read_try(read, sizeof(int), (const void**)&r));
  EXPECT_EQ(1, r[0]);
  EXPECT_EQ(5, cont_buf_get_allocated_size(write));
  EXPECT_EQ(5, cont_buf_get_available_size(write));
  EXPECT_EQ(0, cont_buf_get_unread_size(read));
  EXPECT_EQ(true, cont_buf_read_is_empty(read));

  const char *d = "123456";
  EXPECT_EQ(5, cont_buf_write(write, (void*)d, strlen(d)));
  EXPECT_EQ(true, cont_buf_is_full(write));

  char *rd = NULL;
  EXPECT_EQ(true, cont_buf_read_try(read, sizeof(char), (const void**)&rd));
  EXPECT_EQ('1', *rd);
  EXPECT_EQ(true, cont_buf_read_try(read, sizeof(char), (const void**)&rd));
  EXPECT_EQ('2', *rd);
  EXPECT_EQ(0, cont_buf_get_available_size(write));
  EXPECT_EQ(3, cont_buf_get_unread_size(read));

  cont_buf_no_padding(write);
  EXPECT_EQ(2, cont_buf_get_available_size(write));
  EXPECT_EQ(3, cont_buf_get_unread_size(read));

  EXPECT_EQ(true, cont_buf_read_try(read, sizeof(char), (const void**)&rd));
  EXPECT_EQ('3', *rd);
  EXPECT_EQ(2, cont_buf_get_unread_size(read));

  cont_buf_release(&write);
  EXPECT_EQ(NULL, write);
  cont_buf_release(&write);
}

TEST(structs__cont_buf, extends) {
  struct cont_buf *write = NULL;

  EXPECT_EQ(4, sizeof(int));
  EXPECT_EQ(0, cont_buf_create(3*4, &write));

  EXPECT_EQ(12, cont_buf_get_allocated_size(write));
  EXPECT_EQ(12, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));

  struct cont_buf_read *read = cont_buf_read(write);
  EXPECT_EQ(0, cont_buf_get_unread_size(read));
  EXPECT_EQ(true, cont_buf_is_empty(write));

  int *data = NULL;
  size_t count = 0;
  cont_buf_write_array_begin(write, sizeof(int), (void**)&data, &count);
  EXPECT_EQ(3, count);
  data[0] = 1;
  data[1] = 2;
  data[2] = 3;
  cont_buf_write_array_commit(write, sizeof(int), 3);

  EXPECT_EQ(3*4, cont_buf_get_allocated_size(write));
  EXPECT_EQ(0, cont_buf_get_available_size(write));
  EXPECT_EQ(true, cont_buf_is_full(write));
  EXPECT_EQ(4*3, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  data = NULL;
  count = cont_buf_read_array_begin(read, sizeof(int), (const void**)&data, 10);
  EXPECT_EQ(3, count);
  EXPECT_EQ(1, data[0]);
  cont_buf_read_array_commit(read, sizeof(int), 1);

  EXPECT_EQ(12, cont_buf_get_allocated_size(write));
  EXPECT_EQ(0, cont_buf_get_available_size(write));
  EXPECT_EQ(true, cont_buf_is_full(write));
  EXPECT_EQ(8, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  EXPECT_EQ(0, cont_buf_resize(write, 13));

  EXPECT_EQ(13, cont_buf_get_allocated_size(write));
  EXPECT_EQ(13 - 8, cont_buf_get_available_size(write));
  EXPECT_EQ(false, cont_buf_is_full(write));
  EXPECT_EQ(8, cont_buf_get_unread_size(read));
  EXPECT_EQ(false, cont_buf_read_is_empty(read));

  count = cont_buf_read_array_begin(read, sizeof(int), (const void**)&data, 10);
  EXPECT_EQ(2, count);
  EXPECT_EQ(2, data[0]);
  EXPECT_EQ(3, data[1]);

  cont_buf_release(&write);
  EXPECT_EQ(NULL, write);
  cont_buf_release(&write);
}
