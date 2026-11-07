/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "boxes.h"

// this unit provides pong<char>; ping<char> comes from the unit compiled with default signedness
template void pong<char>(Box<char>*);

void release_from_pong_unit(Box<char>* b) {
  if (b) {
    pong(b);
  }
}

int deref_in_pong_unit_bad() {
  int* p = nullptr;
  return *p;
}

int deref_in_pong_unit_ok() {
  int x = 0;
  int* p = &x;
  return *p;
}
