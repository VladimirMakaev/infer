/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <string.h>

struct S1 {
  int f[2];
};

void array_field_access_Good(struct S1 x, struct S1 y) {
  int a[10];
  x.f[0] = 1;
  y.f[0] = 20;
  a[x.f[0]] = 0;
}

void array_field_access_Bad(struct S1 x, struct S1 y) {
  int a[10];
  y.f[0] = 20;
  x.f[0] = 1;
  a[y.f[0]] = 0;
}

void decreasing_pointer_Good(struct S1* x) {
  int* p = &(x->f[1]);
  p--;
  *p = 0;
}

void decreasing_pointer_Bad(struct S1* x) {
  int* p = &(x->f[1]);
  p--;
  p--;
  *p = 0;
}

void decay_array_field_Good(struct S1* x) {
  int* p = x->f;
  p[1] = 0;
}

// An array field decays to its array only when it is passed to a function.
void FN_decay_array_field_Bad(struct S1* x) {
  int* p = x->f;
  p[2] = 0;
}

struct S2 {
  int arr[4];
  char name[8];
};

void strcpy_array_field_Good(struct S2* x) { strcpy(x->name, "1234567"); }

void strcpy_array_field_Bad(struct S2* x) { strcpy(x->name, "12345678"); }

void local_array_field_Good() {
  struct S2 s;
  s.arr[3] = 0;
  s.name[7] = 0;
}

void local_array_field_Bad() {
  struct S2 s;
  s.arr[4] = 0;
}

void local_array_field_loop_Bad() {
  struct S2 s;
  for (int i = 0; i <= 4; i++) {
    s.arr[i] = 0;
  }
}

void local_array_field_memcpy_Good(const char* src) {
  struct S2 s;
  memcpy(s.name, src, sizeof(s.name));
}

void local_array_field_memcpy_Bad(const char* src, int big) {
  struct S2 s;
  size_t n = big ? 16 : 4;
  memcpy(s.name, src, n);
}

void local_array_field_memset_Bad() {
  struct S2 s;
  memset(s.name, 0, sizeof(s));
}

void local_array_field_by_pointer_Bad() {
  struct S2 s;
  struct S2* p = &s;
  p->arr[4] = 0;
}

void local_array_field_string_init_Good() {
  struct S2 s = {{0}, "abc"};
  s.name[7] = 0;
  strcpy(s.name, "1234567");
}

void local_array_field_string_init_Bad() {
  struct S2 s = {{0}, "abc"};
  s.name[8] = 0;
}

struct S3 {
  const char* p;
};

char pointer_field_string_Bad() {
  struct S3 s;
  s.p = "abc";
  return s.p[4];
}

void local_array_field_copy_Bad() {
  struct S2 x, y;
  x.arr[0] = 0;
  y = x;
  y.arr[4] = 0;
}

struct S4 {
  int x;
  struct S2 inner;
};

void nested_local_array_field_Good() {
  struct S4 o;
  o.inner.name[7] = 0;
}

void nested_local_array_field_Bad() {
  struct S4 o;
  o.inner.name[8] = 0;
}

void local_array_of_structs_Good() {
  struct S2 a[3];
  a[2].arr[3] = 0;
}

void local_array_of_structs_Bad() {
  struct S2 a[3];
  a[2].arr[4] = 0;
}

struct Flexible {
  int n;
  char data[1];
};

union FlexibleBuffer {
  struct Flexible flexible;
  char raw[64];
};

void local_flexible_array_field_Good() {
  union FlexibleBuffer u;
  u.flexible.data[10] = 0;
}

struct Trailing {
  int n;
  char data[2];
};

void local_trailing_array_field_Bad() {
  struct Trailing t;
  t.data[2] = 0;
}

static void write_index(int* p, int i) { p[i] = 0; }

void pass_local_array_field_Good() {
  struct S2 s;
  write_index(s.arr, 3);
}

void pass_local_array_field_Bad() {
  struct S2 s;
  write_index(s.arr, 4);
}

void access_local_array_field_after_call_Bad() {
  struct S2 s;
  write_index(s.arr, 0);
  s.arr[4] = 0;
}

struct Buffer {
  size_t len;
  char buf[16];
};

static int append(struct Buffer* b, const char* s, size_t n) {
  if (b->len + n > sizeof(b->buf)) {
    return -1;
  }
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  return 0;
}

void append_local_array_field_Good() {
  struct Buffer b = {0};
  append(&b, "0123456789", 10);
  append(&b, "0123456789", 10);
}

void strcpy_local_array_field_Good() {
  struct S2 s;
  strcpy(s.name, "1234567");
}

void strcpy_local_array_field_Bad() {
  struct S2 s;
  strcpy(s.name, "12345678");
}

void memcpy_then_access_local_array_field_Bad(const char* src) {
  struct S2 s;
  memcpy(s.name, src, 4);
  s.name[8] = 0;
}

void strcpy_then_access_local_array_field_Bad() {
  struct S2 s;
  strcpy(s.name, "1");
  s.name[8] = 0;
}

void strlen_local_array_field_Good() {
  struct S2 s = {{0}, "abc"};
  int a[4];
  a[strlen(s.name)] = 0;
}

void strlen_local_array_field_Bad() {
  struct S2 s = {{0}, "abcd"};
  int a[4];
  a[strlen(s.name)] = 0;
}

void strcpy_from_local_array_field_Good() {
  struct S2 s = {{0}, "abc"};
  char d[4];
  strcpy(d, s.name);
}

void strcpy_from_local_array_field_Bad() {
  struct S2 s = {{0}, "abcd"};
  char d[4];
  strcpy(d, s.name);
}

void strcat_local_array_field_Good() {
  struct S2 s = {{0}, "abc"};
  strcat(s.name, "defg");
}

void strcat_local_array_field_Bad() {
  struct S2 s = {{0}, "abcd"};
  strcat(s.name, "efgh");
}

void strcpy_from_copied_array_field_Bad() {
  struct S2 x = {{0}, "abcd"}, y;
  char d[4];
  y = x;
  strcpy(d, y.name);
}

void memcpy_cast_then_access_local_array_field_Bad(const char* src) {
  struct S2 s;
  memcpy((char*)s.arr, src, sizeof(s.arr));
  s.arr[4] = 0;
}

// As for local arrays, the address of an element passed to a function is
// evaluated to the element instead of a pointer into the array.
void FN_memcpy_array_field_element_Bad(const char* src) {
  struct S2 s;
  memcpy(&s.name[1], src, 8);
}

// An array field does not decay to its array in pointer arithmetic.
void FN_memcpy_array_field_offset_Bad(const char* src) {
  struct S2 s;
  memcpy(s.name + 1, src, 8);
}

struct S5 {
  char names[2][8];
};

void local_2d_array_field_string_init_Good() {
  struct S5 s = {{"ab", "cd"}};
  s.names[1][7] = 0;
}

void local_2d_array_field_string_init_Bad() {
  struct S5 s = {{"ab", "cd"}};
  s.names[1][8] = 0;
}

union TrailingBuffer {
  struct Trailing trailing;
  char raw[64];
};

// Only trailing arrays of length 0 or 1 are treated as flexible, also when the
// struct overlays a larger buffer in a union.
void FP_local_union_trailing_array_field_Good() {
  union TrailingBuffer u;
  u.trailing.data[10] = 0;
}

/* The length of a string literal is known where it is assigned.  Assigning one
   on more than one path used to lose it: the cell holding the length is missing
   from the state of every path that did not assign that literal, and the join
   replaced the missing side with a symbol standing for an entry value the block
   cannot have, since the procedure allocated it itself. */
void literal_join_strlen_Ok(int cond) {
  const char* s = cond ? "abc" : "xyz";
  char buf[8];
  memcpy(buf, s, strlen(s) + 1);
}

/* The same assignment written as a branch rather than a conditional. */
void literal_branch_strlen_Ok(int cond) {
  const char* s = "abc";
  if (cond) {
    s = "xyz";
  }
  char buf[8];
  memcpy(buf, s, strlen(s) + 1);
}

/* Literals of different lengths still cross-check each length against the other
   one's block, because the pointer keeps both blocks and the length is the union
   of the two.  That is a separate imprecision; the length itself is exact now. */
void FP_literal_join_unequal_lengths_Ok(int cond) {
  const char* s = cond ? "abc" : "de";
  char buf[8];
  memcpy(buf, s, strlen(s) + 1);
}

/* The destination is too small for either literal. */
void literal_join_strlen_Bad(int cond) {
  const char* s = cond ? "abc" : "xyz";
  char buf[2];
  memcpy(buf, s, strlen(s) + 1);
}

/* A read past the end of both literals. */
char literal_join_index_Bad(int cond) {
  const char* s = cond ? "abc" : "xyz";
  return s[6];
}

/* One block, live on both paths and written on only one: its length is genuinely
   unknown on the path that did not write it, so nothing may be concluded here.
   This is the case the missing-cell fallback exists for. */
void one_branch_writes_strlen_Bad(int cond) {
  char buf[8];
  if (cond) {
    strcpy(buf, "abcdef");
  }
  char small[2];
  small[strlen(buf)] = 0;
}

/* The same, written in a loop: an iterate that does not hold the block yet may
   still reach it on a later round, so nothing may be concluded from its absence
   there either.  Both arms of the body write, and the length is still unknown
   on the first iteration, before either has run. */
void loop_both_arms_write_strlen_Bad(int n) {
  char buf[8];
  for (int i = 0; i < n; i++) {
    if (i == 3) {
      strcpy(buf, "abcdef");
    } else {
      buf[0] = 'x';
    }
  }
  char small[2];
  small[strlen(buf)] = 0;
}

/* A loop whose body writes the block on some iterations only. */
void loop_one_arm_writes_strlen_Bad(int n) {
  char buf[8];
  for (int i = 0; i < n; i++) {
    if (i == 3) {
      strcpy(buf, "abcdef");
    }
  }
  char small[2];
  small[strlen(buf)] = 0;
}
