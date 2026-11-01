/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdlib.h>

struct point {
  int x;
  int y;
};

int return_zero() { return ((struct point){.y = 32, .x = 0}).x; }

int detect_zero_bad() {
  if (return_zero() == 0) {
    int* p = NULL;
    *p = 42;
  }
}

int detect_zero_ok() {
  if (return_zero() != 0) {
    int* p = NULL;
    *p = 42;
  }
}

// in C a compound literal is an lvalue with automatic storage, so its address
// escaping is a real one
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"
int* return_address_of_compound_literal_bad() { return &(int){1}; }
#pragma clang diagnostic pop

int value_of_compound_literal_ok() { return (int){1} << 0; }
