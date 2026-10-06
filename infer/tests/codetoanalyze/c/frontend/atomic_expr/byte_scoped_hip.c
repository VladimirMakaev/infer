/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdbool.h>

void byte_builtins() {
  unsigned char flag;
  bool b;
  b = __atomic_test_and_set(&flag, __ATOMIC_SEQ_CST);
  __atomic_clear(&flag, __ATOMIC_SEQ_CST);
}

void c11_nand() {
  int _a;
  _Atomic int a;
  _a = __c11_atomic_fetch_nand(&a, 1, __ATOMIC_SEQ_CST);
}

void scoped_builtins() {
  int _i;
  int i = 0;
  _i = __scoped_atomic_fetch_add(&i, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM);
  _i = __scoped_atomic_sub_fetch(&i, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM);
  _i = __scoped_atomic_fetch_or(&i, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM);
  _i = __scoped_atomic_and_fetch(&i, 1, __ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM);
}

void hip_builtins(int* expected) {
  int _i;
  int i = 0;
  _i = __hip_atomic_fetch_add(&i, 1, __ATOMIC_SEQ_CST, 1);
  __hip_atomic_store(&i, 2, __ATOMIC_SEQ_CST, 1);
  _i = __hip_atomic_load(&i, __ATOMIC_SEQ_CST, 1);
  _i = __hip_atomic_exchange(&i, 3, __ATOMIC_SEQ_CST, 1);
  _i = __hip_atomic_compare_exchange_strong(
      &i, expected, 4, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST, 1);
  _i = __hip_atomic_fetch_max(&i, 5, __ATOMIC_SEQ_CST, 1);
}
