/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

// integers

unsigned int returnUnsigned();

void nonnegative_int_ok() {
  unsigned int x = returnUnsigned();
  if (x < 0) {
    // unreachable
    int* p = NULL;
    *p = 42;
  }
}

int returnSigned();

void signed_int_bad() {
  int x = returnSigned();
  if (x < 0) {
    // reachable
    int* p = NULL;
    *p = 42;
  }
}

// pointers to integers

unsigned int* returnUnsignedPointer();

void nonnegative_int_ptr_ok() {
  unsigned int* x = returnUnsignedPointer();
  if (*x < 0) {
    // unreachable
    int* p = NULL;
    *p = 42;
  }
}

int* returnSignedPointer();

void signed_int_ptr_bad() {
  int* x = returnSigned();
  if (*x < 0) {
    // reachable
    int* p = NULL;
    *p = 42;
  }
}

// struct with integer fields

struct foo {
  unsigned int unsigned_int;
  int signed_int;
};

struct foo* returnFoo();

void nonnegative_field_ok() {
  struct foo* x = returnFoo();
  if (x->unsigned_int < 0) {
    // unreachable
    int* p = NULL;
    *p = 42;
  }
}

void signed_field_bad() {
  struct foo* x = returnFoo();
  if (x->signed_int < 0) {
    // reachable
    int* p = NULL;
    *p = 42;
  }
}

// array of integers

unsigned int* returnUnsignedArray();

int nonnegative_array_ok() {
  unsigned int* a = returnUnsignedArray();
  if (a[0] < 0) {
    // unreachable
    int* p = NULL;
    *p = 42;
  }
}

int* returnSignedArray();

int signed_array_bad() {
  int* a = returnSignedArray();
  if (a[0] < 0) {
    // reachable
    int* p = NULL;
    *p = 42;
  }
}

// constants that do not fit their type do not make the path infeasible

void minus_one_to_unsigned_bad() {
  unsigned int x = -1;
  int* p = NULL;
  *p = x;
}

void narrowing_constant_bad() {
  short s = 0xFFFF;
  int* p = NULL;
  *p = s;
}

void unsigned_wraparound_constant_bad() {
  unsigned int a = 0;
  unsigned int b = a - 1;
  int* p = NULL;
  *p = b;
}

int unsigned_wraparound_symbolic_bad(unsigned int a) {
  unsigned int b = a - 1;
  if (a == 0) {
    int* p = NULL;
    *p = 42;
  }
  return b;
}

struct counter {
  unsigned int n;
};

void unset_sentinel_field_bad() {
  struct counter c = {(unsigned int)-1};
  if (c.n == (unsigned int)-1) {
    int* p = NULL;
    *p = 42;
  }
}

int unsigned_to_signed(unsigned int u) {
  int s = (int)u;
  if (u > 0x7fffffffu) {
    int* p = NULL;
    *p = s;
  }
  return 0;
}

// the cast is the identity: [s] is [u], which does not fit in an int when
// [u > INT_MAX], so the path is pruned
void FN_unsigned_to_signed_bad() { unsigned_to_signed(0xffffffffu); }

// [n] is -1, not wrapped to 0xFFFFFFFF, so the comparison is false
int FP_minus_one_equals_max_ok() {
  unsigned int n = -1;
  if (n == 0xFFFFFFFFu) {
    return 0;
  }
  int* p = NULL;
  return *p;
}
