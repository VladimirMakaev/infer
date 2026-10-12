/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * This source code is licensed under the MIT license in the repository root.
 */
#include <cstring>
#include <cstdlib>

namespace uninit_tracking {
void initialize_unknown(int*);
void initialize_wrapper_unknown(void*);
static void initialize_one(int* value) { *value = 7; }
static void initialize_indexed(int* value) { value[0] = 7; }

struct Pair { int first; int second; };
struct Wrapper { Pair member; };

int scalar_bad() { int value; return value; }
int scalar_ok() { int value = 7; return value; }
int array_no_write_bad() { int values[4]; return values[0]; }
int array_partial_bad() { int values[4]; values[0] = 7; return values[2]; }
int array_nonzero_first_bad() { int values[4]; values[2] = 7; return values[0]; }
int array_other_partial_bad() { int values[4]; values[1] = 7; return values[3]; }
int array_selected_bad(unsigned index) {
  int values[4]; values[0] = 7;
  return index == 2 ? values[index] : 0;
}
int array_write_read_ok() { int values[4]; values[0] = 7; return values[0]; }
int array_full_ok() { int values[4] = {1, 2, 3, 4}; return values[2]; }
int array_zero_ok() { int values[4]{}; return values[2]; }
int array_loop_init_ok() {
  int values[4]; for (int i = 0; i < 4; ++i) values[i] = i; return values[2];
}
int array_same_dynamic_ok(unsigned index) {
  int values[4]; if (index >= 4) return 0; values[index] = 7; return values[index];
}
int array_initialized_rewind_ok(bool rewind) {
  int values[4]{}; int used = 0; int result = 0;
  for (int step = 0; step < 4; ++step) {
    values[used++] = step;
    if (rewind) result += values[--used];
  }
  return result;
}
int array_branch_partial_bad(bool first) {
  int values[4]; values[0] = 7;
  if (first) values[1] = 8; else values[2] = 9;
  values[3] = 10;
  if (first) return values[2];
  return values[1];
}
int array_address_only_ok() { int values[4]; initialize_unknown(&values[0]); return 0; }
int array_unknown_init_ok() { int values[4]; initialize_unknown(&values[0]); return values[0]; }
int array_defined_init_ok() { int values[4]; initialize_one(&values[0]); return values[0]; }
int array_indexed_init_ok() { int values[4]; initialize_indexed(&values[0]); return values[0]; }
int scalar_indexed_init_ok() { int value; initialize_indexed(&value); return value; }
int array_memset_ok() { int values[4]; std::memset(values, 0, sizeof(values)); return values[2]; }
int array_saved_memset_ok() {
  int values[4]; int* saved = &values[2]; std::memset(values, 0, sizeof(values)); return *saved;
}
int array_pointer_loop_ok() {
  int values[4]; int* cursor = values;
  for (int i = 0; i < 4; ++i) *cursor++ = i;
  return values[2];
}
int array_offset_init_ok() { int values[4]; initialize_one(values + 1); return values[1]; }
char array_byte_view_ok() {
  int values[4]; values[0] = 7; return reinterpret_cast<char*>(values)[1];
}
int wrapper_bad() { Wrapper value; value.member.first = 7; return value.member.second; }
int wrapper_no_write_bad() { Wrapper value; return value.member.second; }
int wrapper_full_ok() {
  Wrapper value; value.member.first = 7; value.member.second = 8; return value.member.second;
}
int wrapper_value_ok() { Wrapper value{}; return value.member.second; }
int wrapper_aggregate_ok() { Wrapper value{{7, 8}}; return value.member.second; }
int wrapper_unknown_ok() { Wrapper value; initialize_wrapper_unknown(&value); return value.member.second; }
int wrapper_saved_memset_ok() {
  Wrapper value; int* saved = &value.member.second;
  std::memset(&value, 0, sizeof(value)); return *saved;
}
int heap_saved_symbolic_bad(unsigned index) {
  int* values = static_cast<int*>(std::malloc(2 * sizeof(int)));
  if (!values) return 0;
  int* saved = &values[0]; if (index < 2) values[index] = 7;
  int result = *saved; std::free(values); return result;
}
static int write_byte_read_word(int* values) {
  reinterpret_cast<char*>(values)[0] = 7; return *values;
}
int heap_partial_byte_bad() {
  int* values = static_cast<int*>(std::malloc(sizeof(int)));
  if (!values) return 0;
  int result = write_byte_read_word(values); std::free(values); return result;
}
// Storage escapes and reinterpreted views retain the previous approximation.
int array_escaped_partial_bad_FN() {
  int values[4]; initialize_one(values); return values[2];
}
} // namespace uninit_tracking
