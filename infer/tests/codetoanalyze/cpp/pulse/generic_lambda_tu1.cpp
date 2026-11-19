/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "generic_lambda_header.h"

#if __cplusplus >= 201402L

// Translation unit 1 of two. Both include the same inline function, so both
// translate the same generic lambda at the same header location. Each TU's
// closure must bind its own captures; if the two were mixed up, or if either
// fell back to the capture-less closure, the write below would not reach.

// Both arms write, so `rank` is 10 or 20 and the guard is refuted.
int tu1_hdr_generic_lambda_write_reaches_ok(bool c) {
  unsigned rank = hdr_generic_lambda_write(c);
  int* p = nullptr;
  if (rank == 7) {
    return *p;
  }
  return 0;
}

// The mirror: the write reached, so this guard holds and the deref is reached.
// Without it a lost path would read as a pass above.
int tu1_hdr_generic_lambda_write_reaches_bad(bool c) {
  unsigned rank = hdr_generic_lambda_write(c);
  int* p = nullptr;
  if (rank != 7) {
    return *p;
  }
  return 0;
}

// Each arm keeps its own instantiation's value in this TU.
int tu1_hdr_first_arm_bad(bool c) {
  unsigned rank = hdr_generic_lambda_write(c);
  int* p = nullptr;
  if (rank == 10) {
    return *p;
  }
  return 0;
}

int tu1_hdr_second_arm_bad(bool c) {
  unsigned rank = hdr_generic_lambda_write(c);
  int* p = nullptr;
  if (rank == 20) {
    return *p;
  }
  return 0;
}

#endif // __cplusplus >= 201402L
