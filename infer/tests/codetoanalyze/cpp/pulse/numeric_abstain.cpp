/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdint>

namespace numeric_abstain {

// Representation limits in the solver must preserve executions. Each _bad
// function has a reachable null dereference; the small controls retain precision.

void chain_half_three_bad(double x) {
  double a1 = 0.5 * x;
  double a2 = 0.5 * a1;
  double a3 = 0.5 * a2;
  double b1 = 0.5 * x;
  double b2 = 0.5 * b1;
  double b3 = 0.5 * b2;
  int* p = nullptr;
  if (a3 == b3) {
    *p = 1;
  }
}

void chain_tenth_two_bad(double x) {
  double a1 = 0.1 * x;
  double a2 = 0.1 * a1;
  double b1 = 0.1 * x;
  double b2 = 0.1 * b1;
  int* p = nullptr;
  if (a2 == b2) {
    *p = 1;
  }
}

void chain_tenth_three_bad(double x) {
  double a1 = 0.1 * x;
  double a2 = 0.1 * a1;
  double a3 = 0.1 * a2;
  double b1 = 0.1 * x;
  double b2 = 0.1 * b1;
  double b3 = 0.1 * b2;
  int* p = nullptr;
  if (a3 == b3) {
    *p = 1;
  }
}

void chain_tenth_four_bad(double x) {
  double a1 = 0.1 * x;
  double a2 = 0.1 * a1;
  double a3 = 0.1 * a2;
  double a4 = 0.1 * a3;
  double b1 = 0.1 * x;
  double b2 = 0.1 * b1;
  double b3 = 0.1 * b2;
  double b4 = 0.1 * b3;
  int* p = nullptr;
  if (a4 == b4) {
    *p = 1;
  }
}

void small_negation_bad() {
  double y = -1e3;
  int* p = nullptr;
  if (y < 0) {
    *p = 1;
  }
}

void large_positive_bad() {
  double y = 1e300;
  int* p = nullptr;
  if (y > 0) {
    *p = 1;
  }
}

void large_negation_bad() {
  double y = -1e300;
  int* p = nullptr;
  if (y < 0) {
    *p = 1;
  }
}

void large_multiplication_bad() {
  double y = 2.0 * 1e300;
  int* p = nullptr;
  if (y > 0) {
    *p = 1;
  }
}

void uint64_max_and_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x & 1ULL;
  int* p = nullptr;
  if (y == 1ULL) {
    *p = 1;
  }
}

void uint64_max_and_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x & 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_max_or_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x | 1ULL;
  int* p = nullptr;
  if (y == 18446744073709551615ULL) {
    *p = 1;
  }
}

void uint64_max_or_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x | 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_max_xor_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x ^ 1ULL;
  int* p = nullptr;
  if (y == 18446744073709551614ULL) {
    *p = 1;
  }
}

void uint64_max_xor_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x ^ 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_max_not_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = ~x;
  int* p = nullptr;
  if (y == 0ULL) {
    *p = 1;
  }
}

void uint64_max_not_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = ~x;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_max_shift_left_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x << 1;
  int* p = nullptr;
  if (y == 18446744073709551614ULL) {
    *p = 1;
  }
}

void uint64_max_shift_left_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x << 1;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_max_shift_right_true_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x >> 1;
  int* p = nullptr;
  if (y == 9223372036854775807ULL) {
    *p = 1;
  }
}

void uint64_max_shift_right_reach_bad() {
  uint64_t x = 18446744073709551615ULL;
  uint64_t y = x >> 1;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_and_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x & 1ULL;
  int* p = nullptr;
  if (y == 0ULL) {
    *p = 1;
  }
}

void uint64_top_bit_and_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x & 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_or_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x | 1ULL;
  int* p = nullptr;
  if (y == 9223372036854775809ULL) {
    *p = 1;
  }
}

void uint64_top_bit_or_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x | 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_xor_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x ^ 1ULL;
  int* p = nullptr;
  if (y == 9223372036854775809ULL) {
    *p = 1;
  }
}

void uint64_top_bit_xor_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x ^ 1ULL;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_not_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = ~x;
  int* p = nullptr;
  if (y == 9223372036854775807ULL) {
    *p = 1;
  }
}

void uint64_top_bit_not_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = ~x;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_shift_left_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x << 1;
  int* p = nullptr;
  if (y == 0ULL) {
    *p = 1;
  }
}

void uint64_top_bit_shift_left_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x << 1;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void uint64_top_bit_shift_right_true_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x >> 1;
  int* p = nullptr;
  if (y == 4611686018427387904ULL) {
    *p = 1;
  }
}

void uint64_top_bit_shift_right_reach_bad() {
  uint64_t x = 9223372036854775808ULL;
  uint64_t y = x >> 1;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void int32_negative_one_uint64_bitwise_true_bad() {
  int32_t x = -1;
  uint64_t mask = 1;
  uint64_t y = x & mask;
  int* p = nullptr;
  if (y == 1) {
    *p = 1;
  }
}

void int32_negative_one_uint64_bitwise_reach_bad() {
  int32_t x = -1;
  uint64_t mask = 1;
  uint64_t y = x & mask;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void int32_negative_min_uint64_bitwise_true_bad() {
  int32_t x = -2147483648;
  uint64_t mask = 1;
  uint64_t y = x & mask;
  int* p = nullptr;
  if (y == 0) {
    *p = 1;
  }
}

void int32_negative_min_uint64_bitwise_reach_bad() {
  int32_t x = -2147483648;
  uint64_t mask = 1;
  uint64_t y = x & mask;
  int* p = nullptr;
  (void)y;
  *p = 1;
}

void small_bitwise_bad() {
  uint64_t x = 1234567;
  int* p = nullptr;
  if ((x & 1ULL) == 1) {
    *p = 1;
  }
}

void small_bitwise_false_ok() {
  uint64_t x = 1234567;
  int* p = nullptr;
  if ((x & 1ULL) == 0) {
    *p = 1;
  }
}

void small_shift_bad() {
  uint64_t x = 64;
  int* p = nullptr;
  if ((x >> 1) == 32) {
    *p = 1;
  }
}

void small_shift_false_ok() {
  uint64_t x = 64;
  int* p = nullptr;
  if ((x >> 1) == 33) {
    *p = 1;
  }
}

void chain_half_distinct_ok(double x) {
  double a = 0.5 * x;
  double b = 0.5 * x;
  int* p = nullptr;
  if (a != b) {
    *p = 1;
  }
}

void large_negation_safe_pointer_ok(int* p) {
  double y = -1e300;
  if (p && y < 0) {
    *p = 1;
  }
}

} // namespace numeric_abstain
