/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Declared here and defined only in the capture that did not finish, so
// analysing this file reaches a procedure attributed to a file that was never
// recorded.
int defined_in_the_unfinished_capture(int x);

int call_into_the_unfinished_capture_bad() {
  // the call comes first, because a path that stopped at the bug would never
  // reach the callee
  int result = defined_in_the_unfinished_capture(1);
  int* p = nullptr;
  return result + *p;
}
