/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>
int nondet_a(void);
int nondet_b(void);
/* Each function has an unconditional overrun. Integer conversion and comparison
   must not erase the feasible path before that access. */

/* N1 the suspect: negative signed vs unsigned, compared. */
int conversion_n1_neg_vs_unsigned_cmp_bad(void) {
  int32_t x = nondet_a()?-1:-1; uint32_t y = nondet_b()?1:1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N2 same types, NON-negative signed operand. */
int conversion_n2_nonneg_vs_unsigned_cmp_bad(void) {
  int32_t x = nondet_a()?1:1; uint32_t y = nondet_b()?1:1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N3 both signed, negative operand. */
int conversion_n3_neg_vs_signed_cmp_bad(void) {
  int32_t x = nondet_a()?-1:-1; int32_t y = nondet_b()?1:1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N4 the negative int and the unsigned both exist but are NEVER COMPARED. */
int conversion_n4_neg_and_unsigned_nocmp_bad(void) {
  int32_t x = nondet_a()?-1:-1; uint32_t y = nondet_b()?1:1;
  int v = (int)y + (int)x;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N5 compared, but the result is discarded. */
int conversion_n5_cmp_result_unused_bad(void) {
  int32_t x = nondet_a()?-1:-1; uint32_t y = nondet_b()?1:1;
  (void)(x < y);
  int a[1] = {0};  a[1] = 1;
  return a[0];
}
/* N6 the comparison drives a BRANCH rather than a value. */
int conversion_n6_cmp_in_branch_bad(void) {
  int32_t x = nondet_a()?-1:-1; uint32_t y = nondet_b()?1:1;
  int a[1] = {0};  if (x < y) { a[1] = 1; } else { a[1] = 1; }
  return a[0];
}
/* N7 64-bit flavour: negative int64 vs uint64. */
int conversion_n7_neg_vs_u64_cmp_bad(void) {
  int64_t x = nondet_a()?-1:-1; uint64_t y = nondet_b()?1:1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N8 explicit cast making the conversion visible in the source. */
int conversion_n8_explicit_cast_cmp_bad(void) {
  int32_t x = nondet_a()?-1:-1; uint32_t y = nondet_b()?1:1;
  int v = ((uint32_t)x < y) ? 1 : 0;
  int a[1] = {0};  a[1] = 1;
  return a[0] + v;
}
/* N9 BASELINE: no mixed types anywhere. Must report in both engines. */
int conversion_n9_baseline_bad(void) {
  int32_t x = nondet_a()?-1:-1;
  int a[1] = {0};  a[1] = 1;
  return a[0] + x;
}
