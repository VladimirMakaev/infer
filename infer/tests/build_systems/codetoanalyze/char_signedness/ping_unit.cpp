/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "boxes.h"

// this unit provides ping<char>; pong<char> comes from the unit compiled with -funsigned-char
template void ping<char>(Box<char>*);

void release_from_ping_unit(Box<char>* b) {
  if (b) {
    ping(b);
  }
}

int deref_in_ping_unit_bad() {
  int* p = nullptr;
  return *p;
}

int deref_in_ping_unit_ok() {
  int x = 0;
  int* p = &x;
  return *p;
}
