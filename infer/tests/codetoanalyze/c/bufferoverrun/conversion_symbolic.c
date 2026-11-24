/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/* Symbolic casts deliberately retain the source value. For every negative int,
   conversion to unsigned makes a nonzero positive value. Keeping the negative
   symbolic value can wrongly prune this access: the known false-negative class. */
void FN_symbolic_unsigned_conversion_bad(int source) {
  if (source < 0) {
    unsigned converted = (unsigned)source;
    if (converted > 0u) {
      int a[1];
      a[1] = 1;
    }
  }
}

/* The same symbolic input and conversion must not skip an unconditional error. */
void symbolic_unsigned_conversion_reach_bad(int source) {
  if (source < 0) {
    unsigned converted = (unsigned)source;
    int a[1];
    a[1] = 1;
    (void)converted;
  }
}

/* No negative int converts to unsigned zero. */
void symbolic_unsigned_conversion_zero_ok(int source) {
  if (source < 0) {
    unsigned converted = (unsigned)source;
    if (converted == 0u) {
      int a[1];
      a[1] = 1;
    }
  }
}
