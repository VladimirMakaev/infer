/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <stdarg.h>
#include <stdlib.h>

// A value too big for a register, so a function returning one is given a return
// parameter: it is appended to the formals and its address is appended to the
// actuals, which puts it AFTER the arguments of `...`.
struct big {
  long a, b, c, d;
};

// Writes through its return parameter and does not touch `...`.
struct big store(int n, ...) {
  struct big result = {n, 0, 0, 0};
  return result;
}

// Reads the first argument of `...` and dereferences it.
struct big deref_first(int n, ...) {
  struct big result = {0, 0, 0, 0};
  va_list ap;
  va_start(ap, n);
  int* p = va_arg(ap, int*);
  result.a = *p;
  va_end(ap);
  return result;
}

// Trimming the actuals to a prefix would hand this integer zero to the return
// parameter, so writing the result would be reported as a null dereference.
struct big zero_first_vararg_ok() { return store(1, 0); }

struct big zero_later_vararg_ok() {
  int x = 7;
  return store(2, &x, 0);
}

// Binding the `...` arguments by dropping a prefix would count the return
// parameter's address as the first of them, so `va_arg` would read that instead
// of the null below and this true positive would be missed.
struct big va_arg_reads_first_vararg_bad() { return deref_first(1, NULL); }

struct big va_arg_reads_first_vararg_ok() {
  int x = 7;
  return deref_first(1, &x);
}

// A freed pointer passed in `...` must still be seen through va_arg.
struct big va_arg_reads_freed_vararg_bad() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    struct big empty = {0, 0, 0, 0};
    return empty;
  }
  free(p);
  return deref_first(1, p);
}
