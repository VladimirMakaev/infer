/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>
#include <stddef.h>
int nondet_a(void);
int nondet_b(void);

/* Calibrating the discriminator IN CONVERSION SHAPES. The same-type calibration does
   not automatically carry over: casts, promotions and mixed-signedness comparisons are
   different translation and different modelling. */

/* k1 unconditional defect inside a cast expression: must report. */
int conversion_k1_reach_bad(void) {
  int64_t x = nondet_a() ? 300 : 300;
  int8_t v = (int8_t)x;
  int a[1] = {0};
  a[1] = 1;  return (int)v;
}
/* k2 achievable guard after a NARROWING cast. (int8_t)300 is 44. Must report. */
int conversion_k2_narrow_achievable_bad(void) {
  int64_t x = nondet_a() ? 300 : 300;
  int8_t v = (int8_t)x;
  int a[1] = {0};
  if (v == 44) { a[1] = 1; }
  return 0;
}
/* k3 unachievable guard after the same cast: expect SILENT (the engine prunes). */
int conversion_k3_narrow_unachievable_ok(void) {
  int64_t x = nondet_a() ? 300 : 300;
  int8_t v = (int8_t)x;
  int a[1] = {0};
  if (v == 45) { a[1] = 1; }
  return 0;
}
/* k4 the UNTRUNCATED value: 300 does not fit int8_t, so this is FALSE at run time.
   A report here would mean the engine ignores the cast and keeps the wide value --
   which is the opposite failure from k2 and worth telling apart. */
int conversion_k4_narrow_untruncated_ok(void) {
  int64_t x = nondet_a() ? 300 : 300;
  int8_t v = (int8_t)x;
  int a[1] = {0};
  if (v == 300) { a[1] = 1; }
  return 0;
}
/* k5 sign EXTENSION: (int64_t)(int8_t)-1 is -1. Must report. */
int conversion_k5_sign_extend_bad(void) {
  int8_t x = nondet_a() ? -1 : -1;
  int64_t v = (int64_t)x;
  int a[1] = {0};
  if (v == -1LL) { a[1] = 1; }
  return 0;
}
/* k6 ZERO extension: (int32_t)(uint8_t)255 is 255, not -1. Must report. */
int conversion_k6_zero_extend_bad(void) {
  uint8_t x = nondet_a() ? 255 : 255;
  int32_t v = (int32_t)x;
  int a[1] = {0};
  if (v == 255) { a[1] = 1; }
  return 0;
}
/* k7 the classic mixed-signedness comparison: -1 < 1u is FALSE in C, because -1
   converts to UINT_MAX. v is therefore 0, and this must report. */
int conversion_k7_mixed_compare_bad(void) {
  int32_t x = nondet_a() ? -1 : -1;
  uint32_t y = nondet_b() ? 1 : 1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};
  if (v == 0) { a[1] = 1; }
  return 0;
}
/* k8 the trap answer for k7: if the engine compares mathematically it gets 1.
   FALSE at run time, so a report here means the engine ignored the conversion. */
int conversion_k8_mixed_compare_wrong_ok(void) {
  int32_t x = nondet_a() ? -1 : -1;
  uint32_t y = nondet_b() ? 1 : 1;
  int v = (x < y) ? 1 : 0;
  int a[1] = {0};
  if (v == 1) { a[1] = 1; }
  return 0;
}
/* k9 integer PROMOTION: (int8_t)100 + (int8_t)100 is 200 as int, not -56, because
   both operands promote to int before the addition. Must report. */
int conversion_k9_promotion_bad(void) {
  int8_t x = nondet_a() ? 100 : 100;
  int8_t y = nondet_b() ? 100 : 100;
  int v = x + y;
  int a[1] = {0};
  if (v == 200) { a[1] = 1; }
  return 0;
}
