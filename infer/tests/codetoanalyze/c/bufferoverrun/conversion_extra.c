/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdint.h>
int nondet_a(void);
void implicit_unsigned_conversion_bad(void) {
  int32_t x = nondet_a() ? -2 : -2;
  uint32_t y = 1;
  if (x > y) { int a[1]; a[1] = 1; }
}
void implicit_unsigned_conversion_ok(void) {
  int32_t x = nondet_a() ? -2 : -2;
  uint32_t y = 1;
  if (x < y) { int a[1]; a[1] = 1; }
}
void implicit_narrowing_bad(void) {
  int64_t x = nondet_a() ? 300 : 300;
  uint8_t value = x;
  if (value == 44) { int a[1]; a[1] = 1; }
}
void boolean_cast_bad(void) {
  int value = 2;
  _Bool converted = (_Bool)value;
  if (converted) { int a[1]; a[1] = 1; }
}
void converted_value_keeps_source_bad(void) {
  int64_t source = nondet_a() ? 300 : 300;
  uint8_t converted = (uint8_t)source;
  if (source == 300 && converted == 44) { int a[1]; a[1] = 1; }
}
void value_preserving_guard_ok(int index, uint32_t size) {
  if (index < 0 || index > 100 || size < 1 || size > 100) return;
  int a[size];
  if ((uint32_t)index < size) a[index] = 1;
}
/* Known precision limit: the converted values are {0,255}. The selected convex
   hull [0,255] admits impossible 128, so the array access may be reported. This
   documents the hull trade-off; neither a two-piece interval nor a cast relation
   is part of this change. */
void FP_wrap_boundary_precision_ok(int choose) {
  int x = choose ? 255 : 256;
  uint8_t value = (uint8_t)x;
  if (value == 128) { int a[1]; a[1] = 1; }
}
