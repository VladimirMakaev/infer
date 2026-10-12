/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

namespace volatile_stores {

// Each volatile write is observable, including initialization and values
// overwritten before a source-level read.
void overwritten_ok() {
  volatile int value = 7;
  value = 9;
}

void unused_ok() { volatile int value = 7; }

int read_ok() {
  volatile int value = 7;
  value = 9;
  return value;
}

void const_volatile_ok() { const volatile int value = 7; }

using VolatileInt = volatile int;
void typedef_ok() {
  VolatileInt value = 7;
  value = 9;
}

void pointer_is_volatile_ok(int* first, int* second) {
  int* volatile pointer = first;
  pointer = second;
}

void parameter_ok(volatile int value) { value = 7; }

void floating_ok() {
  volatile float value = 1.25f;
  value = 2.5f;
}

void plain_unused_bad() {
  int value = 7;
  value = 9;
}

int plain_overwritten_bad() {
  int value = 7;
  value = 9;
  return value;
}

void plain_parameter_bad(int value) { value = 7; }

// The pointer itself is plain; the pointee qualifier cannot protect its
// overwritten binding from a dead-store report.
volatile int* pointer_to_volatile_bad(volatile int* first,
                                      volatile int* second) {
  volatile int* pointer = first;
  pointer = second;
  return pointer;
}

// The load is observable, but the plain local's stored value is unused.
int plain_from_volatile_bad(volatile int* source) {
  int value = *source;
  value = 9;
  return value;
}

int plain_before_volatile_view_bad() {
  int value = 7;
  static_cast<volatile int&>(value) = 9;
  return 0;
}

} // namespace volatile_stores
