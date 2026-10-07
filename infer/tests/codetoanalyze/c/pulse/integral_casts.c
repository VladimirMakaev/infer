/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stddef.h>
#include <stdint.h>

// the operand of a cast is read with its own type, not with the type of the cast

void use_byte(unsigned char b);

void byte_argument_then_negative_bad(int c) {
  use_byte((unsigned char)c);
  if (c < 0) {
    int* p = NULL;
    *p = 42;
  }
}

// values known to be constants are converted to the type of the cast

void known_constant_cast_wraps_bad() {
  uint32_t u = 0xffffffffu;
  int32_t r = (int32_t)u;
  if (r == -1) {
    int* p = NULL;
    *p = 42;
  }
}

// results of calls are in the range of their type

int8_t get_int8(void);

void call_result_in_range_ok() {
  int x = get_int8();
  if (x > 127) {
    int* p = NULL;
    *p = 42;
  }
}

void call_result_negative_bad() {
  int x = get_int8();
  if (x < 0) {
    int* p = NULL;
    *p = 42;
  }
}

// casts of non-constant values are the identity: [r] is [u], which is not negative
// (this was reported only because [u] was read as an [int32_t])
void FN_unsigned_to_signed_negative_bad(uint32_t u) {
  int32_t r = (int32_t)u;
  if (r < 0) {
    int* p = NULL;
    *p = 42;
  }
}

// comparisons of values converted to [unsigned] are unsigned: a negative value fails the check

int unsigned_bounds_check_ok(int i) {
  if ((unsigned)i >= 10u) {
    return 0;
  }
  if (i < 0) {
    int* p = NULL;
    *p = 42;
  }
  return 1;
}

void unsigned_bounds_check_negative_bad(int i) {
  if ((unsigned)i >= 10u) {
    if (i < 0) {
      int* p = NULL;
      *p = 42;
    }
  }
}

void variable_bound_check_ok(int i, int n) {
  if ((unsigned)i < (unsigned)n) {
    if (i < 0) {
      int* p = NULL;
      *p = 42;
    }
  }
}

void large_constant_bound_negative_bad(int i) {
  if ((unsigned)i < 0xfffffff0u) {
    if (i < 0) {
      int* p = NULL;
      *p = 42;
    }
  }
}

void implicit_unsigned_comparison_ok(int i) {
  size_t n = 16;
  if (i < n) {
    if (i < 0) {
      int* p = NULL;
      *p = 42;
    }
  }
}

// narrower operands are compared as [int]
void byte_comparison_then_negative_bad(int c) {
  if ((unsigned char)c < 10) {
    if (c < 0) {
      int* p = NULL;
      *p = 42;
    }
  }
}

// the other operand of the comparison is assumed to be below 2^31 unless it is a constant: a
// negative [n] converts to a value above any non-negative [i]
void FN_negative_bound_bad(int i, int n) {
  if (i >= 0 && n < 0 && (unsigned)i < (unsigned)n) {
    int* p = NULL;
    *p = 42;
  }
}
