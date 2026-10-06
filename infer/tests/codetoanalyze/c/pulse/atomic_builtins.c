/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stddef.h>

void test_and_set_clear_flag_bad() {
  unsigned char flag = 0;
  int* p = NULL;
  if (!__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST)) {
    *p = 42;
  }
}

void test_and_set_twice_ok() {
  unsigned char flag = 0;
  int* p = NULL;
  __atomic_test_and_set(&flag, __ATOMIC_SEQ_CST);
  if (!__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST)) {
    *p = 42;
  }
}

void clear_then_test_and_set_bad() {
  unsigned char flag = 1;
  int* p = NULL;
  __atomic_clear(&flag, __ATOMIC_SEQ_CST);
  if (!__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST)) {
    *p = 42;
  }
}

void c11_fetch_nand_returns_old_value_ok() {
  _Atomic int a = 6;
  int* p = NULL;
  if (__c11_atomic_fetch_nand(&a, 3, __ATOMIC_SEQ_CST) != 6) {
    *p = 42;
  }
}

void c11_fetch_nand_stores_nand_ok() {
  _Atomic int a = 6;
  int* p = NULL;
  __c11_atomic_fetch_nand(&a, 3, __ATOMIC_SEQ_CST);
  if (a != ~(6 & 3)) {
    *p = 42;
  }
}

void c11_fetch_nand_stores_nand_bad() {
  _Atomic int a = 6;
  int* p = NULL;
  __c11_atomic_fetch_nand(&a, 3, __ATOMIC_SEQ_CST);
  if (a == ~(6 & 3)) {
    *p = 42;
  }
}

void scoped_fetch_add_ok() {
  int x = 1;
  int* p = NULL;
  int old =
      __scoped_atomic_fetch_add(&x, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM);
  if (old != 1 || x != 2) {
    *p = 42;
  }
}

void scoped_add_fetch_bad() {
  int x = 1;
  int* p = NULL;
  if (__scoped_atomic_add_fetch(
          &x, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM) == 2) {
    *p = 42;
  }
}
