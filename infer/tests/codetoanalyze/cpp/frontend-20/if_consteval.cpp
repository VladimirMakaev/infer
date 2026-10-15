/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// A consteval if has no condition and only one of its branches runs outside
// constant evaluation, so only that branch is translated. (A C++23 construct,
// accepted here as an extension.)

int consteval_takes_else(int x) {
  if consteval {
    return x + 111;
  } else {
    return x + 222;
  }
}

int consteval_negated_takes_then(int x) {
  if !consteval {
    return x + 333;
  } else {
    return x + 444;
  }
}

int consteval_without_else(int x) {
  int r = x + 555;
  if consteval {
    r = x + 666;
  }
  return r;
}
