/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

// integral conversions of constants that do not fit the target type

void minus_one_to_unsigned_char_ok() {
  unsigned char c = -1;
  if (c != 255) {
    int* p = NULL;
    *p = 42;
  }
}

void minus_one_to_unsigned_char_bad() {
  unsigned char c = -1;
  if (c == 255) {
    int* p = NULL;
    *p = 42;
  }
}

void minus_one_to_unsigned_is_positive_ok() {
  unsigned int u = -1;
  if (u < 1) {
    int* p = NULL;
    *p = 42;
  }
}

void unsigned_negation_widened_ok() {
  unsigned long long l = -1u;
  if (l != 0xffffffffull) {
    int* p = NULL;
    *p = 42;
  }
}

void narrowing_to_short_ok() {
  short s = 0x18000;
  if (s != -32768) {
    int* p = NULL;
    *p = 42;
  }
}

void unsigned_to_int_ok() {
  int i = 0xffffffffu;
  if (i != -1) {
    int* p = NULL;
    *p = 42;
  }
}
