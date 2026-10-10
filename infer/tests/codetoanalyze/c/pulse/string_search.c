/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>
#include <string.h>

// Declare the GNU API so this model test also compiles with non-GNU C libraries.
void* memrchr(const void*, int, size_t);

int strchr_zero_ok(const char* text) { return *strchr(text, 0); }

int strchr_empty_ok(void) { return *strchr("", '\0'); }

int strchr_literal_ok(void) { return *strchr("abc", 0); }

int strchr_buffer_ok(void) {
  char text[] = "abc";
  *strchr(text, '\0') = '\0';
  return text[0];
}

int strchr_guarded_zero_ok(int needle) {
  if (needle == 0)
    return *strchr("abc", needle);
  return 0;
}

int strchr_computed_zero_ok(void) {
  int needle = 1;
  return *strchr("abc", needle - 1);
}

int strchr_allocated_ok(void) {
  char* text = strdup("abc");
  if (!text)
    return 0;
  *strchr(text, 0) = 0;
  free(text);
  return 0;
}

int strchr_zero_null_branch_ok(void) {
  if (strchr("abc", 0) == NULL) {
    int* value = NULL;
    return *value;
  }
  return 0;
}

int strrchr_zero_ok(void) { return *strrchr("abc", 0); }

int strchr_null_bad(void) { return *strchr(NULL, 0); }

int strrchr_null_bad(void) { return *strrchr(NULL, 0); }

int strchr_missing_bad(void) { return *strchr("abc", 'z'); }

int strrchr_missing_bad(void) { return *strrchr("abc", 'z'); }

int strchr_unknown_bad(int needle) { return *strchr("abc", needle); }

int strrchr_unknown_bad(int needle) { return *strrchr("abc", needle); }

int memchr_bounded_zero_bad(void) { return *(char*)memchr("abc", 0, 2); }

int memchr_empty_range_bad(void) { return *(char*)memchr("abc", 0, 0); }

int memrchr_bounded_zero_bad(void) { return *(char*)memrchr("abc", 0, 2); }
