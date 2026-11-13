/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/* The C half of the OpenMP region cases. The frontend path is the same as for C++, but the
 * suite options are not, so a regression that only showed up in C would otherwise go unseen. */

#include <stdlib.h>

void use(int);
void use_ptr(int* p);

/* a bug inside a region is reported at all */
void c_region_null_deref_bad(void) {
#pragma omp parallel
  {
    int* p = NULL;
    *p = 1;
  }
}

/* private: a fresh, uninitialised copy, and the write inside does not escape */
void c_private_write_does_not_escape_bad(void) {
  int x;
#pragma omp parallel private(x)
  { x = 1; }
  use(x);
}

/* private on a pointer: the allocation goes into the copy, so nothing outside owns it */
void c_private_pointer_leaks_bad(void) {
  int* p = NULL;
#pragma omp parallel private(p)
  { p = (int*)malloc(sizeof(int)); }
  use_ptr(p);
}

/* firstprivate: the copy reads the original, so the assignment before the region is live */
void c_firstprivate_initialiser_is_live_ok(void) {
  int x = 7;
#pragma omp parallel firstprivate(x)
  { x = 1; }
  use(x);
}

/* firstprivate read inside the region: uninitialised if the copy-in were dropped */
void c_firstprivate_read_inside_ok(void) {
  int x = 7;
#pragma omp parallel firstprivate(x)
  { use(x); }
}

/* shared: the region acts on the original, so the write escapes */
void c_shared_write_escapes_ok(void) {
  int x;
#pragma omp parallel shared(x)
  { x = 1; }
  use(x);
}

/* lastprivate: the last iteration's value is copied out, so the write escapes */
void c_lastprivate_copies_out_ok(void) {
  int x;
#pragma omp parallel for lastprivate(x)
  for (int i = 0; i < 4; i++) {
    x = i;
  }
  use(x);
}

/* reduction: the partial results are combined back into the original */
void c_reduction_combines_out_ok(void) {
  int sum = 0;
#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < 4; i++) {
    sum += i;
  }
  use(sum);
}

/* no pragma at all: the control that says the file is being analysed */
void c_no_pragma_null_deref_bad(void) {
  int* p = NULL;
  *p = 1;
}
