/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>
#include <limits.h>

int conversion_choose(void);

void integer_conversion_common_unsigned_bad(void) {
  int32_t x = conversion_choose() ? -2 : -2;
  uint64_t y = 1;
  /* Only conversion wraps here: UINT64_MAX - 1 + 1 fits uint64_t. */
  if (x + y == UINT64_MAX) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_common_unsigned_ok(void) {
  int32_t x = conversion_choose() ? -2 : -2;
  uint64_t y = 1;
  if (x + y < y) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_common_signed_bad(void) {
  int64_t x = conversion_choose() ? -2 : -2;
  uint32_t y = 1;
  /* int64_t represents every uint32_t: both operands convert to int64_t. */
  if (x + y == -1) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_common_signed_ok(void) {
  int64_t x = conversion_choose() ? -2 : -2;
  uint32_t y = 1;
  if (x + y == UINT32_MAX) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_same_wrap_segment_bad(void) {
  int x = conversion_choose() ? 256 : 300;
  uint8_t value = (uint8_t)x;
  if (value == 44) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_same_wrap_segment_ok(void) {
  int x = conversion_choose() ? 256 : 300;
  uint8_t value = (uint8_t)x;
  int a[50] = {0};
  a[value] = 1;
}

void integer_conversion_bool_nonzero_bad(void) {
  int x = conversion_choose() ? 2 : 2;
  _Bool value = (_Bool)x;
  if (value == 1) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_bool_negative_bad(void) {
  int x = conversion_choose() ? -2 : -2;
  _Bool value = (_Bool)x;
  if (value == 1) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_bool_zero_ok(void) {
  int x = conversion_choose() ? 0 : 0;
  _Bool value = (_Bool)x;
  if (value) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_unsigned_to_signed_bad(void) {
  uint32_t x = conversion_choose() ? UINT32_C(2147483648) : UINT32_C(2147483648);
  /* This fixture uses the LLVM/Clang two's-complement conversion contract. */
  int32_t value = (int32_t)x;
  if (value == INT32_MIN) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_unsigned_to_signed_ok(void) {
  uint32_t x = conversion_choose() ? UINT32_C(2147483648) : UINT32_C(2147483648);
  int32_t value = (int32_t)x;
  if (value > 0) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_wide_bad(void) {
  unsigned __int128 x = ((unsigned __int128)1 << 64) + 3;
  uint64_t value = (uint64_t)x;
  if (value == 3) {
    int a[1] = {0};
    a[1] = 1;
  }
}

void integer_conversion_wide_ok(void) {
  unsigned __int128 x = ((unsigned __int128)1 << 64) + 3;
  uint64_t value = (uint64_t)x;
  if (value == 4) {
    int a[1] = {0};
    a[1] = 1;
  }
}

/* Diagnostic controls. The existing INTEGER_OVERFLOW check must still inspect
   the arithmetic before the result is narrowed. The signed operation is UB and
   is a static-analysis fixture, not an executable conversion oracle. */
uint8_t integer_overflow_before_signed_conversion_bad(int x) {
  if (x != INT_MAX) {
    return 0;
  }
  return (uint8_t)(x + 1);
}

uint8_t integer_overflow_before_unsigned_conversion_bad(uint32_t x) {
  if (x != UINT32_MAX) {
    return 0;
  }
  return (uint8_t)(x + 1);
}

uint8_t integer_overflow_conversion_control_ok(int x) {
  if (x != 255) {
    return 0;
  }
  return (uint8_t)(x + 1);
}
