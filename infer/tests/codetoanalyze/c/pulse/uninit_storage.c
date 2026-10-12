/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * This source code is licensed under the MIT license in the repository root.
 */
#include <string.h>
void uninit_c_fill_unknown(int*);
static void uninit_c_fill_one(int* value) { *value = 7; }
int uninit_c_array_bad(void) { int values[4]; return values[2]; }
int uninit_c_partial_bad(void) { int values[4]; values[0] = 7; return values[2]; }
int uninit_c_nonzero_first_bad(void) { int values[4]; values[2] = 7; return values[0]; }
int uninit_c_address_ok(void) { int values[4]; uninit_c_fill_unknown(&values[0]); return values[0]; }
int uninit_c_defined_ok(void) { int values[4]; uninit_c_fill_one(values); return values[0]; }
int uninit_c_full_ok(void) { int values[4] = {1, 2, 3, 4}; return values[2]; }
int uninit_c_memset_ok(void) { int values[4]; memset(values, 0, sizeof(values)); return values[2]; }
struct uninit_c_pair { int first; int second; };
struct uninit_c_wrapper { struct uninit_c_pair member; };
int uninit_c_wrapper_bad(void) {
  struct uninit_c_wrapper value; value.member.first = 7; return value.member.second;
}
int uninit_c_wrapper_ok(void) {
  struct uninit_c_wrapper value = {{7, 8}}; return value.member.second;
}
int uninit_c_wrapper_saved_memset_ok(void) {
  struct uninit_c_wrapper value; int* saved = &value.member.second;
  memset(&value, 0, sizeof(value)); return *saved;
}
